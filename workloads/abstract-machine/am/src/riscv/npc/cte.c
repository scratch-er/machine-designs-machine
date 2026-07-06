#include <am.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

#define CAUSE_ECALL_M   11
#define CAUSE_BREAKPOINT 3

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      case CAUSE_ECALL_M:   ev.event = EVENT_YIELD;   break;
      case CAUSE_BREAKPOINT: halt(1);                  break;
      default:              ev.event = EVENT_ERROR;   break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));
  user_handler = handler;
  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  Context *ctx = (Context *)((uintptr_t)kstack.end - sizeof(Context));
  memset(ctx, 0, sizeof(Context));
  ctx->mepc    = (uintptr_t)entry;
  ctx->mstatus = 0x00001800; // MPP = M-mode
  ctx->gpr[2]  = (uintptr_t)kstack.end; // sp
  ctx->gpr[10] = (uintptr_t)arg; // a0
  return ctx;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
  (void)enable;
}
