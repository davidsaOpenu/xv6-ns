#include "cgroup.h"
#include "cgfs.h"
#include "spinlock.h"
#include "memlayout.h"

#define MAX_DES_DEF 64
#define MAX_DEP_DEF 64
#define MAX_CGROUP_FILE_NAME_LENGTH 64
#define CGROUP_ACCOUNT_PERIOD_100MS (100 * 1000)

struct
{
    struct spinlock lock;
    struct cgroup cgroups[NPROC];
} cgtable;

void cginit(void)
{
    initlock(&cgtable.lock, "cgtable");
}

void cgroup_lock()
{
    acquire(&cgtable.lock);
}

void cgroup_unlock()
{
    release(&cgtable.lock);
}

static void dev_id_to_name(uint major, uint minor, char * buf)
{
    char * temp_ptr = buf;

    itoa(buf, major);
    temp_ptr += strlen(buf);
    //put the device major:minor delimiter
    temp_ptr[0] = ':';
    temp_ptr ++;
    itoa(temp_ptr, minor);
}

static struct cgroup * unsafe_get_cgroup_by_path(char * path)
{
    char fpath[MAX_PATH_LENGTH];
    format_path(fpath, path);

    if (*fpath != 0)
        for (int i = 0;
             i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
             i++)
            if (strcmp(cgtable.cgroups[i].cgroup_dir_path, fpath) == 0)
                return &cgtable.cgroups[i];

    return 0;
}

static void unsafe_set_cgroup_dir_path(struct cgroup * cgroup, char * path)
{
    char fpath[MAX_PATH_LENGTH];
    format_path(fpath, path);
    char * fpathp = fpath;
    char * cgroup_dir_path = cgroup->cgroup_dir_path;
    if (*fpathp != 0)
        for (int i = 0; (i < sizeof(cgroup->cgroup_dir_path)) &&
                        ((*cgroup_dir_path++ = *fpathp++) != 0);
             i++)
            ;
}

static void unsafe_cgroup_erase(struct cgroup * cgroup, struct proc * proc)
{
    // Iterate all cgroup processes.
    for (unsigned int i = 0;
         i < sizeof(cgroup->proc) / sizeof(*cgroup->proc);
         ++i) {
        // If process was found, remove it from the cgroup.
        if (proc == cgroup->proc[i]) {
            proc->cgroup = cgroup_root();
            cgroup->proc[i] = 0;

            // Update current number of processes in cgroup subtree for all
            // ancestors.
            while (cgroup != 0) {
                cgroup->num_of_procs--;
                cgroup->current_mem -= proc->sz;
                cgroup->current_page -= PGROUNDUP(proc->sz)/PGSIZE;
                if (cgroup->num_of_procs == 0)
                    cgroup->populated = 0;
                cgroup = cgroup->parent;
            }
            break;
        }
    }
}

void format_path(char * buf, char * path)
{
    /* If the path does not start with '/' then add the current working
     * directory path to the start.*/
    struct proc * curproc = myproc();
    char * bufp = buf;
    if (*path != '/') {
        char * cwdp = curproc->cwdp;
        strncpy(bufp, cwdp, strlen(cwdp));
        bufp += strlen(cwdp);
        if (*(bufp - 1) != '/')
            *bufp++ = '/';
    }

    /* Get Pointer to end of path ('/'s at the end don't count)*/
    char * path_end = path + strlen(path) - 1;
    while (path_end > path && *path_end == '/')
        path_end--;

    /* Copy formatted path to buffer*/
    while (path <= path_end) {
        if (bufp > buf && *(bufp - 1) == '/' && *path == '.') {
            if (*(path + 1) == 0 || *(path + 1) == '/') {
                path += 2;
                continue;
            }
            if (*(path + 1) == '.' &&
                (*(path + 2) == 0 || *(path + 2) == '/')) {
                bufp -= 2;
                while (bufp >= buf && *bufp != '/')
                    bufp--;
                if (bufp < buf) {
                    *buf = 0;
                    return;
                }
                bufp++;
                path += 3;
                continue;
            }
        }
        *bufp++ = *path++;
    }

    /* If the path ends with '/' and is not "/" then remove last '/'. */
    if (bufp - 1 > buf && *(bufp - 1) == '/')
        *(bufp - 1) = 0;
    *bufp = 0;
}

struct cgroup * cgroup_root(void)
{
    return &cgtable.cgroups[0];
}

struct cgroup * cgroup_create(char * path)
{
    char fpath[MAX_PATH_LENGTH];
    format_path(fpath, path);
    char parent_path[MAX_PATH_LENGTH];
    char new_dir_name[MAX_PATH_LENGTH];

