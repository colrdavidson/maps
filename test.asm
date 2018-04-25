; 1

start: ; 2
    lui a0 1 ; 3
    bne a0 zero skip ; 4
    nop ; 5

quit:
    addiu a0 zero 10
    addiu v0 zero 0x4001
    syscall ; 6

skip:
    lui a1 1
    beq a0 a1 quit
    nop

; 7
