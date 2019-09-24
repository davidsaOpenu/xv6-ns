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

static int fdalloc(struct file * f)
{
    int fd;
    struct proc * curproc = myproc();

    for (fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd] == 0) {
            curproc->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int unsafe_opencgfile(char * filename, struct cgroup * cgp, int omode)
{
    struct file * f;
    int fd;
    char writable;

    /* Check that the file to be opened is one of the filesystem files and
     * set writeable accordingly.*/
    if (strcmp(filename, "cgroup.procs") == 0 ||
        strcmp(filename, "cgroup.subtree_control") == 0 ||
        strcmp(filename, "cgroup.max.descendants") == 0 ||
        strcmp(filename, "cgroup.max.depth") == 0)
        writable = 1;
    else if (strcmp(filename, "cgroup.controllers") == 0 ||
             strcmp(filename, "cgroup.events") == 0 ||
             strcmp(filename, "cgroup.stat") == 0)
        writable = 0;
    else
        return -1;

    /* Allocate file structure and file desctiptor.*/
    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        return -1;
    }

    f->type = FD_CG;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = ((omode & O_WRONLY) || (omode & O_RDWR)) && writable;
    f->cgp = cgp;
    strncpy(f->cgfilename, filename, sizeof(f->cgfilename));

    cgp->ref_count++;

    return fd;
}

