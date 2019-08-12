#include "types.h"
#include "stat.h"
#include "user.h"

static int pouch_fork(void);
/*static int pouch_cgroup(void);
static int pouch_mount_ns(void);
static int pouch_pid_ns(void);*/
void panic(char *s);

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit(1);
}


/*static int pouch_pid_ns(void){
	//2DO Init container pid namespace
}

static int pouch_mount_ns(void){
	//2DO Init container mount namespace
}

static int pouch_cgroup(void){
	//2DO Init container cgroup (input should be two ints and a pid)
}*/

 
static int pouch_fork(void){

  //Parent process forking child process

  int pid = fork();
  //2DO if user specified <cgroup> argument a cgroup should be set up, pid should be added to it.

  //2DO if user specified <pid namespace> argument a pid namespace should be set up

  if(pid == -1)
    panic("fork");

  if (pid == 0) {
	//2DO if user specified <mount namespace> argument a mount namespace should be set up

	//"Child process - setting up namespaces for the container
	//2DO - should exec in to <executable> param.
	//For example - exec("sh", shargv); 
  }else{
	//"Parent process - waiting for child
	wait(0);

	//"Parent process child died
  }
  return pid;
}


int
main(void)
{
  printf(1, "Pouch container\n");


  pouch_fork();
  exit(0);
}
