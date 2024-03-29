// Internal timer
//
// Copyright (C) 2008-2013  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "x86.h" // rdtscll()
#include "util.h" // timer_setup
#include "parisc/pdc.h"

// Setup internal timers.
void
timer_setup(void)
{
}

void
pmtimer_setup(u16 ioport)
{
}

// Return the number of milliseconds in 'ticks' number of timer irqs.
u32 ticks_to_ms(u32 ticks)
{
    return (10 * ticks / PAGE0->mem_10msec);
}


u32 ticks_from_ms(u32 ms)
{
    return (ms * PAGE0->mem_10msec / 10);
}


/****************************************************************
 * Internal timer reading
 ****************************************************************/

u32 TimerLast VARLOW;

// Sample the current timer value.
static u32
timer_read(void)
{
    return rdtscll();
}

// Check if the current time is past a previously calculated end time.
int
timer_check(u32 end)
{
    return (s32)(timer_read() - end) > 0;
}

static void
timer_sleep(u32 diff)
{
    u32 start = timer_read();
    u32 end = start + diff;
    while (!timer_check(end))
        /* idle wait */;
}

void ndelay(u32 count) {
    timer_sleep((count * PAGE0->mem_10msec / 10) / 1000 / 1000);
}
void udelay(u32 count) {
    timer_sleep((count * PAGE0->mem_10msec / 10) / 1000);
}
void mdelay(u32 count) {
    timer_sleep((count * PAGE0->mem_10msec / 10));
}

void nsleep(u32 count) {
    ndelay(count);
}
void usleep(u32 count) {
    udelay(count);
}
void msleep(u32 count) {
    mdelay(count);
}

// Return the TSC value that is 'msecs' time in the future.
u32
timer_calc(u32 msecs)
{
    return (msecs * PAGE0->mem_10msec / 10) + timer_read();
}
u32
timer_calc_usec(u32 usecs)
{
    return ((usecs * PAGE0->mem_10msec / 10) / 1000) + timer_read();
}

void
timer_calc2_ms(struct wait_t *w, u32 msecs)
{
    w->usecs = msecs / 1000;
    w->end = msecs % 1000;
    if (w->end == 0) {
        w->end = 1000;
        w->usecs -= 1;
    }
    w->end = timer_calc(w->end);
}

int
timer_check2(struct wait_t *w)
{
    s32 remain;

    /* return 1 if timed out */
    remain = (s32)(timer_read() - w->end);
    if (remain <= 0)
        return 0;
    if (w->usecs == 0)
        return 1;
    w->usecs -= 1;
    w->end = timer_calc(1000);
    return 0;
}

void
pit_setup(void)
{
}