    if (get_dir_name(fpath, parent_path) < 0 || get_base_name(fpath, new_dir_name) < 0)
        return 0;

    acquire(&cgtable.lock);

    struct cgroup * parent_cgp = unsafe_get_cgroup_by_path(parent_path);
    /*Cgroup has to be created as a child of another cgroup. (Root cgroup
     * is not created here)*/
    if (parent_cgp == 0) {
        release(&cgtable.lock);
        return 0;
    }

    /*Check if we are allowed to create a new cgorup at the path. For each
     * ancestor check that we haven't reached maximum number of descendants
     * or maximum subtree depth.*/
    struct cgroup * parent_cgp_temp = parent_cgp;
    for (int i = 0; parent_cgp_temp != 0; i++) {
        if (parent_cgp_temp->max_depth_value <= i) {
            release(&cgtable.lock);
            panic("cgroup_create: max depth allowed reached");
        }
        if (parent_cgp_temp->max_descendants_value == parent_cgp_temp->nr_descendants) {
            release(&cgtable.lock);
            panic("cgroup_create: max number of descendants allowed "
                  "reached");
        }
        parent_cgp_temp = parent_cgp_temp->parent;
    }

    /*Find avalible cgroup slot.*/
    struct cgroup * new_cgp = 0;
    for (int i = 1;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (*(cgtable.cgroups[i].cgroup_dir_path) == 0 &&
            cgtable.cgroups[i].ref_count == 0) {
            new_cgp = &cgtable.cgroups[i];
            break;
        }

    /*Check if we have found an avalible slot.*/
    if (new_cgp == 0) {
        release(&cgtable.lock);
        panic("cgroup_create: no avalible cgroup slots");
    }

    /*Initialize the new cgroup.*/
    cgroup_initialize(new_cgp, fpath, parent_cgp);

    /*Update number of descendant cgroups for each ancestor.*/
    while (parent_cgp != 0) {
        parent_cgp->nr_descendants++;
        parent_cgp = parent_cgp->parent;
    }

    release(&cgtable.lock);
    return new_cgp;
}

int cgroup_delete(char * path, char * type)
{
    acquire(&cgtable.lock);
    /*Get cgroup at given path.*/
    struct cgroup * cgp = unsafe_get_cgroup_by_path(path);
    /*If no cgroup at given path return -1.*/
    if (cgp == 0) {
        release(&cgtable.lock);
        return -1;
    }

    if (strcmp(type, "umount") == 0 && cgp != cgroup_root()) {
        release(&cgtable.lock);
        return -2;
    }

    if (strcmp(type, "unlink") == 0 && cgp == cgroup_root()) {
        release(&cgtable.lock);
        return -2;
    }

    /*Check if we are allowed to delete the cgroup. Check if the cgroup has
     * descendants or processes in it.*/
    if (cgp->nr_descendants ||
        (cgp->num_of_procs && cgp != cgroup_root())) {
        release(&cgtable.lock);
        return -2;
    }

    if (cgp != cgroup_root() && cgp -> mem_controller_enabled)
        set_min_mem(cgp, 0);

    /*Delete the path.*/
    *(cgp->cgroup_dir_path) = '\0';

    char increase_num_dying_desc = 0;
    if (cgp->ref_count > 0)
        increase_num_dying_desc = 1;

    /*Update number of descendant cgroups for each ancestor.*/
    cgp = cgp->parent;
    while (cgp != 0) {
        cgp->nr_descendants--;
        if (increase_num_dying_desc)
            cgp->nr_dying_descendants++;
        cgp = cgp->parent;
    }
    release(&cgtable.lock);
    return 0;
}

void cgroup_initialize(struct cgroup * cgroup,
                       char * path,
                       struct cgroup * parent_cgroup)
{
    /*Check if the cgroup is the root or not and initialize accordingly.*/
    if (parent_cgroup == 0) {
        cgroup->cpu_controller_avalible = 1;
        cgroup->cpu_controller_enabled = 1;
        cgroup->depth = 0;
        *(cgroup->cgroup_dir_path) = 0;
        cgroup->parent = 0;
        cgroup->pid_controller_avalible = 1;
        cgroup->pid_controller_enabled = 1;
        cgroup->set_controller_avalible = 1;
        cgroup->set_controller_enabled = 0;
        cgroup->mem_controller_avalible = 1;
        cgroup->mem_controller_enabled = 1;
    }
    else {
        cgroup->parent = parent_cgroup;

        /*Cgroup's cpu controller avalible only when it is enabled in the
         * parent.*/
        if (parent_cgroup->cpu_controller_enabled)
            cgroup->cpu_controller_avalible = 1;
        else
            cgroup->cpu_controller_avalible = 0;

        /*Cgroup's pid controller avalible only when it is enabled in the
         * parent.*/
        if (parent_cgroup->pid_controller_enabled)
            cgroup->pid_controller_avalible = 1;
        else
            cgroup->pid_controller_avalible = 0;

        /*Cgroup's set controller avalible only when it is enabled in the
         * parent. Notice doesn't apply to root, it is not enabled in root*/
        if (parent_cgroup == cgroup_root())
            cgroup->set_controller_avalible = 1;
        else {
            if (parent_cgroup->set_controller_enabled)
                cgroup->set_controller_avalible = 1;
            else
                cgroup->set_controller_avalible = 0;
        }

        /*Cgroup's memory controller avalible only when it is enabled in the
        * parent.*/
        if (parent_cgroup->mem_controller_enabled)
          cgroup->mem_controller_avalible = 1;
        else
          cgroup->mem_controller_avalible = 0;

        cgroup->pid_controller_enabled = 0;
        cgroup->cpu_controller_enabled = 0;
        cgroup->set_controller_enabled = 0;
        cgroup->mem_controller_enabled = 0;
        cgroup->depth = cgroup->parent->depth + 1;
        unsafe_set_cgroup_dir_path(cgroup, path);
    }

