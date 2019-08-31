#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "ns_types.h"

char *argv[] = { "sh", 0 };

typedef enum p_cmd {START, CONNECT, DISCONNECT, DESTROY, LIMIT, INFO} p_cmd;
#define CNTNAMESIZE 100
#define CNTARGSIZE 30

static int pouch_fork(char* container_name, int daemonize);
static int pouch_cmd(char* container_name,enum p_cmd, int daemonize);
static int find_tty(char* tty_name);
static int write_to_config(char* container_name, char* tty_name, int pid);
static int read_from_config(char* container_name, char* tty_name, int* pid);
static int init_pouch_cgroup();
static int create_pouch_cgroup(char* cg_cname);
static int pouch_limit_cgroup(char* container_name, char* cgroup_state_obj, char* limitation);
static int print_cinfo(char* container_name, char * tty_name, int pid);

void panic(char *s);

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit(1);
}

static int pouch_limit_cgroup(char* container_name, char* cgroup_state_obj, char * limitation){

    char cg_limit_cname[256];

    strcpy(cg_limit_cname,"/cgroup/");
    strcat(cg_limit_cname, container_name);
    strcat(cg_limit_cname, "/");
    strcat(cg_limit_cname,cgroup_state_obj);

    int cpu_max_fd = open(cg_limit_cname, O_RDWR);
    if(cpu_max_fd < 0)
        return -1;
    if(write(cpu_max_fd, limitation, sizeof(limitation)) < 0)
        return -1;
    if(close(cpu_max_fd) < 0)
        return -1;
    return 0;
}

static int pouch_cmd(char* container_name, enum p_cmd cmd, int daemonize){
        int tty_fd;
        int pid;
        char tty_name[10];

        if(cmd == START){
           return pouch_fork(container_name, daemonize);
        }

        if(read_from_config(container_name, tty_name, &pid) < 0){
           return -1;
        }

        if(cmd == INFO){
            if(print_cinfo(container_name, tty_name, pid) <0){
                return -1;
            }
            return 0;
        }


        if(cmd == DESTROY && pid != 0){
           if(kill(pid) < 0)
                return -1;
           unlink(container_name);
           return 0;
        }

        if((tty_fd = open(tty_name, O_RDWR)) < 0){
           printf(2, "cannot open tty: %s\n",tty_name);
           return -1;
        }

        if(cmd == CONNECT){
            printf(1, "Pouch container - %s connecting\n",container_name);
           if(connect_tty(tty_fd) < 0){
                close(tty_fd);
                printf(2, "cannot connect to the tty\n");
                return -1;
           }
        }else if(cmd == DISCONNECT && disconnect_tty(tty_fd) < 0){
           close(tty_fd);
           printf(2, "cannot disconnect from tty\n");
           return -1;
        }

        close(tty_fd);
        return 0;
}

static int find_tty(char* tty_name){
        int i;
        int tty_fd;
        char tty[] = "/ttyX";

        for(i=0; i < 3; i++){
            tty[4] = '0' + i;
            if((tty_fd = open(tty, O_RDWR)) < 0){
                        printf(2, "cannot open %s fd\n", tty);
                        return -1;
            }

            if(!is_attached_tty(tty_fd)){
                strcpy(tty_name,tty);
                close(tty_fd);
                return 0;
            }
            close(tty_fd);
        }

        return -1;
}

static int read_from_config(char* container_name, char* tty_name, int* pid){
   char buf[10];
   char* find_end;
   char* find_start;
   int cont_fd = open(container_name, 0);
   if(cont_fd < 0){
      printf(2, "There is no container: %s in a started stage\n", container_name);
      return -1;
    }

   if(read(cont_fd, buf, 5) <= 0) {
      close(cont_fd);
      printf(2,"CONT TTY NOT FOUND\n");
      return -1;
   }

   buf[5] = 0;
   strcpy(tty_name,buf);

   if(read(cont_fd, buf, sizeof(buf)-1) <= 0) {
      close(cont_fd);
      printf(2,"CONT TTY NOT FOUND\n");
      return -1;
   }

   buf[9] = 0;
   find_start = buf+7;
   find_end = find_start;
   while(*find_end != '\n' && *find_end != 0)
        find_end++;

   *find_end = 0;
   *pid = atoi(find_start);

   close(cont_fd);
   return 0;
}

