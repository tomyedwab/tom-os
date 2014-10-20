[bits 32]
[extern main]
[extern syscallHandler]
[extern excDoubleFaultHandler]
[extern excGeneralProtectionFault]
[extern excPageFault]
[extern reportInterruptHandler]
[extern handleIRQ]
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
[global irq_handler_00]
[global irq_handler_01]
[global irq_handler_02]
[global irq_handler_03]
[global irq_handler_04]
[global irq_handler_05]
[global irq_handler_06]
[global irq_handler_07]
[global irq_handler_08]
[global irq_handler_09]
[global irq_handler_10]
[global irq_handler_11]
[global irq_handler_12]
[global irq_handler_13]
[global irq_handler_14]
[global irq_handler_15]

call main
jmp $

user_process_jump:
    push ebp
    mov ebp, esp
    mov eax, [ebp+0x08] ; Directory
    mov ebx, [ebp+0x0c] ; Stack pointer
    mov ecx, [ebp+0x10] ; Target address

    mov cr3, eax ; Switch to user VMM

    mov ax, 0x23 ; Data segment
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
    mov eax, cr3
    push eax
    mov eax, 0x200000
    mov cr3, eax ; Switch to kernel VMM
    push dword [ebp+0x10] ; 2nd argument
    push dword [ebp+0x0c] ; 1st argument
    call syscallHandler
    add esp, 8
    pop eax
    pop ebp
    mov cr3, eax ; Restore user VMM
    retf 8

exc_df_handler:
    push 0x70
    call excDoubleFaultHandler
    add esp, 8 
    iret

exc_gp_handler:
    push 0x71
    call excGeneralProtectionFault
    add esp, 8 
    iret

exc_pf_handler:
    push 0x72
    call excPageFault
    add esp, 8 
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
    add esp, 8
    iret

int11_handler:
    push 0x0b
    call reportInterruptHandler
    add esp, 8
    iret

int12_handler:
    push 0x0c
    call reportInterruptHandler
    add esp, 8
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
    add esp, 8
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

irq_handler_00:
    push ebp
    push ebx
    mov ebx, 0x00
    jmp irq_handler

irq_handler_01:
    push ebp
    push ebx
    mov ebx, 0x01
    jmp irq_handler

irq_handler_02:
    push ebp
    push ebx
    mov ebx, 0x02
    jmp irq_handler

irq_handler_03:
    push ebp
    push ebx
    mov ebx, 0x03
    jmp irq_handler

irq_handler_04:
    push ebp
    push ebx
    mov ebx, 0x04
    jmp irq_handler

irq_handler_05:
    push ebp
    push ebx
    mov ebx, 0x05
    jmp irq_handler

irq_handler_06:
    push ebp
    push ebx
    mov ebx, 0x06
    jmp irq_handler

irq_handler_07:
    push ebp
    push ebx
    mov ebx, 0x07
    jmp irq_handler

irq_handler_08:
    push ebp
    push ebx
    mov ebx, 0x08
    jmp irq_handler

irq_handler_09:
    push ebp
    push ebx
    mov ebx, 0x09
    jmp irq_handler

irq_handler_10:
    push ebp
    push ebx
    mov ebx, 0x10
    jmp irq_handler

irq_handler_11:
    push ebp
    push ebx
    mov ebx, 0x11
    jmp irq_handler

irq_handler_12:
    push ebp
    push ebx
    mov ebx, 0x12
    jmp irq_handler

irq_handler_13:
    push ebp
    push ebx
    mov ebx, 0x13
    jmp irq_handler

irq_handler_14:
    push ebp
    push ebx
    mov ebx, 0x14
    jmp irq_handler

irq_handler_15:
    push ebp
    push ebx
    mov ebx, 0x15
    jmp irq_handler

irq_handler:
    mov ebp, esp

    push eax
    mov eax, cr3
    push eax
    mov eax, 0x200000
    mov cr3, eax ; Switch to kernel VMM

    push ebx
    call handleIRQ
    add esp, 4

    ; Send EOI
    mov al, 0x20
    out 0x20, al

    pop eax
    mov cr3, eax ; Restore user VMM
    pop eax
    pop ebx
    pop ebp
    iret
