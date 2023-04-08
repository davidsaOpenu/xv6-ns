// Mutex that allows only one user to be in a locked (critical) section.
// However, unlock wakes up all processes that has been locked - means, supports up to two processes.
#include "types.h"
#include "defs.h"
#include "sleeplock.h"

struct mutex_s {
    // Allow future fields.
    struct sleeplock lock;
};

// Allocate a new mutex.
int
mutex_open(int ** mutex) {
    struct mutex_s * mutexp = 0;
    if ((mutexp = (struct mutex_s*)kalloc()) == 0) {
        goto bad;
    }

    initsleeplock(&mutexp->lock, "mutex");

    *mutex = (int *)mutexp;

    return 0;

bad:
    if (mutexp) {
        kfree((char *)mutexp);
    }

    return -1;
}

// Lock mutex so only can be in a locked sections.
int
mutex_lock(int ** mutex) {
    struct mutex_s * mutexp = (struct mutex_s *)*mutex;
    struct sleeplock * lock = &mutexp->lock;

    acquiresleep(lock);

    return 0;
}

// Wake up all process that slept on this mutex.
int
mutex_unlock(int ** mutex) {
    struct mutex_s * mutexp = (struct mutex_s *)*mutex;
    struct sleeplock * lock = &mutexp->lock;

    if (!holdingsleep(lock)) {
        goto bad;
    }

    releasesleep(lock);

    return 0;

bad:
    return -1;
}

// Clean resources.
int
mutex_close(int ** mutex) {
    if (mutex == 0 || *mutex == 0) {
        goto bad;
    }

    if (mutex_unlock(mutex) < 0) {
        goto bad;
    }

    kfree((char*)*mutex);
    *mutex = 0;

    return 0;

bad:
    return -1;
}
