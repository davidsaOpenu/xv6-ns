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

static int strcmp(const char * p, const char * q)
{
    while (*p && *p == *q)
        p++, q++;
    return (uchar)*p - (uchar)*q;
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

static int itoa(char * buf, int n)
{
    int i = n;
    int length = 0;

    while (i > 0) {
        length++;
        i /= 10;
    }

    char revbuf[length];

    if (n == 0) {
        *buf++ = '0';
    }
    for (i = 0; n > 0 && i < length; i++) {
        revbuf[i] = (n % 10) + '0';
        n /= 10;
    }
    while (--i >= 0) {
        *buf++ = revbuf[i];
    }
    *buf = '\0';
    return length;
}

static int atoi(char * str)
{
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] < '0' || str[i] > '9')
            return -1;
        res = res * 10 + str[i] - '0';
    }
    return res;
}

static int findprocsoffsets(int * procoff, int * pidoff, struct file * f)
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

static int copyuntilspace(char * s, char * t, int n)
{
    int len = 0;

    while (*t != ' ' && *t != '\0' && (n--) > 0) {
        *s++ = *t++;
        len++;
    }

    *s = 0;
    if (*t == ' ')
        len++;

    return len;
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
    f->ip = 0;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = ((omode & O_WRONLY) || (omode & O_RDWR)) && writable;
    f->cgp = cgp;
    strncpy(f->cgfilename, filename, sizeof(f->cgfilename));
    f->mnt = 0;
	
	cgp->ref_count++;

    return fd;
}

int unsafe_opencgdirectory(struct cgroup * cgp, int omode)
{
    struct file * f;
    int fd;

	if(*cgp->cgroup_dir_path == 0)
		return -1;

    /* Allocate file structure and file desctiptor.*/
    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        return -1;
    }

    f->type = FD_CG;
    f->ip = 0;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = 0;
    f->cgp = cgp;
    *f->cgfilename = 0;
    f->mnt = 0;
	
	cgp->ref_count++;

    return fd;
}

