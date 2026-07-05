# Core Specifications

## Basic Configuration

Supported instruction set: RV32E_Zicsr.

Supported privilege level: M-mode only.

Core count: Single core. The microarchitecture design only needs to consider single-core scenarios.

Reset address: Configurable parameter, default is `0x20000000`.

## Privileged Instructions to Implement

Implement ecall, ebreak, mret, and wfi instructions, where wfi can be implemented as nop.

## CSRs to Implement

- mvendorid: Hardcoded to 0.
- marchid: Hardcoded to 0.
- mepc: Provide an implementation sufficient to hold all 32-bit addresses aligned to 4 bytes.
- mtvec: Provide an implementation sufficient to hold all 32-bit addresses aligned to 4 bytes.
- mcause: Provide an implementation sufficient to represent all interrupts and exceptions this core may encounter.
- mstatus: MPP hardcoded to M-mode, all other bits hardcoded to 0.

All other CSRs are unimplemented. Attempting to access these CSRs should trigger an illegal instruction exception.

## Memory Access Behavior

Memory access is in-order, fence instruction is implemented as nop.

Virtual memory and address translation: Not supported.

Bus: Only supports address-aligned little-endian memory access, unaligned memory access generates load/store/instruction misaligned fault. Externally uses 32-bit wide AXI 4 protocol. AXI returning SLVERR/DECERR generates load/store/instruction access fault.

PMP and PMA: Not supported, entire address space is open for access.

## Interrupt and Exception Behavior

Supports the following types of exceptions (sync exception):

- Instruction address misaligned
- Instruction access fault
- Illegal instruction
- Breakpoint
- Load address misaligned
- Load access fault
- Store/AMO address misaligned
- Store/AMO access fault
- Environment call from M-mode
  
Does not support interrupts (async interrupt).

## Built-in CLINT

The core needs a built-in CLINT, which only implements `mtime` and `mtimeh` to provide timing functionality, and does not provide interrupt functionality. Reading from or writing to `mtimecmp`, `mtimecmph`, and `msip` is ignored (no actual effect and no error, read content is undefined). The timer should increment by 1 each cycle. The built-in CLINT mapped address is a configurable parameter, default is `0x02000000` to `0x0200ffff`.

# Cache Configuration

Implement an instruction cache using flip-flops with capacity of 8 instructions (32 bytes), cacheline size of 16 bytes, associativity of 1. The instruction cache assumes all instruction-fetchable addresses are cacheable.

The instruction cache needs to support burst transfers.

fence.i instruction is implemented as clearing the instruction cache.

Data cache is not supported.

Internal performance counters are needed to calculate icache AMAT.

# Ports of Top Module

Currently, `io_interrupt` and all AXI slave interfaces are reserved. For reserved input interfaces, their inputs should not be used. For reserved output interfaces, their outputs should be hardcoded to 0.

If you are using Chisel, according to the FIRRTL ABI, you only need to define the module as following, and the resulting Verilog module will comply to the tables below:

```scala
class YourTopModuleName extends Module {
    // clock and reset will be automatically added
    val io = IO(new Bundle {
        val io_interrupt = Input(UInt(1.W))
        val master = // some bundle representing an AXI master interface
        val slave = // some bundle representing an AXI slave interface
    })
    // ...
}
```


## General Signals

| Name          | Direction | Width | Description                         |
|---------------|-----------|-------|-------------------------------------|
| clock         | input     | 1     | System clock                        |
| reset         | input     | 1     | Reset (active high)                 |
| io_interrupt  | input     | 1     | External interrupt request          |

## AXI Master

| Name                 | Direction | Width | Description                  |
|----------------------|-----------|-------|------------------------------|
| io_master_awready    | input     | 1     | Write address ready          |
| io_master_awvalid    | output    | 1     | Write address valid          |
| io_master_awaddr     | output    | 32    | Write address                |
| io_master_awid       | output    | 4     | Write address ID             |
| io_master_awlen      | output    | 8     | Write burst length           |
| io_master_awsize     | output    | 3     | Write burst size             |
| io_master_awburst    | output    | 2     | Write burst type             |
| io_master_wready     | input     | 1     | Write data ready             |
| io_master_wvalid     | output    | 1     | Write data valid             |
| io_master_wdata      | output    | 32    | Write data                   |
| io_master_wstrb      | output    | 4     | Write strobes                |
| io_master_wlast      | output    | 1     | Write last beat              |
| io_master_bready     | output    | 1     | Write response ready         |
| io_master_bvalid     | input     | 1     | Write response valid         |
| io_master_bresp      | input     | 2     | Write response status        |
| io_master_bid        | input     | 4     | Write response ID            |
| io_master_arready    | input     | 1     | Read address ready           |
| io_master_arvalid    | output    | 1     | Read address valid           |
| io_master_araddr     | output    | 32    | Read address                 |
| io_master_arid       | output    | 4     | Read address ID              |
| io_master_arlen      | output    | 8     | Read burst length            |
| io_master_arsize     | output    | 3     | Read burst size              |
| io_master_arburst    | output    | 2     | Read burst type              |
| io_master_rready     | output    | 1     | Read data ready              |
| io_master_rvalid     | input     | 1     | Read data valid              |
| io_master_rresp      | input     | 2     | Read response status         |
| io_master_rdata      | input     | 32    | Read data                    |
| io_master_rlast      | input     | 1     | Read last beat               |
| io_master_rid        | input     | 4     | Read data ID                 |

## AXI Slave

| Name                | Direction | Width | Description                   |
|---------------------|-----------|-------|-------------------------------|
| io_slave_awready    | output    | 1     | Write address ready           |
| io_slave_awvalid    | input     | 1     | Write address valid           |
| io_slave_awaddr     | input     | 32    | Write address                 |
| io_slave_awid       | input     | 4     | Write address ID              |
| io_slave_awlen      | input     | 8     | Write burst length            |
| io_slave_awsize     | input     | 3     | Write burst size              |
| io_slave_awburst    | input     | 2     | Write burst type              |
| io_slave_wready     | output    | 1     | Write data ready              |
| io_slave_wvalid     | input     | 1     | Write data valid              |
| io_slave_wdata      | input     | 32    | Write data                    |
| io_slave_wstrb      | input     | 4     | Write strobes                 |
| io_slave_wlast      | input     | 1     | Write last beat               |
| io_slave_bready     | input     | 1     | Write response ready          |
| io_slave_bvalid     | output    | 1     | Write response valid          |
| io_slave_bresp      | output    | 2     | Write response status         |
| io_slave_bid        | output    | 4     | Write response ID             |
| io_slave_arready    | output    | 1     | Read address ready            |
| io_slave_arvalid    | input     | 1     | Read address valid            |
| io_slave_araddr     | input     | 32    | Read address                  |
| io_slave_arid       | input     | 4     | Read address ID               |
| io_slave_arlen      | input     | 8     | Read burst length             |
| io_slave_arsize     | input     | 3     | Read burst size               |
| io_slave_arburst    | input     | 2     | Read burst type               |
| io_slave_rready     | input     | 1     | Read data ready               |
| io_slave_rvalid     | output    | 1     | Read data valid               |
| io_slave_rresp      | output    | 2     | Read response status          |
| io_slave_rdata      | output    | 32    | Read data                     |
| io_slave_rlast      | output    | 1     | Read last beat                |
| io_slave_rid        | output    | 4     | Read data ID                  |
