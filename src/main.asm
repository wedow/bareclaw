format ELF64 executable 3

segment readable executable

entry $
        mov eax, 231
        xor edi, edi
        syscall
