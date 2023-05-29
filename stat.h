#ifndef XV6_STAT_H
#define XV6_STAT_H

#define T_DIR       1   // Directory
#define T_FILE      2   // File
#define T_DEV       3   // Device
#define T_CGFILE    4   // Cgroup file
#define T_CGDIR     5   // Cgroup directory
#define T_PROCFILE  6   // Proc file
#define T_PROCDIR   7   // Proc directory

struct stat {
  short type;  // Type of file
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short nlink; // Number of links to file
  uint size;   // Size of file in bytes
};

#endif /* XV6_STAT_H */

