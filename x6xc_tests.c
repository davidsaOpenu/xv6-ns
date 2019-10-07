#include "syscall.h"
#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "ns_types.h"
#include "file.h"

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

  printf(stderr, " running test - '%s'------------------\n", name);
  int ret = check(fork(), "fork failed");
  if (ret == 0) {
    exit(func());
  }

  pid = ret;
  if (child_exit_status(pid) != 0) {
    printf(stderr, "> > > > > > > failed test - '%s'\n", name);
  }else{
    printf(stderr, "------------------ OK - '%s'\n", name);
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
    }else{
      close(tty_fd);
    }
  }
  return 0;
}


/* Verify ioctl syscall attach
  - TTYGET command returns if secons command is true
      if TTYGETS, DEV_ATTACH = 0, device is not attached
      if TTYGETS, DEV_ATTACH = 1, device is attached
  - TTYSET command sets the second command, return 0 if ok  
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
    printf(stderr, "step 3. %s is already attached\n", tty_name);
    close(tty_fd);
    return 0;
  }

  return 0;
}

/* Verify ioctl syscall connect
  - TTYGET command returns if secons command is true
      if TTYGETS, DEV_CONNECT = 0, device is not connected
      if TTYGETS, DEV_CONNECT = 1, device is connected
  - TTYSET command sets the second command, return 0 if ok  
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
  int tty_fd = CONSOLE; //CONSOLE
  char * tty_name = "console";

  if( ioctl(tty_fd, TTYGETS, DEV_CONNECT) >= 0){
    printf(stderr, " %s  connected TTYGETS / DEV_CONNECT \n",tty_name);
    close(tty_fd);
    return -1;
    /*connecting here with "tty-1" = ip->major - (CONSOLE+1)  ?? */
  }

  if(ioctl(tty_fd, TTYGETS, DEV_ATTACH)  >= 0){
    printf(stderr, " %s  attached TTYGETS / DEV_ATTACH \n",tty_name);
    close(tty_fd);
    return -1;
  }

  if(ioctl(tty_fd, TTYSETS, DEV_ATTACH)  >= 0){
    printf(stderr, " %s  attached TTYSETS / DEV_ATTACH \n",tty_name);
    close(tty_fd);
    return -1;
  }

  if( ioctl(tty_fd, TTYSETS, DEV_CONNECT) >= 0){
    printf(stderr, " %s  connected TTYSETS / DEV_CONNECT \n",tty_name);
    close(tty_fd);
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
    printf(stderr, " %s device is not in the tty list, sys_call returned -1 - correct \n",tty_name);
    close(tty_fd);
    return 0;
  }

  return 0;
}

int main() {
  //init tty tests
  run_test(init_tests, "init test");

  //ioctl syscall tests
  run_test(ioctl_attach_test, "ioctl attach test");
  run_test(ioctl_connect_test, "ioctl connect test");
  run_test(ioctl_console_test, "ioctl_console_test");
  run_test(ioctl_wrong_device_test, "ioctl_wrongdev_test");
  
  exit(0);
}

