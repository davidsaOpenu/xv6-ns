#include "syscall.h"
#include "types.h"
#include "fcntl.h"
#include "ns_types.h"
#include "tests_common.h"


/* Verify init
  - verify the creation of tty devices
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
  return 0;
}


/* Verify ioctl syscall attach/detatch
   for a given tty device:
   verify TTYGETS/TTY_SETS operations on attaching and detatching a tty device
   TTYGETS return "1" on which state(attached/detatched) is set and "0" if unset
   TTYSETS return "0" on operation success
*/
int ioctl_attach_detatch_test() {
  int tty_fd;
  char * tty_name = "tty0";

  if((tty_fd = open(tty_name, O_RDWR)) < 0){
    printf(stderr, "failed to open %s\n",tty_name);
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
  }
  close(tty_fd);
  return 0;
}

/* Verify ioctl syscall connect/disconnect
   for a given tty device:
   verify TTYGETS/TTY_SETS operations on connecting and disconnecting a tty device
   TTYGETS return "1" on which state(connected/disconnected) is set and "0" if unset
   TTYSETS return "0" on operation success
*/
int ioctl_connect_disconnect_test() {
  int tty_fd;
  char * tty_name = "tty0";

  if((tty_fd = open(tty_name, O_RDWR)) < 0){
    printf(stderr, "failed to open %s\n",tty_name);
  }

  if(ioctl(tty_fd, TTYSETS, DEV_CONNECT) < 0){
    printf(stderr, "step 2. %s failed TTYSETS / DEV_CONNECT \n",tty_name);
    close(tty_fd);
    return -1;
  }

  int status = ioctl(tty_fd, TTYGETS, DEV_CONNECT);

  if(ioctl(tty_fd, TTYSETS, DEV_DISCONNECT) == 0){
    printf(stderr, "%s disconnected \n",tty_name);
    ioctl(tty_fd, TTYSETS, DEV_DETACH);
    printf(stderr, "%s detatched \n",tty_name);
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
  close(tty_fd);
  return 0;
}

/* Verify that any attatch/connect operations to the console device fails
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
  }

  else if(status > 0){
    printf(stderr, " %s return status: %d - wrong! \n",tty_name, status);
    close(tty_fd);
    return -1;
  }

  close(tty_fd);
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
    printf(stderr, "ioctl arg failure");
    return -1;
  }

  if(ioctl(tty_fd, TTYSETS, 0) != -1){
    close(tty_fd);
    printf(stderr, "ioctl arg failure");
    return -1;
  }

  if(ioctl(tty_fd, 0, DEV_ATTACH) != -1){
    close(tty_fd);
    printf(stderr, "ioctl arg failure");
    return -1;
  }

  if(ioctl(tty_fd, 0, DEV_CONNECT) != -1){
    close(tty_fd);
    printf(stderr, "ioctl arg failure");
    return -1;
  }

  if(ioctl(tty_fd, O_CREATE, O_CREATE) != -1){
    close(tty_fd);
    printf(stderr, "ioctl arg failure");
    return -1;
  }

  if(ioctl(tty_fd, TTYSETS + 5, DEV_CONNECT + 5) != -1){
    close(tty_fd);
    printf(stderr, "ioctl arg failure");
    return -1;
  }

  close(tty_fd);
  return 0;
}




int main() {

  //TTY INIT TESTS
  if(run_test(init_tests, "init test") < 0)
    exit(1);

  //ioctl TESTS
  if(run_test(ioctl_args_test, "ioctl args test,") < 0)
    exit(1);

  if(run_test(ioctl_wrong_device_test, "ioctl wrongdev test") < 0)
    exit(1);

  if(run_test(ioctl_console_test, "ioctl console test") < 0)
    exit(1);

  //ioctl SCENARIO TESTS
  if(run_test(ioctl_attach_detatch_test, "ioctl attach test") < 0)
    exit(1);

  if(run_test(ioctl_connect_disconnect_test, "ioctl connect test") < 0)
    exit(1);

  printf(stderr, "ioctl TESTS PASS:\n");
  exit(0);
}

