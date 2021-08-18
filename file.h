#ifndef XV6_FILE_H
#define XV6_FILE_H

#include "fs.h"
#include "sleeplock.h"
#include "cgfs.h"
#include "param.h"
#include "vfs_file.h"

struct file {
  struct vfs_file vfs_file;
};


// in-memory copy of an inode
struct inode {
  uint size;
  uint addrs[NDIRECT+1];
  struct vfs_inode vfs_inode;
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE_MAJOR 1
#define CONSOLE_MINOR 0

#endif
