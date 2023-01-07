#include "user.h"
#include "ipc.h"


/* User space mutex syscall wrappers */
int
delmutex(mutex* mtx)
{
  return umutex(mtx, MUTEX_DESTORY_OP, (char *)0);
}

int
mutex_lock(mutex* mtx)
{
  return umutex(mtx, MUTEX_LOCK_OP, (char *)0);
}

int
mutex_unlock(mutex* mtx)
{
  return umutex(mtx, MUTEX_UNLOCK_OP, (char *)0);
}

int
initmutex(mutex* mtx, char * mutex_name)
{
  return umutex(mtx, MUTEX_INIT_OP, mutex_name);
}