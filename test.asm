    lui a0 1
    bne a0 zero skip
    nop

quit:
    addiu a0 zero 0
    addiu v0 zero 0x4001
    syscall

skip:
    j quit
    nop
