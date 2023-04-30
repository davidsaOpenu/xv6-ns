#ifndef XV6_MOUNT_H
#define XV6_MOUNT_H
struct mount {
  struct mount *parent;
  struct inode *mountpoint;
  int ref;
  uint dev;
};

#define NMOUNT (200)

#endif /* XV6_MOUNT_H */
 