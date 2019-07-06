#include "param.h"
#include "proc.h"

#ifndef XV6_CGROUP_H
#define XV6_CGROUP_H

/**
 * Control group, contains up to NPROC processes.
 */
struct cgroup
{
    struct proc * proc[NPROC];
    unsigned int cpu_time;
    unsigned int cpu_percent;
    unsigned int cpu_account_period;
    unsigned int cpu_time_limit;
    unsigned int cpu_account_frame;
};

/**
 * Control groups.
 */
extern struct cgroup cgroups[NPROC];

/**
 * Returns the root cgroup, &cgroups[0].
 */
struct cgroup * cgroup_root(void);

/**
 * Initialize a cgroup.
 */
void cgroup_initialize(struct cgroup * cgroup);

/**
 * Insert a process to the control group. Returns -1 if no space.
 * Erases the process from the other group.
 * Must be called with locked process table.
 */
int cgroup_insert(struct cgroup * cgroup, struct proc * proc);

/**
 * Insert a process to the control group.
 * Must be called with locked process table.
 */
void cgroup_erase(struct cgroup * cgroup, struct proc * proc);

#endif