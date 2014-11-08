[org 0x7c00]

mov bp, 0x7c00 ; Set stack pointer
mov sp, bp

; Load 32 sectors of data to 0x7e00
mov bx, 0x7e00 ; Write to 0x7e00
mov al, 0x08   ; Read 8 sectors
mov cl, 0x02   ; Start reading from sector 0x02, after the boot sector
call read_from_disk

mov bx, 0x8e00 ; Write to 0x8e00
mov al, 0x08   ; Read 8 sectors
mov cl, 0x0a   ; Start reading from sector 0x0a
call read_from_disk

mov bx, 0x9e00 ; Write to 0x9e00
mov al, 0x08   ; Read 8 sectors
mov cl, 0x12   ; Start reading from sector 0x12
call read_from_disk

mov bx, 0xae00 ; Write to 0xae00
mov al, 0x08   ; Read 8 sectors
mov cl, 0x1a   ; Start reading from sector 0x1a
call read_from_disk

jmp 0x7e28    ; Jump to the newly loaded code

; Read 'al' sectors starting at sector 'cl' and writing to 'bx'
read_from_disk:
    mov ah, 0x02  ; BIOS read sector function
    mov ch, 0x00  ; Select cylinder 0
    mov dh, 0x00  ; Select head 0
    int 0x13      ; BIOS interrupt

    jc disk_error_1 ; Write an error if BIOS reported one

    cmp al, 0x08  ; Check number of sectors actually read
    jne disk_error_2

    ret

; disk_error
disk_error_1:
  mov bx, DISK_ERROR_MSG
  call print_string
  jmp $

; disk_error_2
disk_error_2:
  mov bx, DISK_ERROR_2_MSG
  call print_string
  jmp $

; print_string
print_string:
  mov ah, 0x0e

write_char:
  mov al, [bx]
  cmp al, 0x00   ; Test for null terminator
  je end_print
  int 0x10       ; Print out character
  add bx, 1      ; Increment pointer
  jmp write_char

end_print:
  ret            ; End function

DISK_ERROR_MSG db "Disk read error! (1)", 0
DISK_ERROR_2_MSG db "Disk read error! (2)", 0

; Boot sector: 510 bytes of padding and magic number
times 510-($-$$) db 0
dw 0xaa55

; GDT
gdt_start:

gdt_null:           ; the mandatory null descriptor

    dd 0x0          ; ’dd ’ means define double word ( i.e. 4 bytes )
    dd 0x0

gdt_code:           ; the code segment descriptor
                    ; base =0x0 , limit =0 xfffff ,
                    ; 1st flags : ( present )1 ( privilege )00 ( descriptor type )1 -> 1001 b
                    ; type flags : ( code )1 ( conforming )0 ( readable )1 ( accessed )0 -> 1010 b
                    ; 2nd flags : ( granularity )1 (32 - bit default )1 (64 - bit seg )0 ( AVL )0 -> 1100 b
    dw 0xffff       ; Limit ( bits 0 -15)
    dw 0x0          ; Base ( bits 0 -15)
    db 0x0          ; Base ( bits 16 -23)
    db 10011010b    ; 1st flags , type flags
    db 11001111b    ; 2nd flags , Limit ( bits 16 -19)
    db 0x0          ; Base ( bits 24 -31)

gdt_data:           ; the data segment descriptor
                    ; Same as code segment except for the type flags :
                    ; type flags : ( code )0 ( expand down )0 ( writable )1 ( accessed )0 -> 0010 b
    dw 0xffff       ; Limit ( bits 0 -15)
    dw 0x0          ; Base ( bits 0 -15)
    db 0x0          ; Base ( bits 16 -23)
    db 10010010b    ; 1st flags , type flags
    db 11001111b    ; 2nd flags , Limit ( bits 16 -19)
    db 0x0          ; Base ( bits 24 -31)

gdt_end:            ; The reason for putting a label at the end of the
                    ; GDT is so we can have the assembler calculate
                    ; the size of the GDT for the GDT decriptor ( below )

; Define some handy constants for the GDT segment descriptor offsets , which
; are what segment registers must contain when in protected mode. For example,
; when we set DS = 0x10 in PM, the CPU knows that we mean it to use the
; segment described at offset 0x10 (i.e. 16 bytes) in our GDT, which in our
; case is the DATA segment (0x0 -> NULL ; 0x08 -> CODE ; 0x10 -> DATA )
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; Padding
dd 0x00000000
dd 0x00000000

; GDT descriptior
gdt_descriptor:
    dw gdt_end-gdt_start-1  ; Size of our GDT , always less one
                            ; of the true size
    dd gdt_start            ; Start address of our GDT

; Padding
dd 0xfefe

; Some interesting code!
mov bx, DISK_LOADED_MESSAGE
call print_string

cli ; Switch off interrupts

lgdt [gdt_descriptor]

mov eax, cr0   ; To make the switch to protected mode , we set
or eax, 0x1    ; the first bit of CR0 , a control register
mov cr0, eax   ; Update the control register

jmp CODE_SEG:start_protected_mode

[bits 32]

; Define some constants
VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

; prints a null - terminated string pointed to by EDX
print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY ; Set edx to the start of vid mem.

print_string_pm_loop:
    mov al, [ebx]            ; Store the char at EBX in AL
    mov ah, WHITE_ON_BLACK   ; Store the attributes in AH
    cmp al, 0                ; if (al == 0) , at end of string , so
    je print_string_pm_done  ; jump to done
    mov [edx], ax            ; Store char and attributes at current
                             ; character cell.
    add ebx, 1               ; Increment EBX to the next char in string.
    add edx, 2               ; Move to next character cell in vid mem.

    jmp print_string_pm_loop ; loop around to print the next char.

print_string_pm_done:
    popa
    ret                      ; Return from the function

start_protected_mode:

    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000   ; Update our stack position so it is right
    mov esp, ebp       ; at the top of the free space.

    mov ebx, PROTECTED_MODE_MESSAGE
    call print_string_pm

    jmp 0x8000

DISK_LOADED_MESSAGE db "Switching to 32-bit mode", 0
PROTECTED_MODE_MESSAGE db "Entered 32-bit mode successfully.", 0

; Padding up to 1024 bytes
times 1024-($-$$) db 0

; At this point the address is 0x8000
