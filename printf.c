#include "types.h"
#include "stat.h"
#include "user.h"

static int
putc(int fd, char c)
{
  return write(fd, &c, 1);
}

static int
printint(int fd, int xx, int base, int sgn)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;
  int retval = 0;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0) {
    if ((retval = putc(fd, buf[i])) < 0) {
      return retval;
    }
  }
  return retval;
}

// Print to the given fd. Only understands %d, %x, %p, %s.
int
printf(int fd, const char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;
  int retval = 0;

  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        retval = putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        retval = printint(fd, *ap, 10, 1);
        ap++;
      } else if(c == 'x' || c == 'p'){
        retval = printint(fd, *ap, 16, 0);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        retval = putc(fd, *ap);
        ap++;
      } else if(c == '%'){
        retval = putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        retval = putc(fd, '%');
        if (retval >= 0) {
          retval = putc(fd, c);
        }
      }
      state = 0;
    }
    if (retval < 0) {
      break;
    }
  }
  return retval;
}
