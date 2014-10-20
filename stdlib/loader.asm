[bits 32]
[extern main]
[global puts]
call main
jmp $

puts:
    push ebp
    mov ebp, esp
    push 0x00000000 ; Unused param
    push 0x00000001
    call 0x28:0
    leave
    ret
