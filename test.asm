start:
    lui a0 1
    bne a0 zero skip
    nop

quit:
    addiu a0 zero 10
    addiu v0 zero 0x4001
    syscall

skip:
    lui a1 1
    beq a0 a1 quit
    nop
