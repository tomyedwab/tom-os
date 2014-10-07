[bits 32]
[extern main]
[extern syscallHandler]
[extern excDoubleFaultHandler]
[extern excGeneralProtectionFault]
[extern excPageFault]
[extern reportInterruptHandler]
[global syscall_handler_asm]
[global exc_df_handler]
[global exc_gp_handler]
[global exc_pf_handler]
[global int00_handler]
[global int01_handler]
[global int02_handler]
[global int03_handler]
[global int04_handler]
[global int05_handler]
[global int06_handler]
[global int07_handler]
[global int09_handler]
[global int10_handler]
[global int11_handler]
[global int12_handler]
[global int15_handler]
[global int16_handler]
[global int17_handler]
[global int18_handler]
[global int19_handler]
[global int20_handler]

call main
jmp $

syscall_handler_asm:
    push ebx
    push eax
    call syscallHandler
    add esp, 8
    iret

exc_df_handler:
    call excDoubleFaultHandler
    iret

exc_gp_handler:
    call excGeneralProtectionFault
    iret

exc_pf_handler:
    call excPageFault
    iret

int00_handler:
    push 0x00
    call reportInterruptHandler
    add esp, 4
    iret

int01_handler:
    push 0x01
    call reportInterruptHandler
    add esp, 4
    iret

int02_handler:
    push 0x02
    call reportInterruptHandler
    add esp, 4
    iret

int03_handler:
    push 0x03
    call reportInterruptHandler
    add esp, 4
    iret

int04_handler:
    push 0x04
    call reportInterruptHandler
    add esp, 4
    iret

int05_handler:
    push 0x05
    call reportInterruptHandler
    add esp, 4
    iret

int06_handler:
    push 0x06
    call reportInterruptHandler
    add esp, 4
    iret

int07_handler:
    push 0x07
    call reportInterruptHandler
    add esp, 4
    iret

int09_handler:
    push 0x09
    call reportInterruptHandler
    add esp, 4
    iret

int10_handler:
    push 0x0a
    call reportInterruptHandler
    add esp, 4
    iret

int11_handler:
    push 0x0b
    call reportInterruptHandler
    add esp, 4
    iret

int12_handler:
    push 0x0c
    call reportInterruptHandler
    add esp, 4
    iret

int14_handler:
    push 0x0e
    call reportInterruptHandler
    add esp, 4
    iret

int15_handler:
    push 0x0f
    call reportInterruptHandler
    add esp, 4
    iret

int16_handler:
    push 0x10
    call reportInterruptHandler
    add esp, 4
    iret

int17_handler:
    push 0x11
    call reportInterruptHandler
    add esp, 4
    iret

int18_handler:
    push 0x12
    call reportInterruptHandler
    add esp, 4
    iret

int19_handler:
    push 0x13
    call reportInterruptHandler
    add esp, 4
    iret

int20_handler:
    push 0x14
    call reportInterruptHandler
    add esp, 4
    iret
