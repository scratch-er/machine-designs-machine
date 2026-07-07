#include <am.h>
#include <rtthread.h>
#include <klib.h>

/* The target core has no asynchronous interrupts, so interrupt
 * enable/disable are no-ops.  rt_base_t is used as the interrupt
 * lock state token. */

rt_base_t rt_hw_interrupt_disable(void) {
  return 0;
}

void rt_hw_interrupt_enable(rt_base_t level) {
  (void)level;
}
