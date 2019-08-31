#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

int 
attach_tty(int tty_fd)
{
	close(0);
	close(1);
	close(2);
	if(dup(tty_fd) < 0)
          return -1;

	if(dup(tty_fd) < 0)
          return -1;

	if(dup(tty_fd) < 0)
           return -1;

     return 0;
}


int
connect_tty(int tty_fd)
{
      ioctl(tty_fd, DEV_CONNECT);
      return tty_fd;
}

int
disconnect_tty(int tty_fd)
{
    ioctl(tty_fd, DEV_DISCONNECT);
    return 0;
}
