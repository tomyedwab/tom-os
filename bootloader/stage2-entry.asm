[bits 32]
[extern load_kernel]

call load_kernel
jmp 0x010000