static int write_to_config(char* container_name, char* tty_name, int pid){
   int cont_fd = open(container_name, O_CREATE|O_RDWR);
   if(cont_fd < 0){
            printf(2, "cannot open %s\n", container_name);
            return -1;
    }

   printf(cont_fd, "%s\nPPID: %d\nNAME: %s\n",tty_name, pid, container_name);
   close(cont_fd);
   return 0;
}

static int pouch_fork(char* container_name, int daemonize){
   int tty_fd;
   int pid = -1;
   int pid2 = -1;
   char tty_name[10];
   char cg_cname[256];


   //Find tty name
   if(find_tty(tty_name) < 0){
     printf(1, "Cannot find tty\n");
     exit(1);
   }

   if((tty_fd = open(tty_name, O_RDWR)) < 0){
      printf(2, "cannot open tty %s %d\n", tty_name, tty_fd);
      return -1;
   }

   int cont_fd = open(container_name, 0);
   if(cont_fd < 0){
      printf(1, "Pouch container - %s starting\n",container_name);
   }else{
      printf(2, "%s container is already started\n", container_name);
      return -1;
   }

   //Prepare cgroup name for container
   strcpy(cg_cname,"/cgroup/");
   strcat(cg_cname, container_name);
   create_pouch_cgroup(cg_cname);

    //Parent process forking child process
   if(!daemonize || (daemonize && (pid2 = fork()) == 0)){
      //Set up pid namespace before fork
      if(unshare(PID_NS) != 0){
        printf(2, "Cannot create pid namespace\n");
        return -1;
      }

      pid = fork();
      if(pid == -1){
         panic("fork");
      }

      if(pid == 0) {
         if(tty_fd != -1){
            //attach stderr stdin stdout
            if(attach_tty(tty_fd) < 0){
              printf(2,"attach failed");
              exit(1);
            }

           //"Child process - setting up namespaces for the container
           // Set up mount namespace.
           if(unshare(MOUNT_NS) < 0) {
             printf(1, "Cannot create mount namespace\n");
             exit(1);
           }

           printf(2,"Entering container\n");
           exec("sh", argv);
        }else{
           printf(2,"Error connecting tty\n");
        }
      }else{

        //"Parent process - waiting for child

        // Move the current process to "/cgroup/<cname>" cgroup.
        strcat(cg_cname,"/cgroup.procs");
        int cgroup_procs_fd = open(cg_cname, O_RDWR);
        char cur_pid_buf[3];
        itoa(cur_pid_buf, pid);
        write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf));
        close(cgroup_procs_fd);

        if(write_to_config(container_name, tty_name, pid) >= 0)
           wait(0);

        // Delete container cgroup
        unlink(cg_cname);

        disconnect_tty(tty_fd);
        detach_tty(tty_fd);
        close(tty_fd);
        printf(2,"Exiting container\n");
        exit(0);
      }
    }

  close(tty_fd);
  return pid;
}

void printhelp(){
        printf(2,"Pouch containers:\n");
        printf(2,"       pouch start {name}\n");
        printf(2,"       pouch connect {name}\n");
        printf(2,"       pouch disconnect {name}\n");
        printf(2,"       pouch destroy {name}\n");
        printf(2,"       pouch info {name}\n");
        printf(2,"Pouch containers cgroups:\n");
        printf(2,"       pouch cgroup {cname} {state-object} [value]\n");
}

