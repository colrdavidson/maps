section text
; 1

start: ; 2
    addiu a0 zero 1 ; 3
    bne a0 zero skip ; 4
    nop ; 5

j quit
nop

quit:
    addiu a0 zero 0
    addiu v0 zero 4001
    syscall ; 6

skip:
    addiu a1 zero 1
    beq a0 a1 quit
    nop

section data

data: db 7
data2: db 255
data3: db 36
data4: db 49
;comment
