#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "mount.h"
#include "namespace.h"
#include "ns_types.h"
#include "pid_ns.h"

struct {
  struct spinlock lock;
  struct nsproxy nsproxy[NNAMESPACE];
} namespacetable;

void
namespaceinit(void)
{
    initlock(&namespacetable.lock, "namespace");
    mount_nsinit();
    pid_ns_init();
}

void
namespaceput(struct nsproxy* nsproxy)
{
    acquire(&namespacetable.lock);
    if (nsproxy->ref == 1) {
        release(&namespacetable.lock);
        mount_nsput(nsproxy->mount_ns);
        nsproxy->mount_ns = 0;
        pid_ns_put(nsproxy->pid_ns);
        nsproxy->pid_ns = 0;
        acquire(&namespacetable.lock);
    }
    nsproxy->ref -= 1;
    release(&namespacetable.lock);
}

struct nsproxy*
namespacedup(struct nsproxy* nsproxy)
{
    acquire(&namespacetable.lock);
    nsproxy->ref++;
    release(&namespacetable.lock);
    return nsproxy;
}

static struct nsproxy*
allocnsproxyinternal(void)
{
    for (int i = 0; i < NNAMESPACE; i++) {
        if (namespacetable.nsproxy[i].ref == 0) {
            namespacetable.nsproxy[i].ref++;
            return &namespacetable.nsproxy[i];
        }
    }

    // out of nsproxy objects
    return (void*)0;
}

struct nsproxy*
emptynsproxy(void)
{
    acquire(&namespacetable.lock);
    struct nsproxy* result = allocnsproxyinternal();
    if (result == (void*)0)
        return (void*)0;
    result->mount_ns = newmount_ns();
    result->pid_ns = pid_ns_new(0);
    release(&namespacetable.lock);

    return result;
}

struct nsproxy*
namespace_replace_pid_ns(struct nsproxy* oldns, struct pid_ns* pid_ns)
{
    struct nsproxy* nsproxy = allocnsproxyinternal();
    if (nsproxy == (void*)0)
        return (void*)0;
    nsproxy->mount_ns = mount_nsdup(oldns->mount_ns);
    nsproxy->pid_ns = pid_ns_dup(pid_ns);
    return nsproxy;
}

int
unshare(int nstype)
{
    acquire(&namespacetable.lock);
    if (myproc()->nsproxy->ref > 1) {
        struct nsproxy *oldns = myproc()->nsproxy;
        struct nsproxy *newns = allocnsproxyinternal();
        if (newns == (void*)0)
            return -1;
        myproc()->nsproxy = newns;
        myproc()->nsproxy->mount_ns = mount_nsdup(oldns->mount_ns);
        myproc()->nsproxy->pid_ns = pid_ns_dup(oldns->pid_ns);
        oldns->ref--;
    }
    release(&namespacetable.lock);
    switch(nstype) {
        case MOUNT_NS:
            {
                struct mount_ns* previous = myproc()->nsproxy->mount_ns;
                struct mount_ns* new = copymount_ns();
                if (new == (void*)0)
                    return -1;
                myproc()->nsproxy->mount_ns = new;
                mount_nsput(previous);
                return 0;
            }
        case PID_NS:
            {
              if (myproc()->child_pid_ns || pid_ns_is_max_depth(myproc()->nsproxy->pid_ns)) {
                return -1;
              }

              struct pid_ns* pid_ns = pid_ns_new(myproc()->nsproxy->pid_ns);
              if (pid_ns == 0)
                return -1;

              myproc()->child_pid_ns = pid_ns;
              return 0;
            }
        default:
            return -1;
    }
}
