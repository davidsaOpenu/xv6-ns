#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "clock.h"

// lapic.c
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
extern volatile uint *lapic;
void lapicw(int index, int value);

// clockasm.S
uint tscfreq(void);
uint picfreq(volatile uint *addr);

uint cycles_per_jiffy;
uint cycles_per_millisecond;
uint cycles_per_microsecond;
uint timer_freq = 0;

struct spinlock tickslock;
uint ticks;
uint alarm;

#define SAMPLES 10
#define PIT_INTERVAL 10000 // microseconds

static uint *
minsamp(uint *samples)
{
    uint ms = ~0, *sp = 0;
    for(int i = 0; i < SAMPLES; i++)
        if(samples[i] && samples[i] < ms){
            ms = samples[i];
            sp = samples + i;
        }
    return sp;
}

void
clockinit(void)
{
    uint i, samples[SAMPLES];
    for(i = 0; i < SAMPLES; i++)
        samples[i] = tscfreq();
    for(i = 0; i < (SAMPLES - 1) / 2; i++)
        *minsamp(samples) = 0;
    cycles_per_microsecond = (*minsamp(samples) + (PIT_INTERVAL / 2)) / PIT_INTERVAL;
    cycles_per_millisecond = cycles_per_microsecond * MILLISECOND;
    cycles_per_jiffy = (cycles_per_millisecond / JIFFY) * MILLISECOND;
}

void
timerinit(void)
{
    uint i, samples[SAMPLES];
    for(i = 0; i < SAMPLES; i++)
        samples[i] = picfreq(lapic + TCCR);
    for(i = 0; i < (SAMPLES - 1) / 2; i++)
        *minsamp(samples) = 0;
    timer_freq = *minsamp(samples);
    initlock(&tickslock, "tickslock");
    ticks = 0;
    alarm = ~0;
}

void timerintr(void)
{
    uint tscticks, picticks;
    tscticks = ticks_now();
    acquire(&tickslock);
    if(cpuid() == 0){
        ticks++;
        if(alarm <= ticks){
            alarm = ~0;
            wakeup(&ticks);
        }
    }
    picticks = ticks;
    release(&tickslock);
    if(picticks < tscticks)
        lapicw(TICR, timer_freq - (timer_freq / 10));
    else if(picticks > tscticks)
        lapicw(TICR, timer_freq + (timer_freq / 10));
    else
        lapicw(TICR, timer_freq);
}