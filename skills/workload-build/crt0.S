.section .text._start
.globl _start

_start:
    la      sp, __stack_top
    call    main
    ebreak
