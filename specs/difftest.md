Differential testing (difftest) is a technique of testing one implementation by comparing its behavior against another implementation. The design whose correctness we want to ensure is called the design under test (DUT), and the design known to be good is called the reference (REF). Here in designing a processor core, we are comparing the behavior of the RTL design against the behavior of an emulator running the same workload.

The most advantage of differential testing is that, it helps us to quickly identifying where the bug first happened. If DUT and REF behave differently at some point, it is very likely that the bug happens at or near it.

There are many different ways to do such a differential testing. The naive way is single stepping both designs and compare their full architectural state (including the program counter and general purpose registers) after each step. This is easy to implement for simple cases, but may fail to manage a complicated design. For example, for a pipelined core or a more complicated design, it can be difficult to define a concrete architectural state from the hardware's perspective.

Another way of differential testing is comparing the behavior of each instruction's retirement: what value is written into which register, what is the next program counter, etc. As instructions must retire in order, and the behavior of each instruction's retirement is well defined.

You can even do a "cyberg differential test" do verify part of your design. For example, you can build a "cyberg emulator" that uses Verilator to simulate your instruction decoder in RTL, and then the decoded instruction is executed by pure software emulation. The "cyberg emulator" is the DUT and a normal emulator is the REF. In this way you can verify your decoder. But the architecture of you emulator need to be flexible enough for you to replace part of it with simulated RTL.

Handling input is difficult in difftest. Peripherals are uncertain. You either need to run only workloads that do not read from peripherals during a difftest, or use a mechenism to ensure that DUT and REF get the exactly same input.

It is also helpful to log or trace some important events of your design, for example, exceptions, branches, jumps and peripheral operations. But also note that don't let it to be too verbose so useful information is flooded out.
