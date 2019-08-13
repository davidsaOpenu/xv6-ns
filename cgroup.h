#include "param.h"
#include "proc.h"
#include "defs.h"

#ifndef XV6_CGROUP_H
#define XV6_CGROUP_H

#define MAX_DECS_MAX_SIZE 3
#define MAX_DEPTH_MAX_SIZE 3
#define NR_DESC_MAX_SIZE 3
#define NR_DYING_DESC_MAX_SIZE 3

#define MAX_CONTROLLER_NAME_LENGTH 16	// Max length allowed for controller names

/**
 * Control group, contains up to NPROC processes.
 */
struct cgroup
{
    char cgroup_dir_path[MAX_PATH_LENGTH]; /* Path of the cgroup
                                              directory.*/

    int ref_count; /* Reference count.*/

    struct proc * proc[NPROC]; /* Array of all processes in the cgroup.*/
    int num_of_procs;          /* Number of processes in the cgroup subtree
                                  (including processes in this cgroup).*/

    struct cgroup * parent; /* The parent cgroup.*/

    char cpu_controller_avalible; /* Is 1 if cpu controller may be enabled,
                                     otherwise 0.*/
    char cpu_controller_enabled;  /* Is 1 if cpu controller is enabled,
                                     otherwise 0.*/

    char populated; /* Is 1 if subtree has at least one process in it,
                       otherise 0.*/

    char max_descendants_value
        [MAX_DECS_MAX_SIZE]; /*String with the number of maximum descendant
                                cgroups allowed in subtree.*/

    char max_depth_value[MAX_DEPTH_MAX_SIZE]; /*String with the number of
                                                 maximum depth allowed in
                                                 subtree.*/

    char depth[MAX_DEPTH_MAX_SIZE]; /*String with the current depth of the
                                       subtree.*/

    char nr_descendants[NR_DESC_MAX_SIZE]; /*String with the current number
                                              of descendant cgroups.*/
    char nr_dying_descendants
        [NR_DYING_DESC_MAX_SIZE]; /*String with the current number of dying descendant cgroups.*/
};

/**
 * Returns the root cgroup, &cgroups[0].
 */
struct cgroup * cgroup_root(void);

/**
 * Create a new cgroup and initialize it at the given path.
 * Checks that cgroup has parent. 
 * Checks that the limits set in ancestors are not exceeded.
 * Check if we have avalible slot in the table.
 * Updates number of descendants for ancestors.
 * Returns 0 on failure.
 * Any path can be given, the function formats it before usage.
 */
struct cgroup * cgroup_create(char * path);

/**
 * Delete a control group (directory).
 * Returns 0 when deleted successfully.
 * Returns -1 when path is not a cgroup directory.
 * Returns -2 when cannot delete cgroup.
 * Any path can be given, the path is formatted before usage.
 * Type must be "umount" when called from umount systemcall.
 * Type must be "unlink" when called from unlink systemcall.
 * Currently no other types are supported.
 * If the reference count is not 0, the cgroup becomes dying.
 */
int cgroup_delete(char * path, char * type);

/**
 * Initialize a cgroup.
 * The cgroup given by 'cgroup' is initializd with the path 'path' and parent 'parent_cgroup'.
 * Any path can be given, the path is formatted before usage.
 * 'parent_cgroup' must be valid cgroup, the function does not check validity.
 */
void cgroup_initialize(struct cgroup * cgroup,
                       char * path,
                       struct cgroup * parent_cgroup);

/**
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table lock and safe does).
 * Insert a process to the control group. Returns -1 if no space. Returns 0 on success.
 * Erases the process from the other cgroup.
 * Associates process with cgroup.
 * 'cgroup' must be valid cgroup, 'proc' must be valid process.
 */
int unsafe_cgroup_insert(struct cgroup * cgroup, struct proc * proc);
int cgroup_insert(struct cgroup * cgroup, struct proc * proc);

/**
 * Remove a process from the control group.
 * Process is dissociated with cgroup.
 * Returns -1 on failure.
 * 'cgroup' must be valid cgroup, 'proc' must be valid process.
 */
void cgroup_erase(struct cgroup * cgroup, struct proc * proc);

/**
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table lock and safe does).
 * Enable the cpu controller of given cgroup.
 * Returns -1 on failure.
 */
int unsafe_enable_cpu_controller(struct cgroup * cgroup);
int enable_cpu_controller(struct cgroup * cgroup);

/**
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table lock and safe does).
 * Disable the cpu controller of given cgroup.
 * Returns -1 on failure.
 */
int unsafe_disable_cpu_controller(struct cgroup * cgroup);
int disable_cpu_controller(struct cgroup * cgroup);

/**
 * Set the cgroup_dir_path field of the given cgroup to path.
 * Any path can be given, the function formats it before usage.
 */
void set_cgroup_dir_path(struct cgroup * cgroup, char * path);

/**
 * Get the cgroup that is located at the given path.
 * Any path can be given, the function formats it before usage.
 * Returns 0 on failure.
 */
struct cgroup * get_cgroup_by_path(char * path);

/**
 * Set the max_descendants_value field of the given cgroup to value.
 */
void set_max_descendants_value(struct cgroup * cgroup, char * value);

/**
 * Set the max_depth_value field of the given cgroup to value.
 */
void set_max_depth_value(struct cgroup * cgroup, char * value);

/**
 * Set the nr_descendants field of the given cgroup to value.
 */
void set_nr_descendants(struct cgroup * cgroup, char * value);

/**
 * Set the nr_dying_descendants field of the given cgroup to value.
 */
void set_nr_dying_descendants(struct cgroup * cgroup, char * value);

/**
 * Set buf to the names of all chilren cgroup of cgroup at path.
 * Buf must be big enough to fit all of them.
 */
void get_cgroup_names_at_path(char * buf, char * path);

/**
 * Get number of cgroup's immidiate children.
 * Returns -1 on failure.
 */
int cgorup_num_of_immidiate_children(struct cgroup * cgroup);

/**
 * Format path and write it into buf.
 * path can be any path. 
 * The function does the following:
 * 1) If 'path' does not start with '/' then it copies the current working directory to the buffer,
 * 2) Copies path to buffer while changing "/.." and "/." to the appropriate paths.
 * 3) '/'-es at the end of path are not copied.
 * 4) If the buffer ends with '/' then it is deleted.
 *
 * Example:
 * for given path = "b/./c/d/../e/////" and cwd = "/a"
 * buf will be set to : "a/b/c/e"
 */
void format_path(char * buf, char * path);

/**
 * Decrement number of dying descendants for given cgroup and every
 * ancestor.
 */
void decrement_nr_dying_descendants(struct cgroup * cgroup);

#endif
