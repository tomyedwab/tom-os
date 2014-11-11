[bits 32]
[global puts]
[global flush_streams]

puts:
    push ebp
    mov ebp, esp
    push 0x00000000 ; Unused param
    push 0x00000001
    call 0x28:0
    leave
    ret

flush_streams:
    push ebp
    mov ebp, esp
    push 0x00000000 ; Unused param
    push 0x00000002
    call 0x28:0
    leave
    ret
