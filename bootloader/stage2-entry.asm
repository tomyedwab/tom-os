[bits 32]
[extern load_kernel]

call load_kernel
mov ebp, 0x90000 ; Move to the final kernel stack location
mov esp, ebp
jmp 0x010000
