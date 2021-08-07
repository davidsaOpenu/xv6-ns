#include "types.h"

#ifndef CLOCK_H
#define CLOCK_H

#define JIFFY 100 // Hz
#define MILLISECOND 1000 //Hz
#define MICROSECOND (1000 * MILLISECOND)

typedef uint timestamp[2];

// clockasm.S
uint ticks_now(void); // in jiffies
uint steady_clock_now(void); // in milliseconds
void timestamp_now(timestamp ts); // in microseconds from now
uint us_since_ts(timestamp ts); // microseconds from timestamp

// clock.c
void clockinit(void);
void timerinit(void);
void timerintr(void);
extern uint timer_freq;
extern struct spinlock tickslock;
extern uint ticks;
extern uint alarm;

#endif