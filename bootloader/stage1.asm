[org 0x7c00]

mov bp, 0x7c00 ; Set stack pointer
mov sp, bp

call read_from_disk

jmp 0x7e28    ; Jump to the newly loaded code

DAPACK:
    	db	0x10
	    db	0
blkcnt:	dw	0x20		; int 13 resets this to # of blocks actually read/written
db_add:	dw	0x7e00		; memory buffer destination address (0:7e00)
    	dw	0		    ; in memory page zero
d_lba:	dd	0x01		; put the lba to read in this spot
    	dd	0		    ; more storage bytes only for big lba's ( > 4 bytes )

; Read 32 sectors starting at sector 2 and writing to 0x7e00
read_from_disk:
 
	mov si, DAPACK		; address of "disk address packet"
	mov ah, 0x42		; AL is unused
	mov dl, 0x80		; drive number 0 (OR the drive # with 0x80)
	int 0x13
	jc short disk_error_1

    cmp word [blkcnt], 0x20  ; Check number of sectors actually read
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

; Load the memory map via the BIOS
mov di, 0x8800
mov es, di
mov di, 0x0010
call do_e820

; Some interesting code!
mov bx, DISK_LOADED_MESSAGE
call print_string

cli ; Switch off interrupts

lgdt [gdt_descriptor]

mov eax, cr0   ; To make the switch to protected mode , we set
or eax, 0x1    ; the first bit of CR0 , a control register
mov cr0, eax   ; Update the control register

jmp CODE_SEG:start_protected_mode

; use the INT 0x15, eax= 0xE820 BIOS function to get a memory map
; inputs: es:di -> destination buffer for 24 byte entries
; outputs: bp = entry count, trashes all registers except esi
do_e820:
	xor ebx, ebx		; ebx must be 0 to start
	xor bp, bp		; keep an entry count in bp
	mov edx, 0x0534D4150	; Place "SMAP" into edx
	mov eax, 0xe820
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes
	int 0x15
	jc short .failed	; carry set on first call means "unsupported function"
	mov edx, 0x0534D4150	; Some BIOSes apparently trash this register?
	cmp eax, edx		; on success, eax must have been reset to "SMAP"
	jne short .failed
	test ebx, ebx		; ebx = 0 implies list is only 1 entry long (worthless)
	je short .failed
	jmp short .jmpin
.e820lp:
	mov eax, 0xe820		; eax, ecx get trashed on every int 0x15 call
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes again
	int 0x15
	jc short .e820f		; carry set means "end of list already reached"
	mov edx, 0x0534D4150	; repair potentially trashed register
.jmpin:
	jcxz .skipent		; skip any 0 length entries
	cmp cl, 20		; got a 24 byte ACPI 3.X response?
	jbe short .notext
	test byte [es:di + 20], 1	; if so: is the "ignore this data" bit clear?
	je short .skipent
.notext:
	mov ecx, [es:di + 8]	; get lower dword of memory region length
	or ecx, [es:di + 12]	; "or" it with upper dword to test for zero
	jz .skipent		; if length qword is 0, skip entry
	inc bp			; got a good entry: ++count, move to next storage spot
	add di, 24
.skipent:
	test ebx, ebx		; if ebx resets to 0, list is complete
	jne short .e820lp
.e820f:
    mov bx, 0
    mov [es:bx], bp
	clc			; there is "jc" on end of list to this point, so the carry must be cleared
	ret
.failed:
	stc			; "function unsupported" error exit
	ret

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

    mov ebp, 0x88000   ; Update our stack position so it is right
    mov esp, ebp       ; at the top of the free space.

    mov ebx, PROTECTED_MODE_MESSAGE
    call print_string_pm

    jmp 0x8000

DISK_LOADED_MESSAGE db "Switching to 32-bit mode", 0
PROTECTED_MODE_MESSAGE db "Entered 32-bit mode successfully.", 0

; Padding up to 1024 bytes
times 1024-($-$$) db 0

; At this point the address is 0x8000
