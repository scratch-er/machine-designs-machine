.section .text
.globl _start
_start:
    # x3 = UART base address
    lui     x3, 0x10000

    # Print "PASS\n" one byte at a time
    li      x1, 'P'
    sb      x1, 0(x3)
    li      x1, 'A'
    sb      x1, 0(x3)
    li      x1, 'S'
    sb      x1, 0(x3)
    li      x1, 'S'
    sb      x1, 0(x3)
    li      x1, '\n'
    sb      x1, 0(x3)

    # Test finish marker
    ebreak
