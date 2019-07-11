// Cgroup filesysytem functions 
#include "cgfs.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

#define MAX_PID_LENGTH 5


static int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

static char*
strcpyn(char *s, char *t, int n)
{
  char *os;

  os = s;
  if(n == 0)
	while((*s++ = *t++) != 0)
		;
  else
	while((*s++ = *t++) != 0 && (--n) > 0)
		;
  return os;
}

static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

static int 
itoa(char *buf, int n)
{
	char revbuf[sizeof(buf)];
	int i;
	int length = 0;
	if(n == 0){
	    *buf++ = '0';
		length++;
	}
	for(i = 0; n > 0 && i < sizeof(buf); i++){
		revbuf[i] = (n % 10) + '0';
		n /= 10;
	}
    while(--i >= 0){
		*buf++ = revbuf[i];
		length++;
	}
	*buf = '\0';
	return length;
}

static int 
atoi(char* str) 
{ 
    int res = 0;
	for (int i = 0; str[i] != '\0'; ++i) {
		if (str[i] < '0' || str[i] > '9')
			return -1 ;
		res = res * 10 + str[i] - '0';
	}
    return res; 
} 

static int
findprocsoffsets(int *procoff, int *pidoff, struct file *f)
{
	*procoff = 0;
	*pidoff = f->off;
	while(*procoff < sizeof(f->cgp->proc) / sizeof(*f->cgp->proc)){
		if(f->cgp->proc[*procoff] == 0){
			*procoff += 1;
			continue;
		}
		int pid = proc_pid(f->cgp->proc[*procoff]);
		int pidlen = 1;
		while(pid > 0){
			pidlen++;
			pid /= 10;
		}
		if(*pidoff >= pidlen){
			*pidoff -= pidlen;
			*procoff += 1;
		}else
			break;
	}
	
	if (*procoff >= sizeof(f->cgp->proc) / sizeof(*f->cgp->proc))
		return -1;

	return 0;
}

static int
copyuntilspace(char *s, char *t,int n)
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




int opencgfile(char *filename, struct cgroup *cgp, int omode)
{
	
	 struct file *f;
	 int fd;
	 char writable;
	 
	/* Check that the file to be opened is one of the filesystem files and set writeable accordingly.*/
	if(strcmp(filename, "cgroup.procs") == 0 || strcmp(filename, "cgroup.subtree_control") == 0 || strcmp(filename, "cgroup.max.descendants") == 0 || strcmp(filename, "cgroup.max.depth") == 0)
		writable = 1;
	else
		if(strcmp(filename, "cgroup.controllers") == 0 || strcmp(filename, "cgroup.events") == 0 || strcmp(filename, "cgroup.stat") == 0)
			writable = 0;
		else
			return -1;
	
	/* Allocate file structure and file desctiptor.*/
	if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
		if(f)
			fileclose(f);
		return -1;
	}
	
	f->type = FD_CG;
	f->ip = 0;
	f->off = 0;
	f->readable = !(omode & O_WRONLY);
	f->writable = ((omode & O_WRONLY) || (omode & O_RDWR)) && writable;
	f->cgp = cgp;
	strcpyn(f->cgfilename, filename, 0);
	f->mnt = 0;
	
	return fd;
}

