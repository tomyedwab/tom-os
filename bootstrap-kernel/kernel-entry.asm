[bits 32]
[extern main]
[extern syscallHandler]
[extern excDoubleFaultHandler]
[extern excGeneralProtectionFault]
[extern excPageFault]
[extern reportInterruptHandler]
[global user_process_jump]
[global syscall_handler]
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
[global irq_handler]

call main
jmp $

user_process_jump:
    push ebp
    mov ebp, esp
    mov eax, [ebp+0x08] ; Directory
    mov cr3, eax
    mov ebx, [ebp+0x0c] ; Stack pointer
    mov ecx, [ebp+0x10] ; Target address

    mov ax, 0x23 ; Data segment
    mov ax, 0x10 ; Data segment (level 0)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23 ; User data segment + level 3
    push ebx  ; Stack
    pushf
    push 0x1b ; User code segment + level 3
    push ecx
    iret

syscall_handler:
    push ebp
    mov ebp, esp
    push 0x01
    push 0x1234
    call syscallHandler
    add esp, 8
    pop ebp
    ret

exc_df_handler:
    pop ebx
    jmp $
    push 0x70
    call excDoubleFaultHandler
    add esp, 8 
    iret

exc_gp_handler:
    pop ebx
    jmp $
    push 0x71
    call excGeneralProtectionFault
    add esp, 8 
    iret

exc_pf_handler:
    pop ebx
    jmp $
    push 0x72
    call excPageFault
    add esp, 8 
    iret

int00_handler:
    jmp $
    push 0x00
    call reportInterruptHandler
    add esp, 4
    iret

int01_handler:
    jmp $
    push 0x01
    call reportInterruptHandler
    add esp, 4
    iret

int02_handler:
    jmp $
    push 0x02
    call reportInterruptHandler
    add esp, 4
    iret

int03_handler:
    jmp $
    push 0x03
    call reportInterruptHandler
    add esp, 4
    iret

int04_handler:
    jmp $
    push 0x04
    call reportInterruptHandler
    add esp, 4
    iret

int05_handler:
    jmp $
    push 0x05
    call reportInterruptHandler
    add esp, 4
    iret

int06_handler:
    jmp $
    push 0x06
    call reportInterruptHandler
    add esp, 4
    iret

int07_handler:
    jmp $
    push 0x07
    call reportInterruptHandler
    add esp, 4
    iret

int09_handler:
    jmp $
    push 0x09
    call reportInterruptHandler
    add esp, 4
    iret

int10_handler:
    pop ebx
    jmp $
    push 0x0a
    call reportInterruptHandler
    add esp, 8
    iret

int11_handler:
    pop ebx
    jmp $
    push 0x0b
    call reportInterruptHandler
    add esp, 8
    iret

int12_handler:
    pop ebx
    jmp $
    push 0x0c
    call reportInterruptHandler
    add esp, 8
    iret

int14_handler:
    jmp $
    push 0x0e
    call reportInterruptHandler
    add esp, 4
    iret

int15_handler:
    jmp $
    push 0x0f
    call reportInterruptHandler
    add esp, 4
    iret

int16_handler:
    jmp $
    push 0x10
    call reportInterruptHandler
    add esp, 4
    iret

int17_handler:
    pop ebx
    jmp $
    push 0x11
    call reportInterruptHandler
    add esp, 8
    iret

int18_handler:
    jmp $
    push 0x12
    call reportInterruptHandler
    add esp, 4
    iret

int19_handler:
    jmp $
    push 0x13
    call reportInterruptHandler
    add esp, 4
    iret

int20_handler:
    jmp $
    push 0x14
    call reportInterruptHandler
    add esp, 4
    iret

irq_handler:
    ; Send EOI
    mov eax, 0x20
    out 0x20, eax
    iret
