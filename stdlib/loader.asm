[bits 32]
[global flushStreams]
[global sleep]
[global exit]

flushStreams:
    push ebp
    mov ebp, esp
    push 0x00000000 ; Unused param
    push 0x00000002
    call 0x28:0
    leave
    ret

sleep:
    push ebp
    mov ebp, esp
    push dword [ebp+0x8] ; Sleep time
    push 0x00000003
    call 0x28:0
    leave
    ret

exit:
    push ebp
    mov ebp, esp
    push 0x00000000 ; Unused param
    push 0x00000004
    call 0x28:0
    leave
    ret
