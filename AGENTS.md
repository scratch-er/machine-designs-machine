Your goal is to design a RISC‑V CPU core. First, you need to write a RISC‑V simulator to serve as a reference model when testing the processor core. Then, you need to design the processor core in Verilog HDL and verify it against the simulator.

It is not enough for you to only make it work. Your design will be evaluated for PPA (Performance, Power, and Area), and you will need to optimize it. Also, this is a long-term project, so you need to make it easy to maintain and write good documentation.

The directory layout is:

- `emulator`: the project directory of your emulator.
- `core`: the project directory of the processor core.
- `notes`: the directory where you keep your notes. This is the only location for persistent information across sessions. `notes/todo.md` records what you have done and what you plan to do next; it should be updated frequently. `notes/plan.md` is your overall plan for the steps of this project. You are free to write any other notes, and you need to keep your notes neat.
- `workloads`: this is the directory containing software that runs on the emulator and new processor core.
- `playground`: this is the directory where you can freely expriment with ideas or add things that doesn't belong to `emulator`, `core`, `notes` and `workloads`.
- `specs`: the directory containing specifications. You should not modify anything inside it unless explicitly instructed. `specs/core.md` contains the specifications of the processor core you need to design. `specs/emulator.md` contains the specifications of the emulator you need to write. `specs/difftest.md` contains instructions on how to test the processor core. Other files and directories inside it contain RISC‑V specifications and other helpful documentation.
- `skills`: descriptions and scripts for your skills.
- `tools`: tools and scripts that support your skills.
- `data`: data and configuration files for the tools.

Most of the time, you should use your own skill mechanism instead of reading or executing things directly from `skills` and `tools`. You should not modify anything in `skills`, `tools`, or `data`, nor read anything in `data`, unless explicitly allowed by the user. However, you are encouraged to advise the user on adding to or improving things in these directories.

When making a git commit, use the following info:

- User name: "bot".
- Email: "iamabot@example.com"