int unsafe_readcgfile(struct file * f, char * addr, int n)
{
    int r = 0;

    if (f->readable == 0 || *f->cgp->cgroup_dir_path == 0)
        return -1;

    if (strcmp(f->cgfilename, "cgroup.procs") == 0) {
        int procoff;
        int pidoff;
        if (findprocsoffsets(&procoff, &pidoff, f) < 0) {
            return 0;
        }

        while (procoff < (sizeof(f->cgp->proc) / sizeof(*f->cgp->proc)) &&
               r < n) {
            if (f->cgp->proc[procoff] == 0) {
                procoff++;
                continue;
            }

            char buf[MAX_PID_LENGTH];
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
        if (procoff == (sizeof(f->cgp->proc) / sizeof(*f->cgp->proc)) &&
            *(addr - 1) == '\n')
            *(addr - 1) = '\0';
    }

    if (strcmp(f->cgfilename, "cgroup.controllers") == 0) {
        if (f->cgp->cpu_controller_avalible) {
            char controllerslist[] = "cpu";
			int listlen = strlen(controllerslist) + 1;
            while (r < n && (r + f->off) < listlen &&
                   (*addr++ = controllerslist[(r++) + f->off]) != 0)
                ;
        }
    }

    if (strcmp(f->cgfilename, "cgroup.subtree_control") == 0) {
        if (f->cgp->cpu_controller_enabled) {
            char enabledcontrollerslist[] = "cpu";
			int listlen = strlen(enabledcontrollerslist) + 1;
            while (r < n &&
                   (r + f->off) < listlen &&
                   (*addr++ = enabledcontrollerslist[r++ + f->off]) != 0)
                ;
        }
    }

    if (strcmp(f->cgfilename, "cgroup.events") == 0) {
        char eventstext[] = "populated - 0";
        if (f->cgp->populated)
            eventstext[strlen(eventstext) - 1] = '1';
		
		int listlen = strlen(eventstext) + 1;
        while (r < n && (r + f->off) < listlen &&
               (*addr++ = eventstext[r++ + f->off]) != 0)
            ;
    }

    if (strcmp(f->cgfilename, "cgroup.max.descendants") == 0) {
        while (r < n &&
               (r + f->off) < sizeof(f->cgp->max_descendants_value) &&
               (*addr++ = f->cgp->max_descendants_value[r++ + f->off]) !=
                   0)
            ;
    }

    if (strcmp(f->cgfilename, "cgroup.max.depth") == 0) {
        while (r < n && (r + f->off) < sizeof(f->cgp->max_depth_value) &&
               (*addr++ = f->cgp->max_depth_value[r++ + f->off]) != 0)
            ;
    }

    if (strcmp(f->cgfilename, "cgroup.stat") == 0) {
        char stattext[strlen("nr_descendants - ") +
                      strlen(f->cgp->nr_descendants) + strlen("\n") +
                      strlen("nr_dying_descendants - ") +
                      strlen(f->cgp->nr_dying_descendants) + 1];
        char * stattextp = stattext;
        strncpy(stattextp, "nr_descendants - ", sizeof("nr_descendants - "));
        stattextp += strlen("nr_descendants - ");
        strncpy(stattextp, f->cgp->nr_descendants, sizeof(f->cgp->nr_descendants));
        stattextp += strlen(f->cgp->nr_descendants);
        strncpy(stattextp, "\n", sizeof("\n"));
        stattextp += strlen("\n");
        strncpy(stattextp, "nr_dying_descendants - ", sizeof("nr_dying_descendants - "));
        stattextp += strlen("nr_dying_descendants - ");
        strncpy(stattextp, f->cgp->nr_dying_descendants, sizeof(f->cgp->nr_dying_descendants));

        while (r < n && (r + f->off) < sizeof(stattext) &&
               (*addr++ = stattext[r++ + f->off]) != 0)
            ;
    }

    f->off += r;

    return r;
}

int unsafe_readcgdirectory(struct file * f, char * addr, int n)
{
    char buf[MAX_CGROUP_FILE_NAME_LENGTH * MAX_CGROUP_DIR_ENTRIES];
	
	if(*f->cgp->cgroup_dir_path == 0)
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

int unsafe_writecgfile(struct file * f, char * addr, int n)
{
    int r = 0;

    if (f->writable == 0 || *f->cgp->cgroup_dir_path == 0)
        return -1;

    if (strcmp(f->cgfilename, "cgroup.procs") == 0) {
        char buf[n];
        strncpy(buf, addr, n);
        int pid = atoi(buf);
        if (pid <= 0)
            panic("writecgfile: invalid pid");
        cgroup_move_proc(f->cgp, pid);
        r = n;
    }

    if (strcmp(f->cgfilename, "cgroup.subtree_control") == 0) {
        char cpucontroller = 0; // change to 1 if need to enable, 2 if need
                                // to disable, 0 if nothing to change

        while (*addr != '\0' && n > 0) {
            if (*addr != '+' && *addr != '-')
                panic(
                    "writecgfile: invalid subtree_control commands list");

            char buf[MAX_CONTROLLER_NAME_LENGTH];
            int len = copyuntilspace(buf, addr + 1, n - 1);
            if (strcmp(buf, "cpu") == 0) {
                if (*addr == '+')
                    cpucontroller = 1;
                if (*addr == '-')
                    cpucontroller = 2;
                addr += len + 1;
                n -= len + 1;
            } else
                panic(
                    "writecgfile: invalid subtree_control commands list");
        }

        if (cpucontroller == 1 && unsafe_enable_cpu_controller(f->cgp) < 0)
            panic("writecgfile: cannot enable cpu controller");
        if (cpucontroller == 2 && unsafe_disable_cpu_controller(f->cgp) < 0)
            panic("writecgfile: cannot disable cpu controller");

        r = n;
    }

    if (strcmp(f->cgfilename, "cgroup.max.descendants") == 0) {
        if (atoi(addr) < 0 || strlen(addr) > 2)
            panic("writecgfile: invalid argument");
        strncpy(f->cgp->max_descendants_value, addr, sizeof(f->cgp->max_descendants_value));
        r = n;
    }

    if (strcmp(f->cgfilename, "cgroup.max.depth") == 0) {
        if (atoi(addr) < 0 || strlen(addr) > 2)
            panic("writecgfile: invalid argument");
        strncpy(f->cgp->max_depth_value, addr, sizeof(f->cgp->max_descendants_value));
        r = n;
    }

    return r;
}

int unsafe_closecgfileordir(struct file *file)
{
	fileclose(file);
	file->cgp->ref_count--;
	if(file->cgp->ref_count == 0 && *file->cgp->cgroup_dir_path == 0)
		decrement_nr_dying_descendants(file->cgp->parent);
	return 0;
}

int get_cg_file_dir_path_and_file_name(char * path,
                                       char * dir_path,
                                       char * file_name)
{
	char fpath[MAX_PATH_LENGTH];
	format_path(fpath, path);
    char * file_name_temp = fpath;
    char * temp = fpath;
    while (*temp != '\0') {
        if (*temp == '/')
            file_name_temp = temp;
        temp++;
    }

    if (file_name_temp == fpath || file_name_temp == fpath + 1)
        return -1;

    temp = file_name_temp + 1;
    while (*temp != '\0')
        *file_name++ = *temp++;

    temp = fpath;
    while (temp < file_name_temp)
        *dir_path++ = *temp++;

    *file_name = 0;
    *dir_path = 0;

    return 0;
}

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
    }

    if (strcmp(f->cgfilename, "cgroup.controllers") == 0) {
        if (f->cgp->cpu_controller_avalible)
            size += 3;
    }

    if (strcmp(f->cgfilename, "cgroup.subtree_control") == 0) {
        if (f->cgp->cpu_controller_enabled)
            size += 3;
    }

    if (strcmp(f->cgfilename, "cgroup.events") == 0) {
        size += strlen("populated - 0");
    }

    if (strcmp(f->cgfilename, "cgroup.max.descendants") == 0) {
        size += strlen(f->cgp->max_descendants_value);
    }

    if (strcmp(f->cgfilename, "cgroup.max.depth") == 0) {
        size += strlen(f->cgp->max_depth_value);
    }

    if (strcmp(f->cgfilename, "cgroup.stat") == 0) {
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


