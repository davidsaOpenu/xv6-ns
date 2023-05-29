#ifndef XV6_DEVICE_H
#define XV6_DEVICE_H

#define LOOP_DEVICE_DEV (7)
#define DEV_TO_LOOP_DEVICE(dev) ((dev) & 0xffff)
#define LOOP_DEVICE_TO_DEV(ld) ((ld) | (LOOP_DEVICE_DEV << 16))
#define IS_LOOP_DEVICE(dev) (((dev) >> 16) == LOOP_DEVICE_DEV)

#define NLOOPDEVS (10)
#define NIDEDEVS (2)

struct device {
  struct superblock sb;
  int ref;
  struct inode *ip;
};

struct {
  struct spinlock lock; // protects loopdevs
  struct device loopdevs[NLOOPDEVS];
  struct superblock idesb[NIDEDEVS];
} dev_holder;

#endif /* XV6_DEVICE_H */
 