    cgroup->ref_count = 0;
    cgroup->num_of_procs = 0;
    cgroup->populated = 0;
    cgroup->current_mem = 0;
    set_max_descendants_value(cgroup, MAX_DES_DEF);
    set_max_depth_value(cgroup, MAX_DEP_DEF);
    set_nr_descendants(cgroup, 0);
    set_nr_dying_descendants(cgroup, 0);
    // Without any changes, set the maximum number of processes to max in system
    set_max_procs(cgroup, NPROC);
    // Without any changes, set the default cpu id to be used as 0
    set_cpu_id(cgroup, 0);
    // By default a group is not frozen
    frz_grp(cgroup, 0);

    cgroup->mem_stat_file_dirty = 0;
    cgroup->mem_stat_file_dirty_aggregated = 0;
    cgroup->mem_stat_pgfault = 0;
    cgroup->mem_stat_pgmajfault = 0;

    // By default a group has limit of KERNBASE memory, if parent set
    // its max value to something else, we pass it accordingly
    if(parent_cgroup == 0)
        set_max_mem(cgroup, KERNBASE);
    else
    {
        set_max_mem(cgroup, parent_cgroup->max_mem);
    }

    // By default a group has minimum 0 memory.
    set_min_mem(cgroup, 0);
    cgroup->current_page = 0;
    cgroup->protected_mem = 0;

    cgroup->cpu_account_frame = 0;
    cgroup->cpu_percent = 0;
    cgroup->cpu_time = 0;
    cgroup->cpu_period_time = 0;
    cgroup->cpu_time_limit = ~0;
    cgroup->cpu_account_period = CGROUP_ACCOUNT_PERIOD_100MS;
    cgroup->cpu_nr_periods = 0;
    cgroup->cpu_nr_throttled = 0;
    cgroup->cpu_throttled_usec = 0;
    cgroup->cpu_is_throttled_period = 0;

