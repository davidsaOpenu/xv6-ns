#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "ns_types.h"

char *argv[] = { "sh", 0 };

typedef enum p_cmd {START, CONNECT, DISCONNECT, DESTROY} p_cmd;
#define CNTNAMESIZE 100
#define CNTARGSIZE 30

static int pouch_fork(char* container_name, int daemonize);
static int pouch_cmd(char* container_name,enum p_cmd, int daemonize);
static int find_tty(char* tty_name);
static int write_to_config(char* container_name, char* tty_name, int pid, int daemonize);
static int read_from_config(char* container_name, char* tty_name, int* pid);
void panic(char *s);

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit(1);
}

static int pouch_cmd(char* container_name, enum p_cmd cmd, int daemonize){
	int tty_fd;
        int pid;
	char tty_name[10];

        if(cmd == START){
            //cg3. check if user provided limitations in format 'pouch start cname set cpu.limit 20%'
            // or consider another command like : 'pouch cname set cpu.limit 20%'
            // if user not provides a limit - this pid will be unlimited
            // if considering another command - we can change the limitations after container was created
           return pouch_fork(container_name, daemonize);
	}

        if(read_from_config(container_name, tty_name, &pid) < 0){
	   return -1;
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
                printf(2, "cannot connect\n");
                return -1;
           }
        }else if(cmd == DISCONNECT && disconnect_tty(tty_fd) < 0){
	   close(tty_fd);
	   printf(2, "cannot disconnect\n");
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

static int write_to_config(char* container_name, char* tty_name, int pid, int daemonize){
   int cont_fd = open(container_name, O_CREATE|O_RDWR);
   if(cont_fd < 0){
 	    printf(2, "cannot open %s\n", container_name);
     	    return -1;
    }

   printf(cont_fd, "%s\nPPID: %d\nNAME: %s\ndaemonize: %d\n",tty_name, pid, container_name, daemonize);
   close(cont_fd);
   return 0;
}

static int pouch_fork(char* container_name, int daemonize){
   int tty_fd;
   int pid = -1;
   int pid2 = -1;
   char tty_name[10];

   //Find tty name
   if(find_tty(tty_name) < 0){
     printf(1, "Cannot find tty\n");
     exit(1);
   }

   if((tty_fd = open(tty_name, O_RDWR)) < 0){
      printf(2, "cannot open %s %d\n", tty_name, tty_fd);
      return -1;
   }

   int cont_fd = open(container_name, 0);
   if(cont_fd < 0){
      printf(1, "Pouch container - %s starting\n",container_name);
   }else{
      printf(2, "%s container is already started\n", container_name);
      return -1;
   }


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

        // cg4. create a cgroup with container name for the process
        // like: /cgroup/pouch/cname
        // add pid to the cgroup [write(..)]
        // enable cpu controller and add cpu limitations is they are [write(..)]
        // update limitation information in 'write_to_config'

        if(write_to_config(container_name, tty_name, pid, daemonize) >= 0)
           wait(0);

        // cg5. delete cgroup [unlink(..)]

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
        printf(2,"usage: pouch start <name>\n");
	printf(2,"       pouch connect <name>\n");
	printf(2,"       pouch disconnect <name>\n");
        printf(2,"       pouch destroy <name>\n");
}

int
main(int argc, char *argv[])
{
  int daemonize = 1;
  enum p_cmd cmd = START;
  char container_name[CNTNAMESIZE];
  char var[CNTARGSIZE];
  int i;
  if(argc >= 3){
     if((strcmp(argv[1],"--help") == 0) || (char)*argv[1] == '-'){
        printhelp();
        exit(0);
     }

     strcpy(container_name, argv[2]);
     for(i = 3; i < argc; i++){
        strcpy(var, argv[i]);
        printhelp();
        exit(0);
     }
  }else{
    printhelp();
    exit(0);
  }

  //cgoups incorporation
  //cg1. check if root cgroups [read(..)] already exists if not init it with [mkdir(..), mount(..)]
  //cg2. create pouch cgroup [mkdir(..)]

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
     }else{
      printhelp();
      exit(1);
    }

    if(pouch_cmd(container_name, cmd, daemonize) < 0){
	exit(1);
    }

    //cg1.1 delete pouch cgroup [unlink(..)]
    // unmount cgroup fs if created [umount(..)]
  }

  exit(0);
}
