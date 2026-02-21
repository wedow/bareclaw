; syscalls.inc — thin Linux x86-64 syscall wrappers
; Convention: args in rdi, rsi, rdx, r10, r8, r9; return in rax
; NOTE: the syscall instruction clobbers rcx and r11.

sys_read:
        mov eax, 0
        syscall
        ret

sys_write:
        mov eax, 1
        syscall
        ret

sys_open:
        mov eax, 2
        syscall
        ret

sys_close:
        mov eax, 3
        syscall
        ret

sys_mmap:
        mov eax, 9
        syscall
        ret

sys_munmap:
        mov eax, 11
        syscall
        ret

sys_pipe:
        mov eax, 22
        syscall
        ret

sys_dup2:
        mov eax, 33
        syscall
        ret

sys_nanosleep:
        mov eax, 35
        syscall
        ret

sys_fork:
        mov eax, 57
        syscall
        ret

sys_execve:
        mov eax, 59
        syscall
        ret

sys_wait4:
        mov eax, 61
        syscall
        ret

sys_mkdir:
        mov eax, 83
        syscall
        ret

sys_getdents64:
        mov eax, 217
        syscall
        ret

sys_clock_gettime:
        mov eax, 228
        syscall
        ret

sys_exit_group:
        mov eax, 231
        syscall
        ret

sys_getrandom:
        mov eax, 318
        syscall
        ret
