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

static int cgfilesize(struct file * f)
{
    int size = 1;

    if (strcmp(f->cgfilename, "cgroup.procs") == 0) {
        int procoff = 0;
        while (procoff < (sizeof(f->cgp->proc) / sizeof(*f->cgp->proc))) {
            if (f->cgp->proc[procoff] == 0) {
                procoff++;
                continue;
            }
            int i = proc_pid(f->cgp->proc[procoff]);
            while (i != 0) {
                i /= 10;
                size++;
            }
            size++;
            procoff++;
        }
    } else if (strcmp(f->cgfilename, "cgroup.controllers") == 0) {
        if (f->cgp->cpu_controller_avalible)
            size += 3;
    } else if (strcmp(f->cgfilename, "cgroup.subtree_control") == 0) {
        if (f->cgp->cpu_controller_enabled)
            size += 3;
    } else if (strcmp(f->cgfilename, "cgroup.events") == 0) {
        size += strlen("populated - 0");
    } else if (strcmp(f->cgfilename, "cgroup.max.descendants") == 0) {
        size += strlen(f->cgp->max_descendants_value);
    } else if (strcmp(f->cgfilename, "cgroup.max.depth") == 0) {
        size += strlen(f->cgp->max_depth_value);
    } else if (strcmp(f->cgfilename, "cgroup.stat") == 0) {
        size += strlen("nr_descendants - ") +
                strlen(f->cgp->nr_descendants) + strlen("\n") +
                strlen("nr_dying_descendants - ") +
                strlen(f->cgp->nr_dying_descendants);
    }

    return size;
}

int unsafe_cgstat(struct file * f, struct stat * st)
{
    if (*f->cgp->cgroup_dir_path == 0)
        return -1;
    if (*f->cgfilename == 0) {
        st->type = T_CGDIR;
        st->size = MAX_CGROUP_FILE_NAME_LENGTH *
                   (7 + cgorup_num_of_immidiate_children(f->cgp));
    } else {
        st->type = T_CGFILE;
        st->size = cgfilesize(f);
    }
    return 0;
}
