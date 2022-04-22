// Internal timer
//
// Copyright (C) 2008-2013  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "x86.h" // rdtscll()
#include "util.h" // timer_setup
#include "protos.h"

#define ALPHA_CLOCK_HZ  250

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
    return (ticks / ALPHA_CLOCK_HZ);
}


u32 ticks_from_ms(u32 ms)
{
    return (ms * ALPHA_CLOCK_HZ);
}


/****************************************************************
 * Internal timer reading
 ****************************************************************/

static inline long
ndelay_with_int(u32 nsec)
{
  register long a0 __asm__("16") = nsec;
  register long v0 __asm__("0");
  asm volatile ("call_pal 3" : "=r"(v0) : "r"(a0));
  return v0;
}

void
ndelay(u32 nsec)
{
  long left = nsec;
  do {
    left = ndelay_with_int(left);
  } while (left > 0);
}

void udelay(u32 count) {
    ndelay(count * 1000UL);
}
void mdelay(u32 count) {
    ndelay(count * 1000UL * 1000UL);
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

// Sample the current timer value.
static u32
timer_read(void)
{
    return get_wall_time();
}

// Check if the current time is past a previously calculated end time.
int
timer_check(u32 end)
{
    return (s64)(timer_read() - end) > 0;
}

// Return the TSC value that is 'msecs' time in the future.
u32
timer_calc(u32 msecs)
{
    return (msecs * 1000) + timer_read();
}

u32
timer_calc_usec(u32 usecs)
{
    return (usecs / 1000) + timer_read();
}

void
pit_setup(void)
{
}
