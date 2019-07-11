#include "cgroup.h"

/**
 * Open a cgroup filesystem file.
 * 	currently supports opening:
 *	1)	"cgroup.procs"
 *	2)	"cgroup.controllers"
 *	3)	"cgroup.subree_control"
 *	4)	"cgroup.events"
 *	5)	"cgroup.max.descendants"
 *	6)	"cgroup.max.depth"
 *	7)	"cgroup.stat"
 */ 
int opencgfile(char *filename, struct cgroup *cgp, int omode);

/**
 * read from cgroup filesystem file
 * 	currently supports reading from:
 *	1)	"cgroup.procs"
 *	2)	"cgroup.controllers"
 *	3)	"cgroup.subree_control"
 *	4)	"cgroup.events"
 *	5)	"cgroup.max.descendants"
 *	6)	"cgroup.max.depth"
 *	7)	"cgroup.stat"
 */
int readcgfile(struct file *f, char *addr, int n);

/**
 * Open a cgroup filesystem file.
 * 	currently supports writing to:
 *	1)	"cgroup.procs"
 *	2)	"cgroup.subree_control"
 *	3)	"cgroup.max.descendants"
 *	4)	"cgroup.max.depth"
 */ 
int writecgfile(struct file *f, char *addr, int n);


/**
 * Get from path the directory path and the file name and set dir_path and file_name accordingly.
 */ 
int get_cg_file_dir_path_and_file_name(char* path, char* dir_path, char* file_name);