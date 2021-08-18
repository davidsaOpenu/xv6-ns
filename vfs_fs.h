#ifndef XV6_VFS_FS_H
#define XV6_VFS_FS_H

//struct file_system_type;
#include "types.h"

//struct vfs_inode* vfs_idup(struct vfs_inode *ip);
//void              vfs_iput(struct vfs_inode *ip);

//typedef unsigned int size_t;


struct vfs_superblock {
    uint ninodes;      // Number of inodes.
//  struct file_system_type * type;
};

// On-disk inode structure
struct vfs_dinode {
    short type;           // File type
    short major;          // Major device number (T_DEV only)
    short minor;          // Minor device number (T_DEV only)
    short nlink;          // Number of links to inode in file system
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + (sb).inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + (sb).bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define offsetof(TYPE, MEMBER) ((unsigned int) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({              \
const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
(type *)( (char *)__mptr - offsetof(type,member) );})


struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

#endif