static int create_pouch_cgroup(char *cg_cname){

    if(mkdir(cg_cname) != 0){
        printf(2,"failed to create cgroup for: %s \n",cg_cname);
        return -1;
    }
    char cgpath[256];
    strcpy(cgpath, cg_cname);
    strcat(cgpath,"/cgroup.subtree_control");

    int cgroup_subtree_control_fd =
        open(cgpath, O_RDWR);

    if(cgroup_subtree_control_fd < 0)
        return -1;

    // Enable cpu controller
    char buf[256];
    strcpy(buf, "+cpu");
    if(write(cgroup_subtree_control_fd, buf, sizeof(buf)) < 0)
        return -1;
    if(close(cgroup_subtree_control_fd) < 0)
        return -1;
    return 0;

}
static int init_pouch_cgroup(){

    int cgroup_fd = -1;
    if((cgroup_fd = open("/cgroup", O_RDWR)) < 0){

        if(mkdir("/cgroup") != 0){
            printf(1, "Failed to create root cgroup");
            return -1;
        }
        if(mount(0, "/cgroup", "cgroup") != 0){
            printf(1, "Failed to mount cgroup fs");
            return -1;
        }
    }else{
        close(cgroup_fd);
    }

    return 0;
}

static void empty_string(char * string, int length)
{
    for (int i = 0; i < length; i++)
        string[i] = 0;
}

static int print_cinfo(char* container_name, char * tty_name, int pid){
    char buf[256];
    char cgmax[256];
    char cgstat[256];

    strcpy(cgmax, "/cgroup/");
    strcat(cgmax,container_name);
    strcat(cgmax,"/cpu.max");

    strcpy(cgstat, "/cgroup/");
    strcat(cgstat,container_name);
    strcat(cgstat,"/cpu.stat");

    int cpu_max_fd = open(cgmax, O_RDWR);
    int cpu_stat_fd = open(cgstat, O_RDWR);

    printf(1, "     Pouch container- %s:\n",container_name);
    printf(1,"tty - %s\n",tty_name);
    printf(1,"pid - %d\n",pid);
    printf(1,"     cgroups:\n");
    if(cpu_max_fd > 0 && cpu_stat_fd > 0){
        empty_string(buf, sizeof(buf));
        if(read(cpu_max_fd, buf, sizeof(buf)) < 0)
            return -1;
        printf(1, "cpu.max:     \n%s\n", buf);
        empty_string(buf, sizeof(buf));
        if(read(cpu_stat_fd, buf, sizeof(buf)) < 0)
            return -1;
        printf(1, "cpu.stat:     \n%s\n", buf);

        if(close(cpu_max_fd) < 0)
            return -1;
        if(close(cpu_stat_fd) < 0)
            return -1;
    }else{
        printf(1,"None.\n");
    }

    return 0;
}

int
main(int argc, char *argv[])
{
  int daemonize = 1;
  enum p_cmd cmd = START;
  char container_name[CNTNAMESIZE];

  if(argc >= 3){
     if((strcmp(argv[1],"--help") == 0) || (char)*argv[1] == '-'){
        printhelp();
        exit(0);
     }
     strcpy(container_name, argv[2]);
  }else{
    printhelp();
    exit(0);
  }

  init_pouch_cgroup();

  if(argc >= 2){
     if((strcmp(argv[1],"start")) == 0){
        cmd = START;
     }else if((strcmp(argv[1],"connect")) == 0){
        cmd = CONNECT;
     }else if((strcmp(argv[1],"disconnect")) == 0){
        cmd = DISCONNECT;
        printf(1, "Pouch container - %s disconnecting\n",container_name);
     }else if((strcmp(argv[1],"destroy")) == 0){
        cmd = DESTROY;
        printf(1, "Pouch container - %s destroying\n",container_name);
     }else if((strcmp(argv[1],"cgroup")) == 0 && argc == 5){
        cmd = LIMIT;
        printf(1, "Pouch container - %s cgroup applying \n",container_name);
     }else if((strcmp(argv[1],"info")) == 0 ){
        cmd = INFO;
     }else{
        printhelp();
        exit(1);
    }

    if(cmd == LIMIT && argc == 5){
        if(pouch_limit_cgroup(container_name, argv[3], argv[4]) < 0){
            printf(1, "Not applied. Incorrect cgroup object-state provided. \n");
            exit(1);
        }
    }else if(pouch_cmd(container_name, cmd, daemonize) < 0){
        exit(1);
    }
  }

  exit(0);
}
