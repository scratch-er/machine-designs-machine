#include <am.h>

#define CLINT_BASE 0x02000000
#define MTIME_LO   (*(volatile uint32_t *)(CLINT_BASE + 0xBFF8))
#define MTIME_HI   (*(volatile uint32_t *)(CLINT_BASE + 0xBFFC))

// Assume 100 MHz: 1 cycle = 10 ns = 0.01 us
#define CYCLES_PER_US 100

void __am_timer_init() {
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uint32_t hi, lo;
  do {
    hi = MTIME_HI;
    lo = MTIME_LO;
  } while (hi != MTIME_HI);
  uint64_t cycles = ((uint64_t)hi << 32) | lo;
  uptime->us = cycles / CYCLES_PER_US;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 0;
}
