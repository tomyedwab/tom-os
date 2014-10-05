[bits 32]
[extern main]
[extern syscallHandler]
[extern reportInterruptHandler]
[global syscall_handler_asm]
[global gpf_handler_asm]

call main
jmp $

syscall_handler_asm:
    push ebx
    push eax
    call syscallHandler
    add esp, 8
    iret

gpf_handler_asm:
    call reportInterruptHandler
    iret
