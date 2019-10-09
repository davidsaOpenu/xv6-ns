#include "syscall.h"
#include "types.h"
#include "user.h"
#include "wstatus.h"
#include "fcntl.h"
#include "ns_types.h"
#include "file.h"


#define CLONE_NEWPID 2
#define NULL 0

typedef   signed int          pid_t;
typedef   signed int          size_t;

typedef int (*test_func_t)(void);

int stderr = 2;

int check(int r, const char *msg) {
  if (r < 0) {
    printf(stderr, "%s\n", (char *)msg);
    exit(1);
  }

  return r;
}

static int child_exit_status(int pid) {
  int changed_pid = -1;
  int wstatus;
  do {
    changed_pid = check(wait(&wstatus), "failed to waitpid");
  } while (changed_pid != pid);

  // TODO: there is no exit status in xv6
  return WEXITSTATUS(wstatus);
}

int run_test(test_func_t func, const char *name) {
  int status = 0;
  int pid = -1;

  printf(stderr, "    '%s'--------", name);
  int ret = check(fork(), "fork failed");
  if (ret == 0) {
    exit(func());
  }

  pid = ret;
  if (child_exit_status(pid) != 0) {
    printf(stderr, "> > > > > > > failed test - '%s'\n", name);
  }else{
    printf(stderr, "    :OK - '%s'\n", name);
  }
  return status;
}

/* Verify init
  - creation of new tty's
  - 3 ttys should be ready to use as OS loads  
*/
int init_tests() {
  int i;
  int tty_fd;
  char tty[] = "/ttyX";

  for(i = 0; i<3; i++){
    tty[4] = '0' + i;
    if((tty_fd = open(tty, O_RDWR))<0){
      printf(stderr, "failed to open %s\n",tty);
      return -1;
    }
      close(tty_fd);
  }

  //test opening another tty
  char * ttytest = "tty";
  if((tty_fd = open(ttytest, O_RDWR)) == 0){
    printf(stderr, "obtained fd to incorrect tty: %s\n",ttytest);
    return -1;
  }
  return 0;
}


/* Verify ioctl syscall attach
  ioctl(fd, TTYGETS, DEV_ATTACH) should return 0
  ioctl(fd, TTYSETS, DEV_ATTACH) should return 0
  ioctl(fd, TTYGETS, DEV_ATTACH) should return 1 

  ioctl(fd, TTYSETS, DEV_DETACH) should return 0
  ioctl(fd, TTYGETS, DEV_DETACH) should return 1 
*/
int ioctl_attach_test() {
  int tty_fd;
  char * tty_name = "tty0";
  
  if((tty_fd = open(tty_name, O_RDWR)) < 0){
    printf(stderr, "failed to open %s\n",tty_name);
    return -1;
  }

  if(ioctl(tty_fd, TTYGETS, DEV_ATTACH)  < 0){
    printf(stderr, "step 1. %s failed TTYGETS / DEV_ATTACH \n",tty_name);
    close(tty_fd);
    return -1;
  }

  if(ioctl(tty_fd, TTYSETS, DEV_ATTACH) < 0){
    printf(stderr, "step 2. %s failed TTYSETS / DEV_ATTACH \n",tty_name);
    close(tty_fd);
    return -1;
  }

  int status = ioctl(tty_fd, TTYGETS, DEV_ATTACH);

  if(status == 0){
    printf(stderr, "step 3. %s failed TTYGETS / DEV_ATTACH, tty obtained again \n",tty_name);
    close(tty_fd);
    return -1;
  }
  else if(status == -1){
    printf(stderr, "step 3. ioctl failed");
    close(tty_fd);
    return -1;
  }
  else if(status == 1){
    printf(stderr, "step 3. %s  attached\n", tty_name);

    if(ioctl(tty_fd, TTYSETS, DEV_DETACH) == 0){
      printf(stderr, "%s detached \n",tty_name);
    }else{
      printf(stderr, "step 3. %s detach failed \n",tty_name);
      close(tty_fd);
      return -1;
    }

    status = ioctl(tty_fd, TTYGETS, DEV_DETACH);

    if(status == 0){
      printf(stderr, "step 3. %s TTYGETS, DEV_DETACH \n",tty_name);
      close(tty_fd);
      return -1;
    }
    else if(status == -1){
      printf(stderr, "step 3. ioctl GETS / DETACH failed");
      close(tty_fd);
      return -1;
    }
    else if(status == 1){
      printf(stderr, "status after detach. %d \n", tty_name);
      close(tty_fd);
      return 0;
    }

      close(tty_fd);
      return 0;
    }

  return 0;
}

