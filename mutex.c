#include "user.h"
#include "fcntl.h"
#include "mutex.h"

/* Init a unique mutex. */
int
mutex_init(mutex_t *mutex_var){
  int timeid;
  char * buffer = mutex_var->buffer;

  if ((timeid = uptime()) < 0){
    return MUTEX_UPTIME_ERROR;
  }

  strcpy(buffer, MUTEX_PREFIX);
  buffer += sizeof(MUTEX_PREFIX) - 1;

  itoa(buffer, uptime());
  sleep(1); // Skip tick - otherwise, we got the same tick in tests.

  return MUTEX_SUCCESS;
}

/* Locks a mutex if unlocked, sleep otherwise - res might indicates an error. */
int
mutex_lock(mutex_t *mutex_var){
  int res = -1;

  // pathname already exists and O_CREAT and O_EXCL were used
  while ((res = open(mutex_var->buffer, O_CREATE | O_EXCL)) == EEXIST){
      // Sleep to make it less busy
      sleep (10); 
  }

  return res;
}

/* Unlocks a mutex - returns according to unlink return values */
int
mutex_unlock(mutex_t *mutex_var){
  return unlink(mutex_var->buffer);
}
