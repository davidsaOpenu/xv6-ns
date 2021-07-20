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
  int num_chars = 0;

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
    num_chars++;
  }
  return num_chars;
}

static int
printstr(int fd, char *str)
{
  int retval = 0;
  int tmp_num_chars;

  if (str == 0)
    str = "(null)";

  while (*str != 0) {
    if ((tmp_num_chars = putc(fd, *str)) < 0) {
      retval = tmp_num_chars;
      break;
    }
    str++;
    retval += tmp_num_chars;
  }

  return retval;
}

static int
printunknown(int fd, char c)
{
  int retval, tmp_num_chars;

  tmp_num_chars = putc(fd, '%');
  if (tmp_num_chars < 0) {
    return tmp_num_chars;
  }

  retval = putc(fd, c);
  if (retval < 0) {
    return retval;
  }

  return (retval + tmp_num_chars);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
int
printf(int fd, const char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;
  int num_chars = 0;
  int retval = 0;

  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    num_chars = 0;
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        num_chars = putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        num_chars = printint(fd, *ap, 10, 1);
        ap++;
      } else if(c == 'x' || c == 'p'){
        num_chars = printint(fd, *ap, 16, 0);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        num_chars = printstr(fd, s);
      } else if(c == 'c'){
        num_chars = putc(fd, *ap);
        ap++;
      } else if(c == '%'){
        num_chars = putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        num_chars = printunknown(fd, c);
      }
      state = 0;
    }
    if (num_chars < 0) {
      return num_chars;
    }
    retval += num_chars;
  }
  return retval;
}
