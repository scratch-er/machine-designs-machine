#include <am.h>
#include <rtthread.h>

static int cmd_halt(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  halt(0);
  return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_halt, halt, halt the system);
