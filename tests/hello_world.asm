main:
    addiu a0 zero 0

    lui at msg
    ori a1 at msg

    lui at msg_end
    ori t1 at msg_end

    sub a2 a1 t1

    addiu v0 zero 4004
    syscall

    addiu a0 zero 0
    addiu v0 zero 4001
    syscall

msg:
    db "Hello World!"
    db 0xa
    db 0
msg_end:
