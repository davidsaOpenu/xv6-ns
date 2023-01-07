#ifndef XV6_IPC_H
#define XV6_IPC_H

typedef enum mutex_operation_e
{
    MUTEX_INIT_OP = 1,
    MUTEX_UNLOCK_OP,
    MUTEX_LOCK_OP,
    MUTEX_DESTORY_OP
} mutex_operation_t;

#endif