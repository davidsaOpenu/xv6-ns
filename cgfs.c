#include "cgfs.h"
#include "defs.h"
#include "fcntl.h"
#include "file.h"
#include "fs.h"
#include "param.h"
#include "spinlock.h"
#include "types.h"
#include "mmu.h"

#define MAX_PID_LENGTH 5
#define MAX_CGROUP_DIR_ENTRIES 64
#define MAX_STR 64
#define MAX_BUF 4096

// TODO: refactor - move to defs.h
#define min(x, y) (x) > (y) ? (y) : (x)
// TODO: refactor - move to defs.h (also change the name)
#define abs(x) (x) > 0 ? (x) : (0)

// Is static to save space in the stack
static char buf[MAX_BUF];


/**
 * This function copies from given buffer into a given address based on the input parameters.
 * The function gets stopped when it encounters a null terminator character in the input buffer.
 * Receives char parameter pointer "src", int parameter "size", char parameter pointer "dest".
 *
 * "src" is the source we are reading from.
 * "size" is an integer indicating how many characters we allowed to copy.
 * "dest" is the buffer which "buf" is being copied into.
 *
 * The function returns the amount of bytes copied to the dest buffer.
*/
static inline int copy_buffer_up_to_end(char* src, int size, char* dest)
{
    int i;
    for (i = 0; i < size && src[i]; i++) {
        dest[i] = src[i];
    }
    return i;
}

/**
 * This function copies from given buffer into a given address based on the input parameters.
 * The function gets stopped when it encounters a null terminator character in the input buffer,
 * and instead, replace it with a newline character.
 * Receives char parameter pointer "src", int parameter "size", char parameter pointer "dest".
 *
 * "src" is the source we are reading from.
 * "size" is an integer indicating how many characters we allowed to copy.
 * "dest" is the buffer which "buf" is being copied into.
 *
 * The function returns the amount of bytes copied to the dest buffer.
*/
static inline int copy_buffer_up_to_end_replace_end_with_newline(char* src, int size, char* dest)
{

    int i;
    for (i = 0; i < size; i++) {
        if (!src[i]) {
            dest[i] = '\n';
            break;
        }
        dest[i] = src[i];
    }
    return i;
}

/**
 * This function receives a character which should be a '+' or a '-'.
 *
 * "input_char" is the '+'/'-' input character the function should receive.
 *
 * The function returns the new controller state based on the input character
 * in the following way:
 * value 1 means to enable controller ('+')
 * value 2 means to disable controller ('-')
 *
 * In case of an invliad parameter, the function returns 0 as an error.
*/
static inline int return_new_controller_state(char input_char)
{
    switch (input_char) {
        case '+':
            return 1;
            break;
        case '-':
            return 2;
            break;
        default:
            return 0;
            break;
    }
}

/**
 * This function copies from a given string into a given buffer based on the input parameters.
 *
 * Receives char parameter pointer to pointer "buffer", char parameter pointer "string", int parameter "size".
 *
 * The function copies from "string" into "buffer" at most "size" amount of characters.
 * In addition "buffer" index is advanced by "size".
*/
static inline void copy_and_move_buffer(char** buffer, char* string, int size)
{
    strncpy(*buffer, string, size);
    *buffer += size;
}

/**
 * This function copies from a given string into a given buffer based on the input parameters.
 *
 * Receives char parameter pointer to pointer "buffer", char parameter pointer "string", int parameter "size".
 *
 * The function copies from "string" into "buffer" "size" amount of characters.
 * In addition "buffer" index is advanced by "MAX_CGROUP_FILE_NAME_LENGTH".
*/
static inline void copy_and_move_buffer_max_len(char** buffer, char* string)
{
    strncpy(*buffer, string, strlen(string));
    *buffer += MAX_CGROUP_FILE_NAME_LENGTH;
}

/**
 * This function copies from a give source into a given destination based on the input parameters.
 *
 * Receives char parameter pointer "destination", char parameter pointer "source", int parameter "var".
 *
 * The function copies from "source" into "destination" "var" amount of characters.
 * In addition "var" index is advanced by length of string "source".
*/
static inline void move_and_add(char* destination, char* source, int* var)
{
    memmove(destination + *var, source, strlen(source));
    *var += strlen(source);
}

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

static int find_procs_offsets(int * procoff, int * pidoff, struct file * f)
{
    *procoff = 0;
    *pidoff = f->off;
    while (*procoff < sizeof(f->cgp->proc) / sizeof(*f->cgp->proc)) {
        if (f->cgp->proc[*procoff] == 0) {
            *procoff += 1;
            continue;
        }
        int pid = proc_pid(f->cgp->proc[*procoff]);
        int pidlen = 1;
        while (pid > 0) {
            pidlen++;
            pid /= 10;
        }
        if (*pidoff >= pidlen) {
            *pidoff -= pidlen;
            *procoff += 1;
        } else
            break;
    }

    if (*procoff >= sizeof(f->cgp->proc) / sizeof(*f->cgp->proc))
        return -1;

    return 0;
}

static int copy_until_char(char * s, char * t, char ch, int n)
{
    int len = 0;
    while (*t != ch && *t != '\0' && (n--) > 0) {
        *s++ = *t++;
        len++;
    }

    *s = 0;
    if (*t == ch)
        len++;

    return len;
}

static int is_file_writable(cgroup_file_name_t filetype)
{
    /* Check that the file to be opened is one of the filesystem files and
    * set writeable accordingly.*/
   if(NON_WRITABLE < filetype && INVALID_TYPE > filetype) 
   {
       return 0;
   }
   else if(NON_WRITABLE > filetype && CG_FILE_NAME_START < filetype)
   {
       return 1;
   }

   return -1;
}