/* Verify ioctl syscall connect
   ioctl(fd, TTYGETS, DEV_CONNECT) should return 0
   ioctl(fd, TTYSETS, DEV_CONNECT) should return 0
   ioctl(fd, TTYGETS, DEV_CONNECT) should return 1

   ioctl(fd, TTYSETS, DEV_DISCONNECT) should return to console, 0
   ioctl(fd, TTYGETS, DEV_DISCONNECT) should return 1
  
*/
int ioctl_connect_test() {
  int tty_fd;
  char * tty_name = "tty0";
  
  if((tty_fd = open(tty_name, O_RDWR)) < 0){
    printf(stderr, "failed to open %s\n",tty_name);
  }

  if( ioctl(tty_fd, TTYGETS, DEV_CONNECT) < 0){
    printf(stderr, "step 1. %s failed TTYGETS / DEV_CONNECT \n",tty_name);
    close(tty_fd);
    return -1;
  }

  if(ioctl(tty_fd, TTYSETS, DEV_CONNECT) < 0){
    printf(stderr, "step 2. %s failed TTYSETS / DEV_CONNECT \n",tty_name);
    close(tty_fd);
    return -1;
  }

  int status = ioctl(tty_fd, TTYGETS, DEV_CONNECT);

  if(ioctl(tty_fd, TTYSETS, DEV_DISCONNECT) == 0){
    printf(stderr, "%s disconnected \n",tty_name);
  }

  if(status == 0){
    printf(stderr, "step 3. %s failed TTYGETS / DEV_CONNECT, tty obtained again \n",tty_name);
    close(tty_fd);
    return -1;
  }
  else if(status == -1){
    printf(stderr, "step 3. ioctl failed");
    close(tty_fd);
    return -1;
  }
  else if(status == 1){
    printf(stderr, "step 3. %s is connected\n", tty_name);
    close(tty_fd);
    return 0;
  }
    return 0;
}

/* Verify no attach/connect to the console device
  - console is a device with major/minor = 1, it should not be used as
  - a tty for a container.
*/
int ioctl_console_test() {
  int tty_fd = 1; //CONSOLE
  char * tty_name = "console";


  if( ioctl(tty_fd, TTYGETS, DEV_CONNECT) == 0){
    printf(stderr, " %s  connected TTYGETS / DEV_CONNECT \n",tty_name);
    return -1;
  }

  if(ioctl(tty_fd, TTYGETS, DEV_ATTACH)  == 0){
    printf(stderr, " %s  attached TTYGETS / DEV_ATTACH \n",tty_name);
    return -1;
  }

  if(ioctl(tty_fd, TTYSETS, DEV_ATTACH)  == 0){
    printf(stderr, " %s  attached TTYSETS / DEV_ATTACH \n",tty_name);
    return -1;
  }

  if( ioctl(tty_fd, TTYSETS, DEV_CONNECT) == 0){
    printf(stderr, " %s  connected TTYSETS / DEV_CONNECT \n",tty_name);
    return -1;
  }

    return 0;
}

/* Verify wrong device usage
  - only specified devices created by init (tty0-2) can be used as ttys
  
*/
int ioctl_wrong_device_test() {
  int tty_fd;
  char * tty_name = "tty_test";

  if(mknod(tty_name,5,5) < 0){
    printf(stderr, "failed to create test device %s\n",tty_name);
    return -1;
  }

  if((tty_fd = open(tty_name, O_RDWR)) < 0){
    printf(stderr, "failed to open %s\n",tty_name);
    return -1;
  }

  int status = ioctl(tty_fd, TTYGETS, DEV_ATTACH);

  unlink(tty_name);

  if (status == 0){
    printf(stderr, " %s device passed verification? - wrong! \n",tty_name);
    close(tty_fd);
    return -1;
    /* failing here */
  }

  else if(status > 0){
    printf(stderr, " %s return status: %d - wrong! \n",tty_name, status);
    close(tty_fd);
    return -1;
  }

  else if(status == -1){
    //printf(stderr, " %s device is not in the tty list, sys_call returned -1 - correct \n",tty_name);
    close(tty_fd);
    return 0;
  }

  return 0;
}

/* ioctl syscall arguments test
  - check only TTYGET/TTYSET commands accepted with only
  - DEV_CONNECT/DEV_DISCONNECT, DEV_ATTACH/DEV_DETACH
*/

int ioctl_args_test() {

  int tty_fd;
  char * tty_name = "tty0";
  
  if((tty_fd = open(tty_name, O_RDWR)) < 0){
    printf(stderr, "failed to open %s\n",tty_name);
  }
  
  if(ioctl(tty_fd, TTYGETS, 0) != -1){
    close(tty_fd);
    return -1;
  }

  if(ioctl(tty_fd, 0, 1) != -1){
    close(tty_fd);
    return -1;
  }

  if(ioctl(tty_fd, 0, 0) != -1){
    close(tty_fd);
    return -1;
  }

  return 0;
}

int main() {
  //init tty tests
  printf(stderr, "TTY INIT TESTS:\n");
  run_test(init_tests, "init test");

  //ioctl tests
  printf(stderr, "IOCTL TESTS:\n");
  run_test(ioctl_args_test, "ioctl args test,");
  run_test(ioctl_wrong_device_test, "ioctl wrongdev test");
  run_test(ioctl_console_test, "ioctl console test"); /*ilock panic*/

  //ioctl syscall scenario  tests
  printf(stderr, "IOCTL SCENARIO TESTS:\n");
  run_test(ioctl_attach_test, "ioctl attach test");
  run_test(ioctl_connect_test, "ioctl connect test");
  
  
  
  exit(0);
}

