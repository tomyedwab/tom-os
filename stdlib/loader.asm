[bits 32]
[extern main]
[global puts]
call main
ret

puts:
    push ebp
    mov ebp, esp
    mov eax, [ebp+0x08] ; string ptr
    ;push 0x00000001
    ;push 0x00001234
    ;call 0x28:0
    leave
    ret

