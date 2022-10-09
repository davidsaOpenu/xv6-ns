// Console input and output defenitions.
#ifndef XV6_CONSOLE_H
#define XV6_CONSOLE_H

#include "spinlock.h"

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4

// Console driver major number
#define CONSOLE_MAJOR 1
#define CONSOLE_MINOR 0

typedef struct device_lock {
  struct spinlock lock;
  int locking;
} device_lock;

typedef struct tty {
  int flags;
  struct spinlock lock;
} tty;


#endif