static cgroup_file_name_t get_file_name_constant(char * filename)
{
    if (strcmp(filename, CGFS_PROCS) == 0)
        return CGROUP_PROCS;
    else if (strcmp(filename, CGFS_SUBTREE_CONTROL) == 0)
        return CGROUP_SUBTREE_CONTROL;
    else if (strcmp(filename, CGFS_MAX_DESCENDANTS) == 0)
        return CGROUP_MAX_DESCENDANTS;
    else if (strcmp(filename, CGFS_MAX_DEPTH) == 0)
        return CGROUP_MAX_DEPTH;
    else if (strcmp(filename, CGFS_CONTROLLERS) == 0)
        return CGROUP_CONTROLLERS;
    else if (strcmp(filename, CGFS_EVENTS) == 0)
        return CGROUP_EVENTS;
    else if (strcmp(filename, CGFS_STAT) == 0)
        return CGROUP_STAT;
    else if (strcmp(filename, CGFS_CPU_WEIGHT) == 0)
        return CPU_WEIGHT;
    else if (strcmp(filename, CGFS_CPU_MAX) == 0)
        return CPU_MAX;
    else if (strcmp(filename, CGFS_CPU_STAT) == 0)
        return CPU_STAT;
    else if (strcmp(filename, CGFS_PID_MAX) == 0)
        return PID_MAX;
    else if (strcmp(filename, CGFS_PID_CUR) == 0)
        return PID_CUR;
    else if (strcmp(filename, CGFS_SET_CPU) == 0)
        return SET_CPU;
    else if (strcmp(filename, CGFS_SET_FRZ) == 0)
        return SET_FRZ;
    else if (strcmp(filename, CGFS_MEM_CUR) == 0)
      return MEM_CUR;
    else if (strcmp(filename, CGFS_MEM_MAX) == 0)
      return MEM_MAX;
    else if (strcmp(filename, CGFS_MEM_MIN) == 0)
        return MEM_MIN;
    else if (strcmp(filename, CGFS_MEM_STAT) == 0)
      return MEM_STAT;
    else if (strcmp(filename, CGFS_IO_STAT) == 0)
        return IO_STAT;

    return -1;
}

/**
 * This function opens a cgroup dir
 */
static int unsafe_cg_open_dir(char * dirname, struct cgroup * cgp, int omode)
{
    struct file * f;
    int fd = -1;

    if (*cgp->cgroup_dir_path == 0)
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
    f->writable = 0;
    f->cgp = cgp;
    *f->cgfilename = 0;

    cgp->ref_count++;
    return fd;
}

/**
 * This function opens a cgroup file
 */
