; 1

start: ; 2
    lui a0 1 ; 3
    bne a0 zero skip ; 4
    nop ; 5

quit:
    addiu t0 zero data
    addiu t1 zero data2
    addiu t2 zero data3
    addiu t3 zero data4
    lb a0 [t0]
    lb a0 [t1]
    lb a0 [t2]
    lb a0 [t3]
    addiu v0 zero 0x4001
    syscall ; 6

skip:
    lui a1 1
    beq a0 a1 quit
    nop

data: db 101
data2: db 255
data3: db 36
data4: db 49
;comment
