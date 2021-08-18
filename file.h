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

#define CONSOLE 1

#endif