int readcgfile(struct file *f, char *addr, int n)
{
	int r = 0;
	
	if(f->readable == 0)
		return -1;
	
	if(strcmp(f->cgfilename, "cgroup.procs") == 0){
		int procoff;
		int pidoff;
		if(findprocsoffsets(&procoff, &pidoff, f) < 0){
			*addr = '\0';
			return 1;
		}
		
		while(procoff < (sizeof(f->cgp->proc) / sizeof(*f->cgp->proc)) && r < n){
			if(f->cgp->proc[procoff] == 0){
				procoff++;
				continue;				
			}
					
			char buf[MAX_PID_LENGTH];	
			int pidlength = itoa(buf, proc_pid(f->cgp->proc[procoff]));
			if(pidoff < pidlength){
				*addr = buf[pidoff];
				pidoff ++;
			}else{
				*addr = '\n';
				pidoff = 0;
				procoff++;
			}			
			addr++;
			r++;
		}
		*addr = '\0';
		r++;
	}
	
	if(strcmp(f->cgfilename, "cgroup.controllers") == 0){
		if(f->cgp->cpu_controller_avalible){
			char controllerslist[] = "cpu";
			while(r < n && (r + f->off) < sizeof(controllerslist)){
				*addr++ = controllerslist[r + f->off];
				r++;
			}
			r--;
		}
	}
	
	if(strcmp(f->cgfilename, "cgroup.subtree_control") == 0){
		if(f->cgp->cpu_controller_enabled){
			char enabledcontrollerslist[] = "cpu";
			while(r < n && (r + f->off) < sizeof(enabledcontrollerslist)){
				*addr++ = enabledcontrollerslist[r + f->off];
				r++;
			}
			r--;
		}
	}
	
	if(strcmp(f->cgfilename, "cgroup.events") == 0){
		char eventstext[] = "populated - 0";
		if(f->cgp->populated)
			eventstext[sizeof(eventstext) - 2] = '1';
			
		while(r < n && (r + f->off) < sizeof(eventstext)){
				*addr++ = eventstext[r + f->off];
				r++;
		}
		r--;
	}
	
	if(strcmp(f->cgfilename, "cgroup.max.descendants") == 0){
		while(r < n && (r + f->off) < sizeof(f->cgp->max_descendants_value) && f->cgp->max_descendants_value[r + f->off] != 0){
				*addr++ = f->cgp->max_descendants_value[r + f->off];
				r++;
		}
		*addr = '\0';
	}
	
	if(strcmp(f->cgfilename, "cgroup.max.depth") == 0){
		while(r < n && (r + f->off) < sizeof(f->cgp->max_depth_value) && f->cgp->max_depth_value[r + f->off] != 0){
				*addr++ = f->cgp->max_depth_value[r + f->off];
				r++;
		}
		*addr = '\0';
	}
	
	if(strcmp(f->cgfilename, "cgroup.stat") == 0){
		int  blank_spaces_desc = (*(f->cgp->nr_descendants + 1) ? 1 : 2); /*Get the number of '\0' characters in nr_descendants */
		int  blank_spaces_dying_desc = (*(f->cgp->nr_dying_descendants + 1) ? 1 : 2); /*Get the number of '\0' characters in nr_dying_descendants */
		char stattext[(sizeof("nr_descendants - ") - 1) + (sizeof(f->cgp->nr_descendants) - blank_spaces_desc) + (sizeof("\n") - 1) + (sizeof("nr_dying_descendants - ") - 1) + (sizeof(f->cgp->nr_dying_descendants) - blank_spaces_dying_desc) + 1];
		char *stattextp = stattext;
		strcpyn(stattextp, "nr_descendants - ", 0);
		stattextp += sizeof("nr_descendants - ") - 1;
		strcpyn(stattextp, f->cgp->nr_descendants, 0);
		stattextp += sizeof(f->cgp->nr_descendants) - blank_spaces_desc;
		strcpyn(stattextp, "\n", 0);
		stattextp += sizeof("\n") - 1;
		strcpyn(stattextp, "nr_dying_descendants - ", 0);
		stattextp += sizeof("nr_dying_descendants - ") - 1;
		strcpyn(stattextp, f->cgp->nr_dying_descendants, 0);
		
		while(r < n && (r + f->off) < sizeof(stattext) && stattext[r + f->off] != 0){
				*addr++ = stattext[r + f->off];
				r++;
		}
		*addr = '\0';
	}
	
	f->off += r;

	if (r == 0 && n > 0)
	{
		*addr = '\0';
		r++;
	}

	return r;
}

int writecgfile(struct file *f, char *addr, int n)
{
	int r = 0;
	
	if (f->writable == 0)
		return -1;
	
	if(strcmp(f->cgfilename, "cgroup.procs") == 0){
		char buf[n];
		strcpyn(buf, addr, n);
		int pid = atoi(buf);
		if(pid <= 0)
			panic("writecgfile: invalid pid");
		cgroup_move_proc(f->cgp, pid);
		r = n;
	}
	
	if(strcmp(f->cgfilename, "cgroup.subtree_control") == 0){
		char cpucontroller = 0; //change to 1 if need to enable, 2 if need to disable, 0 if nothing to change
	
		while(*addr != '\0' && n > 0){
			if (*addr != '+' && *addr != '-')
				panic("writecgfile: invalid subtree_control commands list");

			char buf[MAX_CONTROLLER_NAME_LENGTH];
			int len = copyuntilspace(buf, addr + 1, n - 1);
			if (strcmp(buf, "cpu") == 0) {
				if (*addr == '+') 
					cpucontroller = 1;
				if(*addr == '-')
					cpucontroller = 2;
				addr += len + 1;
				n -= len + 1;
			}else
				panic("writecgfile: invalid subtree_control commands list");
		}
		
		if(cpucontroller == 1 && enable_cpu_controller(f->cgp) < 0)
				panic("writecgfile: cannot enable cpu controller");
		if (cpucontroller == 2 && disable_cpu_controller(f->cgp) < 0)
				panic("writecgfile: cannot disable cpu controller");
				
		r = n;
	}
	
	if(strcmp(f->cgfilename, "cgroup.max.descendants") == 0){
		if (atoi(addr) < 0)
			panic("writecgfile: invalid argument");
		strcpyn(f->cgp->max_descendants_value, addr, n);
		r = n;
	}
	
	if(strcmp(f->cgfilename, "cgroup.max.depth") == 0){
		if (atoi(addr) < 0)
			panic("writecgfile: invalid argument");
		strcpyn(f->cgp->max_depth_value, addr, n);
		r = n;
	}
	
	return r;
}

int get_cg_file_dir_path_and_file_name(char* path, char* dir_path, char* file_name)
{
	char *file_name_temp = path;
	char *temp = path;
	while(*temp != '\0'){
		if(*temp == '/')
			file_name_temp = temp;
		temp++;		
	}
	
	if(file_name_temp == path || file_name_temp == path + 1)
		return -1;
	
	temp = file_name_temp + 1;
	while(*temp != '\0')
		*file_name++ = *temp++;
	
	temp = path;
	while(temp < file_name_temp)
		*dir_path++ = *temp++;	
	
	*file_name = 0;
	*dir_path = 0;
	
	return 0;
}
