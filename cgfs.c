#include "cgfs.h"
#include "defs.h"
#include "fcntl.h"
#include "file.h"
#include "fs.h"
#include "param.h"
#include "spinlock.h"
#include "types.h"

#define MAX_PID_LENGTH 5
#define MAX_CGROUP_DIR_ENTRIES 64


int unsafe_readcgdirectory(struct file * f, char * addr, int n)
{
    char buf[MAX_CGROUP_FILE_NAME_LENGTH * MAX_CGROUP_DIR_ENTRIES];

    if (*f->cgp->cgroup_dir_path == 0)
        return -1;

    for (int i = 0; i < sizeof(buf); i++)
        buf[i] = ' ';

    char * bufp = buf;

    strncpy(bufp, ".", strlen("."));
    bufp += MAX_CGROUP_FILE_NAME_LENGTH;
    if (f->cgp != cgroup_root()) {
        strncpy(bufp, "..", strlen(".."));
        bufp += MAX_CGROUP_FILE_NAME_LENGTH;
    }
    strncpy(bufp, "cgroup.procs", strlen("cgroup.procs"));
    bufp += MAX_CGROUP_FILE_NAME_LENGTH;
    if (f->cgp != cgroup_root()) {
        strncpy(bufp, "cgroup.controllers", strlen("cgroup.controllers"));
        bufp += MAX_CGROUP_FILE_NAME_LENGTH;
        strncpy(bufp,
                "cgroup.subtree_control",
                strlen("cgroup.subtree_control"));
        bufp += MAX_CGROUP_FILE_NAME_LENGTH;
        strncpy(bufp, "cgroup.events", strlen("cgroup.events"));
        bufp += MAX_CGROUP_FILE_NAME_LENGTH;
    }
    strncpy(
        bufp, "cgroup.max.descendants", strlen("cgroup.max.descendants"));
    bufp += MAX_CGROUP_FILE_NAME_LENGTH;
    strncpy(bufp, "cgroup.max.depth", strlen("cgroup.max.depth"));
    bufp += MAX_CGROUP_FILE_NAME_LENGTH;
    strncpy(bufp, "cgroup.stat", strlen("cgroup.stat"));
    bufp += MAX_CGROUP_FILE_NAME_LENGTH;

    get_cgroup_names_at_path(bufp, f->cgp->cgroup_dir_path);

    int i;
    for (i = f->off; i < sizeof(buf) && i - f->off < n; i++) {
        *addr++ = buf[i];
    }

    i = i - f->off;
    f->off += i;
    return i;
}
