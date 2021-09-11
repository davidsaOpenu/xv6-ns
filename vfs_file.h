#ifndef XV6_VFS_FILE_H
#define XV6_VFS_FILE_H

#include "vfs_fs.h"
#include "sleeplock.h"
#include "cgfs.h"
#include "param.h"
#include "stat.h"

struct file_operations {
    struct vfs_file*    (*filedup) (struct vfs_file*);
    int             (*fileread) (struct vfs_file*, char*, int n);
    int             (*filestat) (struct vfs_file*, struct stat*);
    int             (*filewrite) (struct vfs_file*, char*, int n);
};

struct vfs_file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_CG } type;
  int ref; // reference count
  char readable;
  char writable;
  uint off;
  union {
      // FD_PIPE
      struct pipe *pipe;

      // FD_INODE
      struct {
          struct vfs_inode *ip;
          struct mount *mnt;
      };

      // FD_CG
      struct {
          struct cgroup *cgp;
          char cgfilename[MAX_CGROUP_FILE_NAME_LENGTH];
          union {
              // cpu
              union {
                  struct {
                      char active;
                      int usage_usec;
                      int user_usec;
                      int system_usec;
                      int nr_periods;
                      int nr_throttled;
                      int throttled_usec;
                  } stat;
                  struct {
                      int weight;
                  } weight;
                  struct {
                      int max;
                      int period;
                  } max;
              } cpu;
              // pid
              union {
                  struct {
                      char active;
                      int max;
                  } max;
              } pid;
              // cpu_set
              union {
                  struct {
                      char active;
                      int cpu_id;
                  } set;
              } cpu_s;
              // freezer
              union {
                  struct {
                      int frozen;
                  } freezer;
              } frz;
              // memory
              union {
                  struct {
                      char active;
                  } stat;
                  struct {
                      char active;
                      unsigned int max;
                  } max;
              } mem;
          };
      };
  };
  struct file_operations f_op;
};

struct {
    struct spinlock lock;
    struct vfs_file file[NFILE];
} ftable;

struct inode_operations {
    int             (*dirlink) (struct vfs_inode*, char*, uint);
    struct vfs_inode*   (*dirlookup) (struct vfs_inode*, char*, uint*);
    struct vfs_inode*   (*idup) (struct vfs_inode*);
    void            (*ilock) (struct vfs_inode*);
    void            (*iput) (struct vfs_inode*);
    void            (*iunlock) (struct vfs_inode*);
    void            (*iunlockput) (struct vfs_inode*);
    void            (*iupdate) (struct vfs_inode*);
    int             (*readi) (struct vfs_inode*, char*, uint, uint);
    void            (*stati) (struct vfs_inode*, struct stat*);
    int             (*writei) (struct vfs_inode*, char*, uint, uint);
};

// in-memory copy of an inode
struct vfs_inode {
    uint dev;           // Device number
    uint inum;          // Inode number
    int ref;            // Reference count
    struct sleeplock lock; // protects everything below here
    int valid;          // inode has been read from disk?
    short type;         // copy of disk inode
    short major;
    short minor;
    short nlink;
    struct inode_operations i_op;
};


// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct vfs_inode*, char*, int);
  int (*write)(struct vfs_inode*, char*, int);
  int flags;
};

extern struct devsw devsw[];

#define CONSOLE 1

#endif