static int unsafe_cg_open_file(char * filename, struct cgroup * cgp, int omode)
{
    int writable = 1;
    int fd = -1;
    struct file * f;
    cgroup_file_name_t filename_const = get_file_name_constant(filename);

    if(-1 == filename_const)
        return -1;

    writable = is_file_writable(filename_const);

      /* Allocate file structure and file desctiptor.*/
    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        return -1;
    }

    switch (filename_const) {
        case CPU_STAT:
            if (cgp == cgroup_root())
                return -1;
            f->cpu.stat.active = cgp->cpu_controller_enabled;
            f->cpu.stat.usage_usec = cgp->cpu_time;
            f->cpu.stat.user_usec = cgp->cpu_time;
            f->cpu.stat.system_usec = 0;
            f->cpu.stat.nr_periods = cgp->cpu_nr_periods;
            f->cpu.stat.nr_throttled = cgp->cpu_nr_throttled;
            f->cpu.stat.throttled_usec = cgp->cpu_throttled_usec;
            break;

        case CPU_WEIGHT:
            if (cgp == cgroup_root())
                return -1;
            f->cpu.weight.weight = 0;
            break;

        case CPU_MAX:
            if (cgp == cgroup_root())
                return -1;
            f->cpu.max.max = cgp->cpu_time_limit;
            f->cpu.max.period = cgp->cpu_account_period;
            break;

        case PID_MAX:
            if (cgp == cgroup_root())
                return -1;
            f->pid.max.active = cgp->pid_controller_enabled;
            f->pid.max.max = cgp->max_num_of_procs;
            break;

        case SET_CPU:
            if (cgp == cgroup_root())
                return -1;
            f->cpu_s.set.active = cgp->set_controller_enabled;
            f->cpu_s.set.cpu_id = cgp->cpu_to_use;
            break;

        case SET_FRZ:
            if (cgp == cgroup_root())
                return -1;
            f->frz.freezer.frozen = cgp->is_frozen;
            break;

        case MEM_MAX:
            if (cgp == cgroup_root())
            return -1;
            f->mem.max.active = cgp->mem_controller_enabled;
            f->mem.max.max = cgp->max_mem;
            break;

        case MEM_MIN:
            if (cgp == cgroup_root())
                return -1;
            f->mem.min.active = cgp->mem_controller_enabled;
            f->mem.min.min = cgp->min_mem;
            break;

        case MEM_STAT:
            if (cgp == cgroup_root())
                return -1;
            f->mem.stat.active = cgp->mem_controller_enabled;
            f->mem.stat.file_dirty = cgp->mem_stat_file_dirty;
            f->mem.stat.file_dirty_aggregated = cgp->mem_stat_file_dirty_aggregated;
            f->mem.stat.pgfault = cgp->mem_stat_pgfault;
            f->mem.stat.pgmajfault = cgp->mem_stat_pgmajfault;
            f->mem.stat.kernel = get_total_memory() * PGSIZE;
            break;

        case IO_STAT:
            if(cgp == cgroup_root())
                return -1;
            /* internally initialize the io related stats in the file structure */
            set_cgroup_io_stat(f);
            break;
        // for any other type we do nothing (no special handling)
        default:
            break;
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

int unsafe_cg_open(cg_file_type type, char * filename, struct cgroup * cgp, int omode)
{
    int fd = -1;

    if (type == CG_FILE)
    {
        fd = unsafe_cg_open_file(filename, cgp, omode);
    }
    else if (type == CG_DIR)
    {
        fd = unsafe_cg_open_dir(filename, cgp, omode);
    }

    return fd;
}


static int read_file_cg_procs(struct file * f, char * addr, int n)
{
    int procoff;
    int pidoff;
    int r = 0;

    if (find_procs_offsets(&procoff, &pidoff, f) < 0) {
        return 0;
    }

    while (procoff < (sizeof(f->cgp->proc) / sizeof(*f->cgp->proc)) &&
            r < n) 
    {
        if (f->cgp->proc[procoff] == 0) {
            procoff++;
            continue;
        }

        memset(buf,'\0',MAX_PID_LENGTH);
        int pidlength = itoa(buf, proc_pid(f->cgp->proc[procoff]));
        if (pidoff < pidlength) {
            *addr = buf[pidoff];
            pidoff++;
        } else {
            *addr = '\n';
            pidoff = 0;
            procoff++;
        }
        addr++;
        r++;
    }

    return r; 
}

static int read_file_cg_controllers(struct file * f, char * addr, int n)
{
    int i= 0;
    memset(buf,'\0',MAX_STR);

    if (f->cgp->cpu_controller_avalible) {
        move_and_add(buf, "cpu\n", &i);
    }
    if (f->cgp->pid_controller_avalible) {
        move_and_add(buf, "pid\n", &i);
    }
    if (f->cgp->set_controller_avalible) {
        move_and_add(buf, "set\n", &i);
    }
    if (f->cgp->mem_controller_avalible) {
        move_and_add(buf, "mem\n", &i);
    }

    return copy_buffer_up_to_end(buf + f->off, min(i, n), addr);
}

static int read_file_cg_subtree(struct file * f, char * addr, int n)
{
    int i = 0;
    memset(buf,'\0',MAX_STR);

    if (f->cgp->cpu_controller_enabled) {
        move_and_add(buf, "cpu\n", &i);
    }
    if (f->cgp->pid_controller_enabled) {
        move_and_add(buf, "pid\n", &i);
    }
    if (f->cgp->set_controller_enabled) {
        move_and_add(buf, "set\n", &i);
    }
    if (f->cgp->mem_controller_enabled) {
        move_and_add(buf, "mem\n", &i);
    }

    return copy_buffer_up_to_end(buf + f->off, min(i, n), addr);
}

static int read_file_cg_stat(struct file * f, char * addr, int n)
{
    char nr_descendants_buf[MAX_DECS_SIZE];
    memset(nr_descendants_buf,'\0',MAX_DECS_SIZE);
    itoa(nr_descendants_buf, f->cgp->nr_descendants);
    char nr_dying_descendants_buf[MAX_DECS_SIZE];
    memset(nr_dying_descendants_buf,'\0',MAX_DECS_SIZE);
    itoa(nr_dying_descendants_buf, f->cgp->nr_dying_descendants);

    char * stattext = buf;
    char * stattextp = stattext;

    copy_and_move_buffer(&stattextp, "nr_descendants - ", strlen("nr_descendants - "));
    copy_and_move_buffer(&stattextp, nr_descendants_buf, strlen(nr_descendants_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
    copy_and_move_buffer(&stattextp, "nr_dying_descendants - ", strlen("nr_dying_descendants - "));
    copy_and_move_buffer(&stattextp, nr_dying_descendants_buf, strlen(nr_dying_descendants_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(stattext + f->off, min(abs(stattextp - stattext - f->off), n), addr);
}

static int read_file_cpu_stat(struct  file * f, char * addr, int n)
{
    char usage_buf[11] = {0};
    char user_buf[11] = {0};
    char system_buf[11] = {0};
    char nr_periods_buf[11] = {0};
    char nr_throttled_buf[11] = {0};
    char throttled_usec_buf[11] = {0};
    char * stattext = buf;
    char * stattextp = stattext;

    itoa(usage_buf, f->cpu.stat.usage_usec);
    itoa(user_buf, f->cpu.stat.user_usec);
    itoa(system_buf, f->cpu.stat.system_usec);
    itoa(nr_periods_buf, f->cpu.stat.nr_periods);
    itoa(nr_throttled_buf, f->cpu.stat.nr_throttled);
    itoa(throttled_usec_buf, f->cpu.stat.throttled_usec);

    copy_and_move_buffer(&stattextp, "usage_usec - ", strlen("usage_usec - "));
    copy_and_move_buffer(&stattextp, usage_buf, strlen(usage_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
    copy_and_move_buffer(&stattextp, "user_usec - ", strlen("user_usec - "));
    copy_and_move_buffer(&stattextp, user_buf, strlen(user_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
    copy_and_move_buffer(&stattextp, "system_usec - ", strlen("system_usec - "));
    copy_and_move_buffer(&stattextp, system_buf, strlen(system_buf));

    if (f->cpu.stat.active) {
        copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
        copy_and_move_buffer(&stattextp, "nr_periods - ", strlen("nr_periods - "));
        copy_and_move_buffer(&stattextp, nr_periods_buf, strlen(nr_periods_buf));
        copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
        copy_and_move_buffer(&stattextp, "nr_throttled - ", strlen("nr_throttled - "));
        copy_and_move_buffer(&stattextp, nr_throttled_buf, strlen(nr_throttled_buf));
        copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
        copy_and_move_buffer(&stattextp, "throttled_usec - ", strlen("throttled_usec - "));
        copy_and_move_buffer(&stattextp, throttled_usec_buf, strlen(throttled_usec_buf));
    }

    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(stattext + f->off, min(abs(stattextp - stattext - f->off), n), addr);
}

static int read_file_cpu_weight(struct file * f, char * addr, int n)
{
    char tmp_num_buff[20] = {0};
    char * weighttext = buf;
    char *weighttextp = weighttext;
    copy_and_move_buffer(&weighttextp, "weight - ", strlen("weight - "));
    int num_str_length = utoa(tmp_num_buff, f->cpu.weight.weight);
    copy_and_move_buffer(&weighttextp, tmp_num_buff, num_str_length);
    copy_and_move_buffer(&weighttextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(weighttext + f->off, min(abs(weighttextp - weighttext - f->off), n), addr);
}

static int read_file_cpu_max(struct file * f, char * addr, int n)
{
    char tmp_num_buff[20] = {0};
    char * maxtext = buf;
    char *maxtextp = maxtext;
    copy_and_move_buffer(&maxtextp, "max - ", strlen("max - "));
    int num_str_length = utoa(tmp_num_buff, f->cpu.max.max);
    copy_and_move_buffer(&maxtextp, tmp_num_buff, num_str_length);
    copy_and_move_buffer(&maxtextp, "\nperiod - ", strlen("\nperiod - "));
    num_str_length = utoa(tmp_num_buff, f->cpu.max.period);
    copy_and_move_buffer(&maxtextp, tmp_num_buff, num_str_length);
    copy_and_move_buffer(&maxtextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(maxtext + f->off, min(abs(maxtextp - maxtext - f->off), n), addr);
}

static int read_file_pid_max(struct file *f, char * addr, int n)
{
    char max_buf[11] = {0};
    char * maxtext = buf;
    char * maxtextp = maxtext;

    itoa(max_buf, f->pid.max.max);

    copy_and_move_buffer(&maxtextp, "max - ", strlen("max - "));
    copy_and_move_buffer(&maxtextp, max_buf, strlen(max_buf));
    copy_and_move_buffer(&maxtextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(maxtext + f->off, min(abs(maxtextp - maxtext - f->off), n), addr);
}

static int read_file_pid_cur(struct file * f, char * addr, int n)
{
    char nr_of_procs_buf[MAX_DECS_SIZE] = {0};
    itoa(nr_of_procs_buf, f->cgp->num_of_procs);

    char * stattext = buf;
    char * stattextp = stattext;

    copy_and_move_buffer(&stattextp, "num_of_procs - ", strlen("num_of_procs - "));
    copy_and_move_buffer(&stattextp, nr_of_procs_buf, strlen(nr_of_procs_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(stattext + f->off, min(abs(stattextp - stattext - f->off), n), addr);
}

static int read_file_set_cpu(struct file * f, char * addr, int n)
{
    char cpu_buf[11] = {0};
    char * cputext = buf;
    char * cputextp = cputext;

    itoa(cpu_buf, f->cpu_s.set.cpu_id);

    copy_and_move_buffer(&cputextp, "use_cpu - ", strlen("use_cpu - "));
    copy_and_move_buffer(&cputextp, cpu_buf, strlen(cpu_buf));
    copy_and_move_buffer(&cputextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(cputext + f->off, min(abs(cputextp - cputext - f->off), n), addr);
}

static int read_file_set_frz(struct file * f, char * addr, int n)
{
    char frz_buf[11] = {0};
    char * frztext = buf;
    char * frztextp = frztext;

    itoa(frz_buf, f->frz.freezer.frozen);

    copy_and_move_buffer(&frztextp, frz_buf, strlen(frz_buf));
    copy_and_move_buffer(&frztextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(frztext + f->off, min(abs(frztextp - frztext - f->off), n), addr);
}

static int read_file_mem_cur(struct file * f, char * addr, int n)
{
    char cur_mem_buf[10] = {0};
    char * stattext = buf;
    char * stattextp = stattext;

    utoa(cur_mem_buf, f->cgp->current_mem);

    copy_and_move_buffer(&stattextp, cur_mem_buf, strlen(cur_mem_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(stattext + f->off, min(abs(stattextp - stattext - f->off), n), addr);
}

static int read_file_mem_max(struct file * f, char * addr, int n)
{
    char max_buf[10] = {0};
    char * maxtext = buf;
    char * maxtextp = maxtext;

    utoa(max_buf, f->mem.max.max);

    copy_and_move_buffer(&maxtextp, max_buf, strlen(max_buf));
    copy_and_move_buffer(&maxtextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(maxtext + f->off, min(abs(maxtextp - maxtext - f->off), n), addr);
}

static int read_file_mem_min(struct file * f, char * addr, int n)
{
    char max_buf[10] = { 0 };
    char* maxtext = buf;
    char* maxtextp = maxtext;

    utoa(max_buf, f->mem.min.min);

    copy_and_move_buffer(&maxtextp, max_buf, strlen(max_buf));
    copy_and_move_buffer(&maxtextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(maxtext + f->off, min(abs(maxtextp - maxtext - f->off), n), addr);
}

static int read_file_io_stat(struct file *f, char * addr, int n)
{
    char *stattext = buf;
    char *stattextp = stattext;
    uint stattext_size = min(n + f->off, sizeof(buf) / sizeof(char));
    cgroup_io_device_statistics_t * dev_stat = (void *)0;
    char rbytes_buff[8] = {0};
    char wbytes_buff[8] = {0};
    char rios_buff[8] = {0};
    char wios_buff[8] = {0};
    uint buff_length = 0;

    if(f == (void *)0 || f->cgp == (void *)0)
        panic("Can't read file io stat. file structure invalid (NULL)");

    /* set the io stats in the file structure (f->cgp should be already set with
        the io inodes, if there are any)
    */
    get_cgroup_io_stat(f, f->cgp);

    /* parse from file structure */
    memset(stattext, 0, stattext_size);

    for(int i = 0; i < NDEV; i ++)
    {
        dev_stat = (cgroup_io_device_statistics_t *)f->io.devices_stats[i];
        if(dev_stat == (void *)0)
            continue;

        copy_and_move_buffer(&stattextp, dev_stat->dev_name, strlen(dev_stat->dev_name));

        copy_and_move_buffer(&stattextp, " rbytes=", strlen(" rbytes="));
        buff_length = utoa(rbytes_buff, dev_stat->device_stats.rbytes);
        copy_and_move_buffer(&stattextp, rbytes_buff, buff_length);

        copy_and_move_buffer(&stattextp, " wbytes=", strlen(" wbytes="));
        buff_length = utoa(wbytes_buff, dev_stat->device_stats.wbytes);
        copy_and_move_buffer(&stattextp, wbytes_buff, buff_length);

        copy_and_move_buffer(&stattextp, " rios=", strlen(" rios="));
        buff_length = utoa(rios_buff, dev_stat->device_stats.rios);
        copy_and_move_buffer(&stattextp, rios_buff, buff_length);

        copy_and_move_buffer(&stattextp, " wios=", strlen(" wios="));
        buff_length = utoa(wios_buff, dev_stat->device_stats.wios);
        copy_and_move_buffer(&stattextp, wios_buff, buff_length);

        copy_and_move_buffer(&stattextp, "\n", strlen("\n"));
    }

    return copy_buffer_up_to_end(stattext + f->off, min(abs(stattextp - stattext - f->off), n), addr);
}

static int read_file_mem_stat(struct file * f, char * addr, int n)
{
    char file_dirty_buf[10] = {0};
    char file_dirty_aggregated_buf[10] = {0};
    char pgfault_buf[10] = {0};
    char pgmajfault_buf[10] = {0};
    char kernel_buf[10] = {0};

    uint stattext_size = strlen("file_dirty - ") +
            utoa(file_dirty_buf, f->mem.stat.file_dirty) + 1
            + strlen("file_dirty_aggregated - ") +
            utoa(file_dirty_aggregated_buf, f->mem.stat.file_dirty_aggregated) + 1 +
            strlen("pgfault - ") +
            utoa(pgfault_buf, f->mem.stat.pgfault) + 1 +
            strlen("pgmajfault - ") +
            utoa(pgmajfault_buf, f->mem.stat.pgmajfault) + 2 +
            strlen("kernel - ") +
            utoa(kernel_buf, f->mem.stat.kernel);

    char *stattext = buf;
    char *stattextp = stattext;
    memset(stattext, '\0', stattext_size);

    copy_and_move_buffer(&stattextp, "file_dirty - ", strlen("file_dirty - "));
    copy_and_move_buffer(&stattextp, file_dirty_buf, strlen(file_dirty_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    copy_and_move_buffer(&stattextp, "file_dirty_aggregated - ", strlen("file_dirty_aggregated - "));
    copy_and_move_buffer(&stattextp, file_dirty_aggregated_buf, strlen(file_dirty_aggregated_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    copy_and_move_buffer(&stattextp, "pgfault - ", strlen("pgfault - "));
    copy_and_move_buffer(&stattextp, pgfault_buf, strlen(pgfault_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    copy_and_move_buffer(&stattextp, "pgmajfault - ", strlen("pgmajfault - "));
    copy_and_move_buffer(&stattextp, pgmajfault_buf, strlen(pgmajfault_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    copy_and_move_buffer(&stattextp, "kernel -  ", strlen("kernel - "));
    copy_and_move_buffer(&stattextp, kernel_buf, strlen(kernel_buf));
    copy_and_move_buffer(&stattextp, "\n", strlen("\n"));

    return copy_buffer_up_to_end(stattext + f->off, min(abs(stattextp - stattext - f->off), n), addr);
}

static int read_file_cg_events(struct file * f, char * addr , int n)
{
    char eventstext[] = "populated - 0\nfrozen - 0\n";

    if (f->cgp->populated)
        eventstext[strlen("popluated - ")] = '1';

    if (f->cgp->is_frozen)
        eventstext[strlen(eventstext) - 2] = '1';

    return copy_buffer_up_to_end(eventstext + f->off, min(sizeof(eventstext), n), addr);
}

static int read_file_cg_max_descen(struct file * f, char * addr, int n)
{
    itoa(buf, f->cgp->max_descendants_value);
    return copy_buffer_up_to_end_replace_end_with_newline(buf, min(sizeof(buf), n), addr);
}

static int unsafe_cg_read_file(struct file * f, char * addr, int n)
{
        int r = 0;
        cgroup_file_name_t filename_const = get_file_name_constant(f->cgfilename);
        
        if (filename_const < 1 || f->readable == 0 || *f->cgp->cgroup_dir_path == 0)
            return -1;

        switch(filename_const)
        {
        case CGROUP_PROCS:
            r = read_file_cg_procs(f, addr, n);
            break;

        case CGROUP_CONTROLLERS:
            r = read_file_cg_controllers(f, addr, n);
            break;

        case CGROUP_SUBTREE_CONTROL:
            r = read_file_cg_subtree(f, addr, n);
            break;

        case CGROUP_EVENTS:
            r = read_file_cg_events(f, addr, n);
            break;

        case CGROUP_MAX_DESCENDANTS:
            r = read_file_cg_max_descen(f, addr, n);
            break;

        case CGROUP_MAX_DEPTH:
            memset(buf,'\0',MAX_DEPTH_SIZE);
            itoa(buf, f->cgp->max_depth_value);
            r = copy_buffer_up_to_end_replace_end_with_newline(buf, min(sizeof(buf), n), addr);
            break;
        
        case CGROUP_STAT:
            r = read_file_cg_stat(f, addr, n);
            break;
        
        case CPU_STAT:
            r = read_file_cpu_stat(f, addr, n);
            break;

        case CPU_WEIGHT:
            r = read_file_cpu_weight(f, addr, n);
            break;

        case CPU_MAX:
            r = read_file_cpu_max(f, addr, n);
            break;
        
        case PID_MAX:
            r = read_file_pid_max(f, addr, n);
            break;

        case PID_CUR:
            r = read_file_pid_cur(f, addr, n);
            break;

        case SET_CPU:
            r = read_file_set_cpu(f, addr, n);
            break;
        
        case SET_FRZ:
            r = read_file_set_frz(f, addr, n);
            break;

        case MEM_CUR:
            r = read_file_mem_cur(f, addr, n);
            break;

        case MEM_MAX:
            r = read_file_mem_max(f, addr, n);
            break;

        case MEM_MIN:
            r = read_file_mem_min(f, addr, n);
            break;

        case MEM_STAT:
            r = read_file_mem_stat(f, addr, n);
            break;

        case IO_STAT:
            r = read_file_io_stat(f, addr, n);
            break;
        // for any other file type we do nothing (no special handling)
        default:
            break;
        }
        // all the read_file functions doesn't return an error
        f->off += r;
        return r;
}


int unsafe_cg_read(cg_file_type type, struct file * f, char * addr, int n)
{
    int i = 0;

    if (type == CG_FILE){
        return unsafe_cg_read_file(f, addr, n);
   } else if (type == CG_DIR){

        memset(buf, '\0', MAX_CGROUP_FILE_NAME_LENGTH * MAX_CGROUP_DIR_ENTRIES);
        if (*f->cgp->cgroup_dir_path == 0)
            return -1;

        for (int i = 0; i < sizeof(buf); i++)
            buf[i] = ' ';

        char * bufp = buf;

        copy_and_move_buffer_max_len(&bufp, ".");

        if (f->cgp != cgroup_root()) {
            copy_and_move_buffer_max_len(&bufp, "..");
        }

        copy_and_move_buffer_max_len(&bufp, CGFS_PROCS);

        if (f->cgp != cgroup_root()) {
            copy_and_move_buffer_max_len(&bufp, CGFS_CONTROLLERS);
            copy_and_move_buffer_max_len(&bufp, CGFS_SUBTREE_CONTROL);
            copy_and_move_buffer_max_len(&bufp, CGFS_EVENTS);
            copy_and_move_buffer_max_len(&bufp, CGFS_SET_FRZ);
        }

        copy_and_move_buffer_max_len(&bufp, CGFS_MAX_DESCENDANTS);
        copy_and_move_buffer_max_len(&bufp, CGFS_MAX_DEPTH);
        copy_and_move_buffer_max_len(&bufp, CGFS_STAT);
//        copy_and_move_buffer_max_len(&bufp, "cgroup.current");
//        copy_and_move_buffer_max_len(&bufp, CGFS_MEM_CUR);

        if (f->cgp != cgroup_root()) {
            copy_and_move_buffer_max_len(&bufp, CGFS_MEM_CUR);
            copy_and_move_buffer_max_len(&bufp, CGFS_CPU_STAT);
            copy_and_move_buffer_max_len(&bufp, CGFS_MEM_STAT);
            copy_and_move_buffer_max_len(&bufp, CGFS_IO_STAT);

            if (f->cgp->cpu_controller_enabled) {
                copy_and_move_buffer_max_len(&bufp, CGFS_CPU_WEIGHT);
                copy_and_move_buffer_max_len(&bufp, CGFS_CPU_MAX);
            }

            if (f->cgp->pid_controller_enabled) {
                copy_and_move_buffer_max_len(&bufp, CGFS_PID_MAX);
            }

            if (f->cgp->set_controller_enabled) {
                copy_and_move_buffer_max_len(&bufp, CGFS_SET_CPU);
            }

            if (f->cgp->mem_controller_enabled) {
              copy_and_move_buffer_max_len(&bufp, CGFS_MEM_MAX);
              copy_and_move_buffer_max_len(&bufp, CGFS_MEM_MIN);
            }
        }

        get_cgroup_names_at_path(bufp, f->cgp->cgroup_dir_path);

        for (i = f->off; i < sizeof(buf) && i - f->off < n; i++) {
            *addr++ = buf[i];
        }

        i = i - f->off;
        f->off += i;
        return i;
    }

    return -1;
}

static int write_file_cg_procs(struct file * f, char * addr, int n)
{
    buf[n] = 0;
    strncpy(buf, addr, n);
    int pid = atoi(buf);
    if (pid <= 0 || cgroup_move_proc(f->cgp, pid) == -1)
        return -1;
    
    return n;
}

static int write_file_cg_subtree(struct file * f, char * addr, int n)
{
    char cpucontroller = 0; // change to 1 if need to enable, 2 if need
                            // to disable, 0 if nothing to change
    char pidcontroller = 0;
    char setcontroller = 0;
    char memcontroller = 0;
    char ch = ' ';
    int len = 0;
    int total_len = 0;

    while (*addr != '\0' && n > 0) {
        if (*addr != '+' && *addr != '-')
            return -1;

        memset(buf,'\0',MAX_CONTROLLER_NAME_LENGTH);
        len = copy_until_char(buf, addr + 1,ch, n - 1);
        if (strcmp(buf, "cpu") == 0) {
            cpucontroller = return_new_controller_state(*addr);
            addr += len + 1;
            total_len += len + 1;
        } else if (strcmp(buf, "pid") == 0) {
            pidcontroller = return_new_controller_state(*addr);
            addr += len + 1;
            total_len += len + 1;
        } else if (strcmp(buf, "set") == 0) {
            setcontroller = return_new_controller_state(*addr);
            addr += len + 1;
            total_len += len + 1;
        } else if (strcmp(buf, "mem") == 0) {
            memcontroller = return_new_controller_state(*addr);
            addr += len + 1;
            total_len += len + 1;
        } else
            return -1;
    }

    if (cpucontroller == 1 && unsafe_enable_cpu_controller(f->cgp) == CGROUP_RESULT_FAILURE)
        return -1;
    if (cpucontroller == 2 &&
            unsafe_disable_cpu_controller(f->cgp) < 0)
        return -1;

    if (pidcontroller == 1 && unsafe_enable_pid_controller(f->cgp) == CGROUP_RESULT_FAILURE)
        return -1;
    if (pidcontroller == 2 &&
            unsafe_disable_pid_controller(f->cgp) == CGROUP_RESULT_FAILURE)
        return -1;

    if (setcontroller == 1 && unsafe_enable_set_controller(f->cgp) == CGROUP_RESULT_FAILURE)
        return -1;
    if (setcontroller == 2 &&
        unsafe_disable_set_controller(f->cgp) < 0)
        return -1;

    if (memcontroller == 1 && unsafe_enable_mem_controller(f->cgp) == CGROUP_RESULT_FAILURE)
        return -1;
    if (memcontroller == 2 &&
        unsafe_disable_mem_controller(f->cgp) == CGROUP_RESULT_FAILURE)
        return -1;

    return n - total_len;
}

static int write_file_cg_max_descen(struct file * f, char * addr, int n)
{
    if (atoi(addr) < 0 || strlen(addr) > 2)
        return -1;
    f->cgp->max_descendants_value = atoi(addr);
    return n;
}

static int write_file_max_depth(struct file * f, char * addr, int n)
{
    if (atoi(addr) < 0 || strlen(addr) > 2)
        return -1;
    f->cgp->max_depth_value = atoi(addr);
    return  n;
}

static int write_file_cpu_max(struct file * f, char * addr, int n)
{
    char max_string[32] = {0};
    char period_string[32] = {0};
    int max = -1;
    int period = -1;
    int i = 0;

    //sh.c doesn't treat space inside for example: "1000 20000" as a single argument
    //so we will use special format, this also allows to parse zeroes inside a value

    while (*addr && *addr != ',' &&  *addr != '\0' && i < sizeof(max_string)) {
        max_string[i] = *addr;
        i++;
        addr++;
    }
    max_string[i] = '\0';
    i=0;
    addr++;

    while (*addr && *addr != '\0' && i < sizeof(period_string)) {
        period_string[i] = *addr;
        i++;
        addr++;
    }
    period_string[i] = '\0';

    // Update max.
    max = atoi(max_string);
    if (-1 == max) {
        return -1;
    }

    // Update period.
    if (period_string[0]) {
        period = atoi(period_string);
        if (-1 == period) {
            return -1;
        }

        // Update period fields.
        f->cgp->cpu_account_period = period;
        f->cpu.max.period = period;
    }

    // Update max fields.
    f->cgp->cpu_time_limit = max;
    f->cpu.max.max = max;

    return n;
}

static int write_file_pid_max(struct file * f, char * addr, int n)
{
    char max_string[32] = {0};
        int max = -1;
        int i = 0;

        while (*addr && *addr != ',' &&  *addr != '\0' && i < sizeof(max_string)) {
            max_string[i] = *addr;
            i++;
            addr++;
        }
        max_string[i] = '\0';
        i = 0;

        // Update max.
        max = atoi(max_string);
        if (-1 == max) {
            return -1;
        }

        // Update max pids field if the paramter is within allowed values.
        result_t test = set_max_procs(f->cgp, max);
        if (test != CGROUP_RESULT_SUCCESS_OPERATION)
            return -1;
        f->pid.max.max = max;

        return n;
}

static int write_file_set_cpu(struct file * f, char * addr, int n)
{
    char set_string[32] = { 0 };
    int set = -1;
    int i = 0;

    while (*addr && *addr != ',' &&  *addr != '\0' && i < sizeof(set_string)) {
        set_string[i] = *addr;
        i++;
        addr++;
    }
    set_string[i] = '\0';
    i = 0;
    addr++;

    // Update set parameter.
    set = atoi(set_string);
    if (-1 == set) {
        return -1;
    }

    // Update cpu id field if the paramter is within allowed values.
    result_t test = set_cpu_id(f->cgp, set);
    if (test != CGROUP_RESULT_SUCCESS_OPERATION)
        return -1;
    f->cpu_s.set.cpu_id = set;

    return  n;
}

static int write_file_set_frz(struct file * f, char * addr, int n)
{
    char set_string[32] = { 0 };
    int set_freeze = -1;
    int i = 0;

    while (*addr && *addr != ',' &&  *addr != '\0' && i < sizeof(set_string)) {
        set_string[i] = *addr;
        i++;
        addr++;
    }
    set_string[i] = '\0';
    i = 0;
    addr++;

    // Update freeze parameter.
    set_freeze = atoi(set_string);
    if (-1 == set_freeze) {
        return -1;
    }

    // Update is_frozen field if the paramter is within allowed values.
    result_t test = frz_grp(f->cgp, set_freeze);
    if (test != CGROUP_RESULT_SUCCESS_OPERATION)
        return -1;
    f->frz.freezer.frozen = set_freeze;

    return n;
}

static int write_file_mem_max(struct file * f, char * addr, int n)
{
    char max_string[32] = { 0 };
    unsigned int max = -1;
    int i = 0;

    while (*addr && *addr != ',' && *addr != '\0' && i < sizeof(max_string)) {
        max_string[i] = *addr;
        i++;
        addr++;
    }
    max_string[i] = '\0';
    i = 0;

    // Update max.
    max = atoi(max_string);
    if (-1 == max) {
        return -1;
    }

    // Update max memory field if the paramter is within allowed values.
    result_t test = set_max_mem(f->cgp, max);
    if (test != CGROUP_RESULT_SUCCESS_OPERATION)
        return -1;
    f->mem.max.max = max;

    return n;
}

static int write_file_mem_min(struct file * f, char * addr, int n)
{
    char min_string[32] = { 0 };
    unsigned int min = -1;
    int i = 0;

    while (*addr && *addr != ',' && *addr != '\0' && i < sizeof(min_string)) {
        min_string[i] = *addr;
        i++;
        addr++;
    }
    min_string[i] = '\0';
    i = 0;

    // Update min.
    min = atoi(min_string);
    if (-1 == min) {
        return -1;
    }

    // Update min memory field if the paramter is within allowed values.
    result_t test = set_min_mem(f->cgp, min);
    if (test != CGROUP_RESULT_SUCCESS_OPERATION)
        return -1;
    f->mem.min.min = min;
    return  n;
}

int unsafe_cg_write(struct file * f, char * addr, int n)
{
    int r = 0;
    cgroup_file_name_t filename_const = get_file_name_constant(f->cgfilename);

    if (f->writable == 0 || *f->cgp->cgroup_dir_path == 0 || n > MAX_BUF)
        return -1;
    
    if (filename_const == CGROUP_PROCS)
    {
        r = write_file_cg_procs(f, addr, n);
    }
    else if (filename_const == CGROUP_SUBTREE_CONTROL) {
        r = write_file_cg_subtree(f, addr, n);
    }
    else if (filename_const == CGROUP_MAX_DESCENDANTS) {
        r = write_file_cg_max_descen(f, addr, n);
    }
    else if (filename_const == CGROUP_MAX_DEPTH) {
        r = write_file_max_depth(f, addr, n);
    }
    else if (filename_const == CPU_MAX && f->cgp->cpu_controller_enabled){
        r = write_file_cpu_max(f, addr, n);
    }
    else if (filename_const == PID_MAX && f->cgp->pid_controller_enabled){
        r = write_file_pid_max(f, addr, n);
    }
    else if (filename_const == SET_CPU && f->cgp->set_controller_enabled) {
        r = write_file_set_cpu(f, addr, n);
    }
    else if (filename_const == SET_FRZ) {
        r = write_file_set_frz(f, addr, n);
    }
    else if (filename_const == MEM_MAX && f->cgp->mem_controller_enabled) {
        r = write_file_mem_max(f, addr, n);
    }
    else if (filename_const == MEM_MIN && f->cgp->mem_controller_enabled) {
        r = write_file_mem_min(f, addr, n);
    }

    return r;
}

int unsafe_cg_close(struct file * file)
{
    fileclose(file);
    file->cgp->ref_count--;
    if (file->cgp->ref_count == 0 && *file->cgp->cgroup_dir_path == 0)
        decrement_nr_dying_descendants(file->cgp->parent);
    return 0;
}

int get_base_name(char * path, char * base_name)
{
    char fpath[MAX_PATH_LENGTH] = { 0 };
    format_path(fpath, path);

    char * file_name_temp = fpath;
    char * temp = fpath;
    while (*temp != '\0') {
        if (*temp == '/')
            file_name_temp = temp;
        temp++;
    }

    if (file_name_temp == fpath || file_name_temp == temp - 1)
        return -1;

    temp = file_name_temp + 1;
    while (*temp != '\0')
        *base_name++ = *temp++;

    *base_name = 0;

    return 0;
}

int get_dir_name(char * path, char * dir_name)
{
    char fpath[MAX_PATH_LENGTH] = {0};
    format_path(fpath, path);

    char * file_name_temp = fpath;
    char * temp = fpath;
    while (*temp != '\0') {
        if (*temp == '/')
            file_name_temp = temp;
        temp++;
    }

    if (file_name_temp == fpath || file_name_temp == temp - 1)
        return -1;

    temp = fpath;
    while (temp < file_name_temp)
        *dir_name++ = *temp++;

    *dir_name = 0;

    return 0;
}

static int cg_file_size(struct file * f)
{
    int size = 0;
    int filename_const = get_file_name_constant(f->cgfilename);

    if (filename_const == CGROUP_PROCS) {
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
    } else if (filename_const == CGROUP_CONTROLLERS) {
        if (f->cgp->cpu_controller_avalible)
            size += 3;
        if (f->cgp->pid_controller_avalible)
            size += 3;
    } else if (filename_const == CGROUP_SUBTREE_CONTROL) {
        if (f->cgp->cpu_controller_enabled)
            size += 3;
        if (f->cgp->pid_controller_enabled)
            size += 3;
    } else if (filename_const == CGROUP_EVENTS) {
        size += strlen("populated - 0");
    } else if (filename_const == CGROUP_MAX_DESCENDANTS) {
        size += intlen(f->cgp->max_descendants_value);
    } else if (filename_const == CGROUP_MAX_DEPTH) {
        size += intlen(f->cgp->max_depth_value);
    } else if (filename_const == CGROUP_STAT) {
        size += strlen("nr_descendants - ") +
            intlen(f->cgp->nr_descendants) + strlen("\n") +
            strlen("nr_dying_descendants - ") +
            intlen(f->cgp->nr_dying_descendants) + strlen("\n") +
            strlen("nr_of_procs - ") +
            intlen(f->cgp->num_of_procs) + strlen("\n");
    } else if (filename_const == PID_CUR) {
        size += strlen("num_of_procs - ") +
            intlen(f->cgp->num_of_procs) + strlen("\n");
    } else if (filename_const == MEM_CUR) {
        size += strlen("cur_mem_in_bytes - ") +
            intlen(f->cgp->current_mem) + strlen("\n");
    }

    return size;
}

int unsafe_cg_stat(struct file * f, struct stat * st)
{
    if (*f->cgp->cgroup_dir_path == 0)
        return -1;
    if (*f->cgfilename == 0) {
        st->type = T_CGDIR;
        st->size = MAX_CGROUP_FILE_NAME_LENGTH *
                   (7 + cgorup_num_of_immidiate_children(f->cgp));
    } else {
        st->type = T_CGFILE;
        st->size = cg_file_size(f);
    }
    return 0;
}
