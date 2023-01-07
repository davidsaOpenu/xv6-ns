// Mutual exclusion syscall implementation

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "console.h"
#include "mutex.h"

typedef struct mutex_table_entry_s
{
    // mutex_id should never be 0
    int mutex_id;
    struct mutex mtx;
} mutex_table_entry_t;

mutex_table_entry_t mtx_table_g[MUTEX_TABLE_SIZE] = {0};
int mutexes_counter_g = 0;

/* universal mutex function */
int umutex(struct mutex * mtx, mutex_operation_t mutex_op, char * mutex_name)
{
    switch (mutex_op)
    {
        case MUTEX_INIT_OP:
            return initmutex(mtx, mutex_name);
            break;
        case MUTEX_LOCK_OP:
            return mutex_lock(*(struct mutex **)mtx);
            break;
        case MUTEX_UNLOCK_OP:
            return mutex_unlock(*(struct mutex **)mtx);
            break;
        case MUTEX_DESTORY_OP:
            return delmutex(*(struct mutex **)mtx);
            break;
        default:
            return -1;
    }
    return 0;
}


int initmutex(struct mutex * mtx, char * mutex_name)
{
    int size = 0;
    int i = 0;

    /* Note: We do not check if the mutex already exists.
    Its the user responsibility to do so
    */
    if(mtx == 0 || mutex_name == 0)
        return -1;

    size = strlen(mutex_name);

    if(size == 0)
        return -1;

    if(size > MUTEX_NAME_LENGTH)
        size = MUTEX_NAME_LENGTH;

    if(mutexes_counter_g >= MUTEX_TABLE_SIZE - 1)
        return -1;

    for(i = 0; i < MUTEX_TABLE_SIZE; i++)
    {
        if(mtx_table_g[i].mutex_id == 0)
        {
            //found empty entry, assign the user space mtx address
            mtx_table_g[i].mutex_id = (int)mtx;
            mtx_table_g[i].mtx.lock = MUTEX_UNLOCK;
            strncpy(mtx_table_g[i].mtx.name, mutex_name, size);

            mutexes_counter_g++;
            // hold the kernel address in the mutex so we can access the mutex easier from kernel
            /* Note: the mutex structure in userspace (user.h) is actually a typedef of int
                Only in creation we treat it that way to assign it a pointer of the real structure in kernel*/
            *(int *)mtx = (int)&(mtx_table_g[i].mtx);
            return 0;
        }
    }

    return -1;
}

int delmutex(struct mutex * mtx)
{
    int i = 0;

    if(mtx == 0)
        return -1;

    for(i = 0; i < MUTEX_TABLE_SIZE; i++)
    {
        // we compare pointers
        if(&(mtx_table_g[i].mtx) == mtx)
        {
            // set id to 0 and clear mutex struct
            mtx_table_g[i].mutex_id = 0;
            memset(&(mtx_table_g[i].mtx), 0, sizeof(struct mutex));
            mutexes_counter_g--;
            return 0;
        }
    }

    return -1;
}

int mutex_lock(struct mutex * mtx)
{
    // validate input
    if(mtx == 0)
    {
        return -1;
    }

    //minimal validation
    if(mtx->name == 0)
    {
        return -1;
    }

    //only when we succeed locking we exit the loop
    while(xchg(&(mtx->lock), MUTEX_LOCK) != MUTEX_UNLOCK)
    {
        //make the scheduler run, we don't want to busy loop
        yield();
    }

    // We get here only after we have the lock, refer to spinlock.c for exaplaination
    __sync_synchronize();
    return 0;
}

int mutex_unlock(struct mutex * mtx)
{
    // validate input
    if(mtx == 0)
        return -1;

    //minimal validation
    if(mtx->name == 0)
        return -1;

    // refer to spinlock.c for exaplaination
    __sync_synchronize();
    asm volatile("lock; movl $0, %0" : "+m" (mtx->lock) : );

    return 0;
}

