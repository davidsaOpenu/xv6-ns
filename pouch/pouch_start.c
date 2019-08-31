#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "ns_types.h"

char *argv[] = { "sh", 0 };
char *argv2[] = { "sh &", 0 };


static int pouch_fork(char* container_name, char* tty_name, int user_tty, int demonize);
static int find_tty(char* tty_name);
static int write_to_config(char* container_name, char* tty_name, int pid, int demonize);
void panic(char *s);

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit(1);
}


//Function will find a tty return a free tty to be used by the container.  
static int find_tty(char* tty_name){
	//2DO replace static tty2 with find_tty	
	strcpy(tty_name,"tty2");
	return 0;
}


static int write_to_config(char* container_name, char* tty_name, int pid, int demonize){
   int cont_fd = open(container_name, O_CREATE|O_RDWR);
   if(cont_fd < 0){
 	    printf(2, "cannot open %s\n", container_name);
     	    return -1;
    }

   printf(cont_fd, "NAME: %s\nTTY: %s\nPPID: %d\nDEMONIZE: %d\n",container_name, tty_name, pid, demonize); 
   close(cont_fd);
   return 0;
}
 
static int pouch_fork(char* container_name, char* tty_name, int user_tty, int demonize){
   int tty_fd;
   int pid = -1;
   int pid2 = -1;

   //Find tty name
   if(!user_tty && find_tty(tty_name) < 0){
     printf(1, "Cannot find tty\n");
     exit(1);
   }

   if((tty_fd = open(tty_name, O_CREATE|O_RDWR)) < 0){
 	if((tty_fd = open(tty_name, O_RDWR)) < 0){
 	    printf(2, "cannot open %s %d\n", tty_name, tty_fd);
     	    return -1;
  	}
    }

    //Parent process forking child process
    //pid = fork();
    if (!demonize || (demonize && (pid2 = fork()) == 0)){

            //Set up pid namespace before fork
            if(unshare(PID_NS) != 0){
		printf(2, "Cannot create pid namespace\n");
		return -1;
	    }

	    pid = fork();
	    if(pid == -1)
	       panic("fork");

	    if (pid == 0) {	
			if(tty_fd != -1){
				//attach stderr stdin stdout
				if(attach_tty(tty_fd) < 0){
				 printf(2,"attach failed"); //2DO disconnect back to console.
				 exit(1);
				}

				//Connect tty
				//if(!demonize)
				connect_tty(tty_fd); 

				//"Child process - setting up namespaces for the container
				// Set up mount namespace.
				if (unshare(MOUNT_NS) < 0) {
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
		if(write_to_config(container_name,tty_name, pid, demonize) >= 0)
					wait(0);

		disconnect_tty(tty_fd);
		close(tty_fd);
		exit(0);
		printf(2,"Exiting container\n");
	  }	
  }

  close(tty_fd);
  return pid;
}

void printhelp(){
	printf(2,"usage: pouch_start <name> [--tty tty<0-2>] <--demonize | &>\n");
}

int
is_tty_file(char* tty_name){
	if(tty_name[0] == 't' && tty_name[1] == 't' && tty_name[2] == 'y' && tty_name[3] >= '0' && tty_name[3] <= '2'){
		return 1;
	} 

	return 0;
}

int
main(int argc, char *argv[])
{
  int user_tty = 0;
  int demonize = 0;
  char tty_name[5];
  char container_name[100] = "cont_";
  char var[30];
  int i;
  if(argc >= 2){
	if((strcmp(argv[1],"--help") == 0) || (char)*argv[1] == '-'){
			printhelp();
			exit(0);
	}

	strcpy(container_name+5, argv[1]);
        for(i = 2; i < argc; i++){
		strcpy(var, argv[i]);
		if(i < (argc - 1) && (strcmp(var,"--tty") == 0) && strlen(argv[i+1]) == 4){
			strcpy(tty_name, argv[i+1]);
			if(is_tty_file(tty_name)){
				user_tty = 1;
			}
			i++;
			continue;
		}

		if((strcmp(var,"--demonize") == 0)){
			demonize = 1;
			continue;
		}

		printhelp();
		exit(0);

	}
  }else{
	printhelp();
	exit(0);
  }

  printf(1, "Pouch container - %s\n",container_name);

  pouch_fork(container_name, tty_name, user_tty, demonize);
  exit(0);
}
