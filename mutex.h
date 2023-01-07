#ifndef XV6_MUTEX_H
#define XV6_MUTEX_H

#define MUTEX_NAME_LENGTH  (4)

// Mutual exclusion lock.
struct mutex {
  uint lock;       // Is the lock held?
  char name[4];        // Name of lock.
};

#endif
