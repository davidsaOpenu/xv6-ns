#include "cgroup.h"

struct cgroup cgroups[NPROC];

struct cgroup * cgroup_root(void)
{
    return &cgroups[0];
}

void cgroup_initialize(struct cgroup * cgroup)
{
    cgroup->cpu_account_frame = 0;
    cgroup->cpu_percent = 0;
    cgroup->cpu_time = 0;
    cgroup->cpu_time_limit = ~0;
    cgroup->cpu_account_period = 1 * 100 * 1000;
}

int cgroup_insert(struct cgroup * cgroup, struct proc * proc)
{
    // Whether a free slot was found.
    int found = 0;

    // Index of the free slot.
    unsigned int index = 0;

    // Find available slot.
    for (unsigned int i = 0; i < sizeof(cgroup->proc) / sizeof(*cgroup->proc); ++i) {
        // If not found yet, and the current entry is available, save the
        // index and indicate that an entry was found.
        if (!found && !cgroup->proc[i]) {
            index = i;
            found = 1;
            continue;
        }

        // If process was found, return success.
        if (proc == cgroup->proc[i]) {
            return 0;
        }
    }

    // If not found, return a failure.
    if (!found) {
        return -1;
    }

    // Erase the proc from the other cgroup.
    if (proc->cgroup) {
        cgroup_erase(proc->cgroup, proc);
    }

    // Associate the process with the cgroup.
    cgroup->proc[index] = proc;

    // Set the cgroup of the current process.
    proc->cgroup = cgroup;

    return 0;
}

void cgroup_erase(struct cgroup * cgroup, struct proc * proc)
{
    // Iterate all cgroup processes.
    for (unsigned int i = 0; i < sizeof(cgroup->proc) / sizeof(*cgroup->proc); ++i) {
        // If process was found, remove it from the cgroup.
        if (proc == cgroup->proc[i]) {
            proc->cgroup = 0;
            cgroup->proc[i] = 0;
            break;
        }
    }
}
