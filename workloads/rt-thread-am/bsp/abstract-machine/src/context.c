#include <am.h>
#include <rtthread.h>
#include <klib.h>

/* Saved by the CTE yield trap for a voluntary context switch. */
static Context *rt_switch_to     = NULL;
static Context **rt_switch_fromp = NULL;

static Context *ev_handler(Event e, Context *c) {
  switch (e.event) {
    case EVENT_YIELD:
      /* The ecall instruction is the yield; advance mepc so the switched-out
       * thread resumes at the instruction after ecall when it runs again. */
      c->mepc += 4;
      if (rt_switch_fromp) {
        *rt_switch_fromp = c;
      }
      return rt_switch_to;
    default:
      assert(0);
  }
  return c;
}

void __am_cte_init() {
  cte_init(ev_handler);
}

void rt_hw_context_switch_to(rt_ubase_t to) {
  rt_switch_to    = *(Context **)to;
  rt_switch_fromp = NULL;
  yield();
  /* Never reached. */
  __builtin_unreachable();
}

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to) {
  rt_switch_to    = *(Context **)to;
  rt_switch_fromp = (Context **)from;
  yield();
}

void rt_hw_context_switch_interrupt(rt_ubase_t from, rt_ubase_t to,
                                    rt_thread_t from_thread,
                                    rt_thread_t to_thread) {
  (void)from_thread;
  (void)to_thread;
  rt_hw_context_switch(from, to);
}

rt_uint8_t *rt_hw_stack_init(void *tentry, void *parameter,
                             rt_uint8_t *stack_addr, void *texit) {
  /* Allocate the initial context frame at the top of the thread stack.
   * Keep the stack pointer 16-byte aligned for the RISC-V ABI. */
  uintptr_t top = (uintptr_t)stack_addr;
  top -= sizeof(Context);
  top &= ~0xF;

  Context *ctx = (Context *)top;
  memset(ctx, 0, sizeof(Context));

  ctx->mepc    = (uintptr_t)tentry;
  ctx->mstatus = 0x00001800;          /* MPP = M-mode */
  ctx->gpr[2]  = top;                 /* sp */
  ctx->gpr[1]  = (uintptr_t)texit;    /* ra -> _thread_exit */
  ctx->gpr[10] = (uintptr_t)parameter;/* a0 */

  return (rt_uint8_t *)ctx;
}
