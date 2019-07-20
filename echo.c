#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int i = 1;
  int size = 1000;
  char * data = malloc(size);
  int offset = 0;
  char new_line = 1;

  if (!data) {
    exit(1);
  }

  if (argc > 1 && !strcmp("-n", argv[1])) {
    ++i;
    new_line = 0;
  }

  for(; i < argc; i++) {
    int length = strlen(argv[i]);
    if (length + offset > size) {
      char * newdata = 0;
      size = length + size * 3 / 2;
      newdata = malloc(size);
      if (!newdata) {
        free(data);
        exit(1);
      }
      memmove(newdata, data, offset);
      free(data);
      data = newdata;
    }
    memmove(data + offset, argv[i], length);
    offset += length;
    if (i + 1 < argc) {
      data[offset] = ' ';
      ++offset;
    } else if (new_line) {
      data[offset] = '\n';
      ++offset;
    }
  }

  write(1, data, offset);
  free(data);
  exit(0);
}
