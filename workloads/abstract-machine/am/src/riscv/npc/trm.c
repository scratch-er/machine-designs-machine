#include <am.h>
#include <klib-macros.h>

extern char _heap_start;
extern char _pmem_start;
int main(const char *args);
#define PMEM_SIZE (1 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_pmem_start + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER);

#define UART_BASE 0x10000000

void putch(char ch) {
  *(volatile char *)UART_BASE = ch;
}

void halt(int code) {
  (void)code;
  asm volatile("ebreak");
  while (1);
}

void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}
