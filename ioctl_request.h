#ifndef XV6_IOCTL_REQUEST
#define XV6_IOCTL_REQUEST

typedef enum ioctl_request {
    IOCTL_CPU_START = 1000,
    IOCTL_GET_PROCESS_CPU_TIME,
    IOCTL_GET_PROCESS_CPU_PERCENT,
    IOCTL_GET_TIME, // NEED TO DELLETE - temp for debugging
} ioctl_request;

#endif
