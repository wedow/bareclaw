; arena.inc — mmap-backed bump allocator
; Requires: syscalls.inc (for sys_mmap, sys_munmap, sys_write, sys_exit_group)

ARENA_SIZE equ 64 * 1024 * 1024  ; 64 MB

; arena_init — mmap 64MB anonymous region, store base/pos/end
arena_init:
        xor edi, edi              ; addr = NULL
        mov esi, ARENA_SIZE       ; length = 64MB
        mov edx, 3                ; PROT_READ | PROT_WRITE
        mov r10d, 0x22            ; MAP_PRIVATE | MAP_ANONYMOUS
        mov r8d, -1               ; fd = -1
        xor r9d, r9d             ; offset = 0
        call sys_mmap
        cmp rax, -1
        je .init_fail
        mov [arena_base], rax
        mov [arena_pos], rax
        lea rcx, [rax + ARENA_SIZE]
        mov [arena_end], rcx
        ret
.init_fail:
        mov eax, 1
        mov edi, 2                ; stderr
        lea rsi, [.err_msg]
        mov edx, .err_msg_end - .err_msg
        syscall
        mov eax, 231
        mov edi, 1
        syscall
        jmp .err_msg_end
  .err_msg db 'arena_init: mmap failed', 10
  .err_msg_end:

; arena_alloc — allocate rdi bytes, return pointer in rax
; Aligns allocation to 8 bytes. Exits on exhaustion.
arena_alloc:
        mov rax, [arena_pos]
        ; align rax up to 8
        add rax, 7
        and rax, -8
        lea rcx, [rax + rdi]
        cmp rcx, [arena_end]
        ja .alloc_oom
        mov [arena_pos], rcx
        ret
.alloc_oom:
        mov eax, 1
        mov edi, 2
        lea rsi, [.oom_msg]
        mov edx, .oom_msg_end - .oom_msg
        syscall
        mov eax, 231
        mov edi, 1
        syscall
        jmp .oom_msg_end
  .oom_msg db 'arena_alloc: out of memory', 10
  .oom_msg_end:

; arena_reset — set position back to base (instant free-all)
arena_reset:
        mov rax, [arena_base]
        mov [arena_pos], rax
        ret

; arena_destroy — munmap the region
arena_destroy:
        mov rdi, [arena_base]
        mov esi, ARENA_SIZE
        call sys_munmap
        xor eax, eax
        mov [arena_base], rax
        mov [arena_pos], rax
        mov [arena_end], rax
        ret
