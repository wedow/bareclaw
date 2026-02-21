format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/syscalls.inc'

segment readable executable

entry $

; --- Test 1: sys_write via pipe, verify return = byte count ---
        test_begin "sys_write returns byte count"
        ; create a pipe: pipe([pipefd])
        lea rdi, [pipefd]
        call sys_pipe
        ; write 5 bytes to write-end
        mov edi, dword [pipefd + 4]
        lea rsi, [hello]
        mov edx, 5
        call sys_write
        ; close both ends
        push rax
        mov edi, dword [pipefd]
        call sys_close
        mov edi, dword [pipefd + 4]
        call sys_close
        pop rax
        assert_eq rax, 5

; --- Test 2: sys_getrandom returns requested length ---
        test_begin "sys_getrandom returns requested length"
        lea rdi, [randbuf]
        mov esi, 16
        xor edx, edx          ; flags = 0
        call sys_getrandom
        assert_eq rax, 16

; --- Test 3: sys_mmap + write + read + sys_munmap ---
        test_begin "mmap anon, write, read, munmap"
        ; mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
        xor edi, edi           ; addr = NULL
        mov esi, 4096          ; length
        mov edx, 3             ; PROT_READ|PROT_WRITE
        mov r10d, 0x22         ; MAP_PRIVATE|MAP_ANONYMOUS
        mov r8d, -1            ; fd = -1
        xor r9d, r9d           ; offset = 0
        call sys_mmap
        ; write 0x42 to first byte, read it back
        mov byte [rax], 0x42
        movzx rbx, byte [rax]
        ; munmap
        mov rdi, rax
        mov esi, 4096
        call sys_munmap
        assert_eq rbx, 0x42

        tests_done

segment readable writeable

pipefd dd 0, 0
hello  db 'hello'
randbuf rb 16
