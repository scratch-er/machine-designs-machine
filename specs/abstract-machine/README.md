Abstract machine (AM) is an educational abstract layer for operating systems. When developing this project, you will need to run several programs ported to the AM framework on your emulator and processor core. I have already checked out AM's source tree at `workloads/abstract-machine`. When building a program ported to AM, you need to run `make AM_HOME=... ARCH=...`. Here, `AM_HOME` is the absolute path to the AM source tree, and `ARCH` is `riscv32e-npc` for this project.

As an educational abstract layer, some critical parts are intentionally left blank for you to port. There is also a ported target called `native`, it is running AM program as a native program under current OS. The `native` port is primarily meant for you to try, to debug or to test a program running on AM quickly. You need to port AM to `riscv32e-npc`, which is the target matching the processor you are designing. Here is what you need to port:

- Implement TRM.
- Implement UART and uptime timer in IOE. You can assume that the processor runs at 100MHz.
- Implement CTE.
- Implement kilb. You can directly use the implementation of a libc in [Sonnet libc](https://gitlink.org.cn/foobat/sonnet-libc).
- Refactor build system so it uses clang. Currently, it assumes that you are using GCC.

I have checked out a set of programs running on AM at `workloads/am-kernels`. You can build and run these programs to test your AM port, but do not modify them.