    /* IO statistics initialization */
    memset(cgroup->io_stats, 0, sizeof(cgroup_io_device_statistics_t));
    cgroup->used_devices = 0;
}

int cgroup_insert(struct cgroup * cgroup, struct proc * proc)
{
    acquire(&cgtable.lock);
    int res = unsafe_cgroup_insert(cgroup, proc);
    release(&cgtable.lock);
    return res;
}

int unsafe_cgroup_insert(struct cgroup * cgroup, struct proc * proc)
{
    // If the number of processes in the cgroup is already at max allowed and pid controller enabled, return error
    if (cgroup->pid_controller_enabled == 1 &&
        (cgroup->num_of_procs + 1) > cgroup->max_num_of_procs)
        return -1;

    // If the process memory in addition to existing memory is over the limit and memory controller is enabled, return error.
    if (cgroup->mem_controller_enabled == 1 &&
      (cgroup->current_mem + proc->sz) > cgroup->max_mem)
      return -1;

    // Whether a free slot was found.
    int found = 0;

    // Index of the free slot.
    unsigned int index = 0;

    // Find available slot.
    for (unsigned int i = 0;
         i < sizeof(cgroup->proc) / sizeof(*cgroup->proc);
         ++i) {
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
        if (protect_memory(proc->cgroup, cgroup, proc->sz)!=0) {
            return -1;
        }
        unsafe_cgroup_erase(proc->cgroup, proc);
    }

    // Associate the process with the cgroup.
    cgroup->proc[index] = proc;

    // Set the cgroup of the current process.
    proc->cgroup = cgroup;

    // Update current number of processes in cgroup subtree for all
    // ancestors.
    while (cgroup != 0) {
        cgroup->num_of_procs++;
        cgroup->populated = 1;
        cgroup->current_mem += proc->sz;
        cgroup->current_page += PGROUNDUP(proc->sz)/PGSIZE;
        cgroup = cgroup->parent;
    }
    return 0;
}

int protect_memory(struct cgroup* src, struct cgroup* dst, int proc_size) {

    // By moving procces fron dst cgroup to src we need to decrease
    // protected memory pages for dst cgroup and decrease for src
    int proc_pg = PGROUNDUP(proc_size) / PGSIZE;
    int dec = calc_dec_dst_protect_pg(dst, proc_pg);
    int inc = calc_inc_src_protect_pg(src, proc_pg);
    if (increse_protect_counter(inc - dec) == 0) {
        dst->protected_mem -= dec;
        src->protected_mem += inc;
        return 0;
    }
    return 1;
}

int calc_dec_dst_protect_pg(struct cgroup* cgroup, int proc_pg)
{
    if (cgroup != cgroup_root() && cgroup->mem_controller_enabled && cgroup->protected_mem > 0) {// There is proteted mem to releas
        int used_pg = cgroup->current_page + proc_pg;
        int need_protect = PGROUNDUP(cgroup->min_mem)/PGSIZE - used_pg;
        if (need_protect <= 0) { // Cgroup used memory more or equal to mem min
            return cgroup->protected_mem;
        }
        else {
            return (cgroup->protected_mem - need_protect);
        }
    }
    else
        return 0;
}

int calc_inc_src_protect_pg(struct cgroup* cgroup, int proc_pg)
{
   if (cgroup != cgroup_root() && cgroup->mem_controller_enabled) {
      int used_pg = cgroup->current_page - proc_pg;
      int need_protect = PGROUNDUP(cgroup->min_mem)/PGSIZE - used_pg;
      if (need_protect <= 0) { // Cgroup used memory more or equal to mem min
            return 0;
        }
        else {
            return (need_protect - cgroup->protected_mem);
        }
   }
   return 0;
}

void cgroup_erase(struct cgroup* cgroup, struct proc* proc)
{
    acquire(&cgtable.lock);
    unsafe_cgroup_erase(cgroup, proc);
    release(&cgtable.lock);
}

int unsafe_enable_cpu_controller(struct cgroup * cgroup)
{
    // If cgroup has processes in it, controllers can't be enabled.
    if (!cgroup || cgroup->populated == 1) {
        return -1;
    }

    // If controller is enabled do nothing.
    if (cgroup->cpu_controller_enabled) {
        return 0;
    }

    if (cgroup->cpu_controller_avalible) {
        /* TODO: complete activation of controller. */

        // Set cpu controller to enabled.
        cgroup->cpu_controller_enabled = 1;
        // Set cpu controller to avalible in all child cgroups.
        for (int i = 1;
             i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
             i++)
            if (cgtable.cgroups[i].parent == cgroup)
                cgtable.cgroups[i].cpu_controller_avalible = 1;
    }

    return 0;
}

int enable_cpu_controller(struct cgroup * cgroup)
{
    acquire(&cgtable.lock);
    int res = unsafe_enable_cpu_controller(cgroup);
    release(&cgtable.lock);
    return res;
}

int unsafe_disable_cpu_controller(struct cgroup * cgroup)
{
    if (!cgroup) {
        return -1;
    }

    // If controller is disabled do nothing.
    if (!cgroup->cpu_controller_enabled) {
        return 0;
    }

    // Check that all child cgroups have cpu controller disabled. (cannot
    // disable controller when children have it enabled)
    for (int i = 1;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (cgtable.cgroups[i].parent == cgroup &&
            cgtable.cgroups[i].cpu_controller_enabled) {
            return -1;
        }

    /* TODO: complete deactivation of controller. */

    // Set cpu controller to enabled.
    cgroup->cpu_controller_enabled = 0;

    // Set cpu controller to unavalible in all child cgroups.
    for (int i = 1;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (cgtable.cgroups[i].parent == cgroup)
            cgtable.cgroups[i].cpu_controller_avalible = 0;

    return 0;
}

int disable_cpu_controller(struct cgroup * cgroup)
{
    acquire(&cgtable.lock);
    int res = unsafe_disable_cpu_controller(cgroup);
    release(&cgtable.lock);
    return res;
}

void set_cgroup_dir_path(struct cgroup * cgroup, char * path)
{
    acquire(&cgtable.lock);
    unsafe_set_cgroup_dir_path(cgroup, path);
    release(&cgtable.lock);
}

struct cgroup * get_cgroup_by_path(char * path)
{
    acquire(&cgtable.lock);
    struct cgroup * cgp = unsafe_get_cgroup_by_path(path);
    release(&cgtable.lock);
    return cgp;
}

void set_max_descendants_value(struct cgroup * cgroup, unsigned int value)
{
    if (value >= 0)
        cgroup->max_descendants_value = value;
}

void set_max_depth_value(struct cgroup * cgroup, unsigned int value)
{
    if (value >= 0)
        cgroup->max_depth_value = value;
}

void set_nr_descendants(struct cgroup * cgroup, unsigned int value)
{
    if (value >= 0)
        cgroup->nr_descendants = value;
}

void set_nr_dying_descendants(struct cgroup * cgroup, unsigned int value)
{
    if (value >= 0)
        cgroup->nr_dying_descendants = value;
}

void get_cgroup_names_at_path(char * buf, char * path)
{
    if (*path == 0)
        return;

    for (int i = 1;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (*(cgtable.cgroups[i].cgroup_dir_path) != 0 &&
            strcmp(cgtable.cgroups[i].parent->cgroup_dir_path, path) ==
                0) {
            char * child_name =
                &(cgtable.cgroups[i].cgroup_dir_path[strlen(path) + 1]);
            int child_name_len = strlen(child_name);
            while (*child_name != 0)
                *buf++ = *child_name++;
            buf += MAX_CGROUP_FILE_NAME_LENGTH - child_name_len;
        }
}

int cgorup_num_of_immidiate_children(struct cgroup * cgroup)
{
    char * path = cgroup->cgroup_dir_path;
    int num = 0;
    if (*path == 0)
        return -1;

    for (int i = 0;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (*(cgtable.cgroups[i].cgroup_dir_path) != 0 &&
            strcmp(cgtable.cgroups[i].parent->cgroup_dir_path, path) == 0)
            num++;

    return num;
}

void decrement_nr_dying_descendants(struct cgroup * cgroup)
{
    while (cgroup != 0) {
        cgroup->nr_dying_descendants--;
        cgroup = cgroup->parent;
    }
}

int cg_open(cg_file_type type, char * filename, struct cgroup * cgp, int omode)
{
    acquire(&cgtable.lock);
    int res = unsafe_cg_open(type, filename, cgp, omode);
    release(&cgtable.lock);
    return res;
}

int cg_sys_open(char * path, int omode)
{
    struct cgroup *cgp;

    if ((cgp = get_cgroup_by_path(path)))
        return cg_open(CG_DIR, 0, cgp, omode);

    char dir_path[MAX_PATH_LENGTH];
    char file_name[MAX_PATH_LENGTH];

    if (get_dir_name(path, dir_path) == 0 && get_base_name(path, file_name) == 0 && (cgp = get_cgroup_by_path(dir_path)))
        return cg_open(CG_FILE, file_name, cgp, omode);

    return -1;
}

int cg_read(cg_file_type type, struct file * f, char * addr, int n)
{
    acquire(&cgtable.lock);
    int res = unsafe_cg_read(type, f, addr, n);
    release(&cgtable.lock);
    return res;
}

int cg_write(struct file * f, char * addr, int n)
{
    acquire(&cgtable.lock);
    int res = unsafe_cg_write(f, addr, n);
    release(&cgtable.lock);
    return res;
}

int cg_close(struct file * file)
{
    acquire(&cgtable.lock);
    int res = unsafe_cg_close(file);
    release(&cgtable.lock);
    return res;
}

int cg_stat(struct file * f, struct stat * st)
{
    acquire(&cgtable.lock);
    int res = unsafe_cg_stat(f, st);
    release(&cgtable.lock);
    return res;
}

int set_max_procs(struct cgroup * cgroup, int limit) {
    // If no cgroup found, return error.
    if (cgroup == 0)
        return -1;

    // Set the limit if it is within allowed parameters.
    // 0 is used for testing.
    if (limit >= 0 && limit <= NPROC) {
        cgroup->max_num_of_procs = limit;
        return 1;
    }

    return 0;
}

int unsafe_enable_pid_controller(struct cgroup *cgroup) {
    // If cgroup has processes in it, controllers can't be enabled.
    if (cgroup == 0 || cgroup->populated == 1) {
        return -1;
    }

    // If controller is enabled do nothing.
    if (cgroup->pid_controller_enabled) {
        return 0;
    }

    if (cgroup->pid_controller_avalible) {
        // Set pid controller to enabled.
        cgroup->pid_controller_enabled = 1;
        // Set pid controller to avalible in all child cgroups.
        for (int i = 1;
                i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
                i++)
            if (cgtable.cgroups[i].parent == cgroup)
                cgtable.cgroups[i].pid_controller_avalible = 1;
    }

    return 0;
}

int unsafe_disable_pid_controller(struct cgroup *cgroup) {
    if (cgroup == 0) {
        return -1;
    }

    // If controller is disabled do nothing.
    if (cgroup->pid_controller_enabled == 0) {
        return 0;
    }

    // Check that all child cgroups have pid controller disabled. (cannot
    // disable controller when children have it enabled)
    for (int i = 1;
            i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
            i++)
        if (cgtable.cgroups[i].parent == cgroup &&
                cgtable.cgroups[i].pid_controller_enabled) {
            return -1;
        }

    // Set pid controller to disabled.
    cgroup->pid_controller_enabled = 0;

    // Set pid controller to unavalible in all child cgroups.
    for (int i = 1;
            i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
            i++)
        if (cgtable.cgroups[i].parent == cgroup)
            cgtable.cgroups[i].pid_controller_avalible = 0;

    return 0;
}

int enable_pid_controller(struct cgroup * cgroup)
{
    acquire(&cgtable.lock);
    int res = unsafe_enable_pid_controller(cgroup);
    release(&cgtable.lock);
    return res;
}

int disable_pid_controller(struct cgroup * cgroup)
{
    acquire(&cgtable.lock);
    int res = unsafe_disable_pid_controller(cgroup);
    release(&cgtable.lock);
    return res;
}

int set_cpu_id(struct cgroup * cgroup, int cpuid) {
    // If no cgroup found, return error.
    if (cgroup == 0)
        return -1;

    // Set the cpu id if it is within allowed parameters.
    // NCPU+1 is used for testing, since this cpu id can never be in the system.
    if (cpuid >= 0 && cpuid <= NCPU + 1) {
        cgroup->cpu_to_use = cpuid;
        return 1;
    }

    return 0;
}

int unsafe_enable_set_controller(struct cgroup *cgroup) {
    // If cgroup has processes in it, controllers can't be enabled.
    if (cgroup == 0 || cgroup->populated == 1) {
        return -1;
    }

    // If controller is enabled do nothing.
    if (cgroup->set_controller_enabled) {
        return 0;
    }

    if (cgroup->set_controller_avalible) {
        // Set cpu set controller to enabled.
        cgroup->set_controller_enabled = 1;
        // Set cpu set controller to avalible in all child cgroups.
        for (int i = 1;
             i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
             i++)
            if (cgtable.cgroups[i].parent == cgroup)
                cgtable.cgroups[i].set_controller_avalible = 1;
    }

    return 0;
}

int unsafe_disable_set_controller(struct cgroup *cgroup) {
    if (cgroup == 0) {
        return -1;
    }

    // If controller is disabled do nothing.
    if (cgroup->set_controller_enabled == 0) {
        return 0;
    }

    // Check that all child cgroups have cpu set controller disabled. (cannot
    // disable controller when children have it enabled)
    for (int i = 1;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (cgtable.cgroups[i].parent == cgroup &&
            cgtable.cgroups[i].set_controller_enabled) {
            return -1;
        }

    // Set cpu set controller to disabled.
    cgroup->set_controller_enabled = 0;

    // Set cpu set controller to unavalible in all child cgroups.
    for (int i = 1;
         i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
         i++)
        if (cgtable.cgroups[i].parent == cgroup)
            cgtable.cgroups[i].set_controller_avalible = 0;

    return 0;
}

int enable_set_controller(struct cgroup * cgroup)
{
    acquire(&cgtable.lock);
    int res = unsafe_enable_set_controller(cgroup);
    release(&cgtable.lock);
    return res;
}

int disable_set_controller(struct cgroup * cgroup)
{
    acquire(&cgtable.lock);
    int res = unsafe_disable_set_controller(cgroup);
    release(&cgtable.lock);
    return res;
}

int frz_grp(struct cgroup * cgroup, int frz) {
    // If no cgroup found, return error.
    if (cgroup == 0)
        return -1;

    // Freeze/unfreeze cgroup based on input.
    if (frz == 1 || frz == 0) {
        cgroup->is_frozen = frz;
        return 1;
    }

    return 0;
}

int set_max_mem(struct cgroup* cgroup, unsigned int limit) {
  // If no cgroup found, return error.
  if (cgroup == 0)
    return -1;
    
  // Set the limit if it is within allowed parameters.
  // 0 is used for testing.
  if (limit >= 0 && limit <= KERNBASE && limit >= cgroup->min_mem) {
    cgroup->max_mem = limit;
    return 1;
  }

  return 0;
}

int set_min_mem(struct cgroup* cgroup, unsigned int limit) {
    // If no cgroup found, return error.
    if (cgroup == 0)
        return CGROUP__RESULT_FAILURE;

    // don't allow a child cgroup to set minimum value more than
    // the max set in its parent
    if(cgroup != cgroup_root())
    {
        if(cgroup->parent->max_mem < limit)
        {
            return CGROUP__RESULT_FAILURE;
        }
    }

    // Set the limit if it is within allowed parameters.
    // 0 is used for testing.
    if (limit >= 0 && limit <= KERNBASE && limit <= cgroup->max_mem) {
        if (set_protect_mem(cgroup, PGROUNDUP(limit) / PGSIZE) == 0) {
            cgroup->min_mem = limit;
            return CGROUP__RESULT_SUCCESS;
        }
    }

    return CGROUP__RESULT_FAILURE;
}

int set_protect_mem(struct cgroup* cgroup, unsigned int pages) {

    int protect = pages - cgroup->current_page;
    if (protect <= 0){//actualy we dont need to protect memory, cgroup use memory more then min
        if (cgroup->protected_mem > 0) {// we need to release all protectd memory
            decrese_protect_counter(cgroup->protected_mem);
            cgroup->protected_mem = 0;
        }
    }
    else {// we do need to protect memory
            if (increse_protect_counter(protect - cgroup->protected_mem) == 0) //there is enough memory to protect or we decreas
                cgroup->protected_mem = protect;
            else
                return CGROUP__RESULT_FAILURE;
    }

    return CGROUP__RESULT_SUCCESS;
}

int unsafe_enable_mem_controller(struct cgroup* cgroup) {
  // If cgroup has processes in it, controllers can't be enabled.
  if (cgroup == 0 || cgroup->populated == 1) {
    return -1;
  }

  // If controller is enabled do nothing.
  if (cgroup->mem_controller_enabled) {
    return 0;
  }

  int protect = PGROUNDUP(cgroup->min_mem)/PGSIZE - cgroup->current_page;
  if (protect > 0)
      if (increse_protect_counter(protect) != 0)
          return -1;

  if (cgroup->mem_controller_avalible) {
    // Set memory controller to enabled.
    cgroup->mem_controller_enabled = 1;
    // Set memory controller to avalible in all child cgroups.
    for (int i = 1;
      i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
      i++)
      if (cgtable.cgroups[i].parent == cgroup)
        cgtable.cgroups[i].mem_controller_avalible = 1;
  }

  return 0;
}

int unsafe_disable_mem_controller(struct cgroup* cgroup) {
  if (cgroup == 0) {
    return -1;
  }

  // If controller is disabled do nothing.
  if (cgroup->mem_controller_enabled == 0) {
    return CGROUP__RESULT_FAILURE;
  }

  // Check that all child cgroups have memory controller disabled. (cannot
  // disable controller when children have it enabled)
  for (int i = 1;
    i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
    i++)
    if (cgtable.cgroups[i].parent == cgroup &&
      cgtable.cgroups[i].mem_controller_enabled) {
      return CGROUP__RESULT_SUCCESS;
    }

  // Set memory controller to disabled.
  cgroup->mem_controller_enabled = 0;
  // set limits to default
  set_min_mem(cgroup, 0);
  set_max_mem(cgroup, KERNBASE);

  // Set memory controller to unavalible in all child cgroups.
  for (int i = 1;
    i < sizeof(cgtable.cgroups) / sizeof(cgtable.cgroups[0]);
    i++)
    if (cgtable.cgroups[i].parent == cgroup)
      cgtable.cgroups[i].mem_controller_avalible = 0;

  return CGROUP__RESULT_SUCCESS;
}

int enable_mem_controller(struct cgroup* cgroup)
{
  acquire(&cgtable.lock);
  int res = unsafe_enable_mem_controller(cgroup);
  release(&cgtable.lock);
  return res;
}

int disable_mem_controller(struct cgroup* cgroup)
{
  acquire(&cgtable.lock);
  int res = unsafe_disable_mem_controller(cgroup);
  release(&cgtable.lock);
  return res;
}

void cgroup_mem_stat_file_dirty_incr(struct cgroup* cgroup)
{
    if (cgroup != cgroup_root() && cgroup != 0 && cgroup->populated == 1) {
        cgroup->mem_stat_file_dirty++;
    }
}

void cgroup_mem_stat_file_dirty_decr(struct cgroup* cgroup)
{
    if (cgroup != cgroup_root() && cgroup != 0 && cgroup->populated == 1) {
        cgroup->mem_stat_file_dirty--;
    }
}

void cgroup_mem_stat_file_dirty_aggregated_incr(struct cgroup* cgroup)
{
    if (cgroup != cgroup_root() && cgroup != 0 && cgroup->populated == 1) {
        cgroup->mem_stat_file_dirty_aggregated++;
    }
}

void cgroup_mem_stat_pgfault_incr(struct cgroup* cgroup)
{
    if (cgroup != cgroup_root() && cgroup != 0 && cgroup->populated == 1) {
        cgroup->mem_stat_pgfault++;
    }
}

void cgroup_mem_stat_pgmajfault_incr(struct cgroup* cgroup)
{
    if (cgroup != cgroup_root() && cgroup != 0 && cgroup->populated == 1) {
        cgroup->mem_stat_pgmajfault++;
    }
}

/* add IO device to the cgroup's available IO device array */
void cgroup_add_io_device(struct cgroup * cgroup_ptr, struct inode * node)
{
    uint major = 0;
    uint minor = 0;
    int i = 0;
    char device_name[DEVICE_NAME] = {0};

    if (cgroup_ptr != cgroup_root() && cgroup_ptr != 0 && cgroup_ptr->populated == 1)
    {
        /* TODO: not sure if this necessary (we can't just exit this function because
        of the fact that we exceeded the amount of available IO devices per cgroup) */
        if(cgroup_ptr->used_devices > NDEV)
        {
            panic("Can't add more IO devices to cgroup");
        }

        cgroup_ptr->used_devices ++;

        /* set the device name */
        major = node->major;
        minor = node->minor;

        //write the device name in the format ("major:minor")
        dev_id_to_name(major, minor, device_name);

        /* The same entry in io_stats should be empty */
        strncpy(cgroup_ptr->io_stats[i].dev_name, device_name, DEVICE_NAME);
        cgroup_ptr->io_stats[i].major = major;
        cgroup_ptr->io_stats[i].minor = minor;
    }
}

/* remove IO device from the cgroup's available IO device array */
void cgroup_remove_io_device(struct cgroup * cgroup_ptr, struct inode * node)
{
    int major = -1;
    int minor = -1;

    // Note: most of the times the remove_io_device will be issued from the root cgroup

    if (cgroup_ptr == cgroup_root() || cgroup_ptr == 0 || cgroup_ptr->populated != 1)
        return;

    if(cgroup_ptr->used_devices == 0)
    {
        panic("No available IO devices in cgroup to remove");
    }

    major = node->major;
    minor = node->minor;

    for(int i = 0; i < NDEV; i++)
    {
        // Note: we assume that io_stats is initialized when cgroup is initialized
        if((major == cgroup_ptr->io_stats[i].major) && (minor == cgroup_ptr->io_stats[i].minor))
        {
            // clear the io statistics structure - it's irrelevant now
            memset(&(cgroup_ptr->io_stats[i]), 0, sizeof(cgroup_io_device_statistics_t));
            cgroup_ptr->used_devices --;
        }
    }
}

void set_cgroup_io_stat(struct file *f)
{
    if(f == (void *)0)
        panic("Invalid file handler (NULL), can't set io stats");

    for(int i = 0; i < NDEV; i++)
        f->io.devices_stats[i] = (cgroup_io_device_statistics_t *)0;
}

void get_cgroup_io_stat(struct file *f, struct cgroup * cgp)
{
    struct dev_stat device_stat = {0};
    int dev_major = 0;
    int dev_minor = 0;
    int cnt = 0;

    if(f == (void *)0)
        panic("Invalid file handler (NULL), can't get io stats");
 
    if(cgp == (void *)0)
        panic("Invalid cgroup (NULL), can't get io stats");

    for(int i = 0; i < NDEV; i++)
    {
        /* Note: counting on i-nodes in this case is problematic.
        The problem is that if the user closes
        the inode then, it might be used by other file/device.
        This will lead to failure (so we can't rely on inodes).
        That's why io_stats was introduced. 
        */

        dev_major = cgp->io_stats[i].major;
        dev_minor = cgp->io_stats[i].minor;
        if(0 == dev_major || (cgp->io_stats[i].dev_name == (char *)0))
        {  continue; }

        if(cnt >= cgp->used_devices)
            break;
        
        /* get the stats of the IO device from its driver via minor/major nuimbers */
        devsw[dev_major].stat(dev_minor, &device_stat);

        /* copy the stats to the cgroup structure of the cgroup interface (IO_STAT)
           Note: The cgroup's io_stats are ordered via indexes
        */
        memmove((struct dev_stat *) &(cgp->io_stats[i].device_stats), &device_stat,
                 sizeof(struct dev_stat));

        //set also the cgroup's io stats pointers in the file structure
        f->io.devices_stats[cnt] = &(cgp->io_stats[i]);

        // clear just in case we have garbage here (we are reusing the struct)
        memset(&device_stat, 0, sizeof(struct dev_stat));
        cnt ++;
    }
}

