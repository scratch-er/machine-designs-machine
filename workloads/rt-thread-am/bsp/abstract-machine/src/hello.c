#include <rtthread.h>
#include <klib.h>

static int hello() {
  printf("Hello RISC-V!\n");
  return 0;
}
INIT_ENV_EXPORT(hello);
