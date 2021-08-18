#ifndef XV6_FS_H
#define XV6_FS_H

// On-disk file system format.
// Both the kernel and user programs use this header file.

#include "vfs_fs.h"

#define ROOTINO 1  // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
    uint size;         // Size of file system image (blocks)
    uint nblocks;      // Number of data blocks
    uint nlog;         // Number of log blocks
    uint logstart;     // Block number of first log block
    uint inodestart;   // Block number of first inode block
    uint bmapstart;    // Block number of first free map block
    struct vfs_superblock vfs_sb;
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
    struct vfs_dinode vfs_dinode;
    uint size;            // Size of file (bytes)
    uint addrs[NDIRECT + 1];   // Data block addresses
};

//struct file_system_type fs_type {
//    name = "fs"
//};

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

#endif
