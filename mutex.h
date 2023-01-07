#ifndef XV6_MUTEX_H
#define XV6_MUTEX_H
#include "ipc.h"

#define MUTEX_NAME_LENGTH  (4)
#define MUTEX_LOCK (1)
#define MUTEX_UNLOCK (0)

// Mutual exclusion lock.
struct mutex {
  // Specifies the status of the lock(MUTEX_LOCK/UNLOCK)
  uint lock;
  // Name of the mutex used for debugging and future features
  char name[4];
};

#endif
