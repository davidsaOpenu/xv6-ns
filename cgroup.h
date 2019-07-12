#include "param.h"
#include "proc.h"

#ifndef XV6_CGROUP_H
#define XV6_CGROUP_H

#define MAX_DECS_MAX_SIZE 3
#define MAX_DEPTH_MAX_SIZE 3
#define NR_DESC_MAX_SIZE 3
#define NR_DYING_DESC_MAX_SIZE 3

#define MAX_CONTROLLER_NAME_LENGTH 16

#define MAX_PATH_LENGTH 512

/**
 * Control group, contains up to NPROC processes.
 */
struct cgroup
{
	char cgroup_dir_path[MAX_PATH_LENGTH]; 	/* Path of the cgroup directory.*/
	
    struct proc * proc[NPROC]; 				/* Array of all processes in the cgroup.*/
	int num_of_procs; 						/* Number of processes in the cgroup subtree (including processes in this cgroup).*/
	
	struct cgroup *parent;					/* The parent cgroup.*/
	
	char cpu_controller_avalible;			/* Is 1 if cpu controller may be enabled, otherwise 0.*/
	char cpu_controller_enabled;			/* Is 1 if cpu controller is enabled, otherwise 0.*/
	
	char populated;							/* Is 1 if subtree has at least one process in it, otherise 0.*/
	
	char max_descendants_value[MAX_DECS_MAX_SIZE];		/*String with the number of maximum descendant cgroups allowed in subtree.*/
	
	char max_depth_value[MAX_DEPTH_MAX_SIZE];			/*String with the number of maximum depth allowed in subtree.*/
	
	char depth[MAX_DEPTH_MAX_SIZE];						/*String with the current depth of the subtree.*/
	
	char nr_descendants[NR_DESC_MAX_SIZE];				/*String with the current number of descendant cgroups.*/
	char nr_dying_descendants[NR_DYING_DESC_MAX_SIZE]; 	/*TODO: check how to incorporate this.*/
	
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
 * Create a new cgroup and initialize it.
 */
struct cgroup * cgroup_create(char* path);

/**
 * Delete a control group (directory).
 * Returns 0 when deleted successfully.
 * Returns -1 when path is not a cgroup directory.
 * Returns -2 when cannot delete cgroup.
 */
int cgroup_delete(char* path);

/**
 * Initialize a cgroup.
 */
void cgroup_initialize(struct cgroup * cgroup, char* path, struct cgroup * parent_cgroup);

/**
 * Insert a process to the control group. Returns -1 if no space.
 * Erases the process from the other group.
 * Must be called with locked process table.
 */
int cgroup_insert(struct cgroup * cgroup, struct proc * proc);

/**
 * Remove a process from the control group.
 * Must be called with locked process table.
 */
void cgroup_erase(struct cgroup * cgroup, struct proc * proc);

/**
 * Enable the cpu controller.
 */
int enable_cpu_controller(struct cgroup * cgroup);

/**
 * Disable the cpu controller.
 */
int disable_cpu_controller(struct cgroup * cgroup);

/**
 * Set the cgroup_dir_path field of the given cgroup to path.
 */
void set_cgroup_dir_path(struct cgroup * cgroup, char* path);

/**
 * Set the cgroup that is located at the given path.
 */
struct cgroup * get_cgroup_by_path(char* path);

/**
 * Set the max_descendants_value field of the given cgroup to value.
 */
void set_max_descendants_value(struct cgroup * cgroup, char* value);

/**
 * Set the max_depth_value field of the given cgroup to value.
 */
void set_max_depth_value(struct cgroup * cgroup, char* value);

/**
 * Set the nr_descendants field of the given cgroup to value.
 */
void set_nr_descendants(struct cgroup * cgroup, char* value);

/**
 * Set the nr_dying_descendants field of the given cgroup to value.
 */
void set_nr_dying_descendants(struct cgroup * cgroup, char* value);

/**
 * Set buf to the names of all chilren cgroup of cgroup at path.
 */
void get_cgroup_names_at_path(char *buf, char *path);

#endif