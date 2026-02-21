format ELF64 executable 3

include '../src/test_macros.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/arena.inc'

entry $

; --- Test 1: arena_init, verify base is non-zero ---
        test_begin "arena_init: base is non-zero"
        call arena_init
        mov rax, [arena_base]
        cmp rax, 0
        je .fail_1
        test_pass
        jmp .test_2
.fail_1:
        test_fail
.test_2:

; --- Test 2: alloc 100 bytes, write pattern, read back ---
        test_begin "arena_alloc: 100 bytes write/read"
        mov rdi, 100
        call arena_alloc
        mov rbx, rax              ; save pointer
        ; write 0xAB to first byte, 0xCD to last byte
        mov byte [rbx], 0xAB
        mov byte [rbx + 99], 0xCD
        movzx rax, byte [rbx]
        cmp al, 0xAB
        jne .fail_2
        movzx rax, byte [rbx + 99]
        cmp al, 0xCD
        jne .fail_2
        test_pass
        jmp .test_3
.fail_2:
        test_fail
.test_3:

; --- Test 3: alloc 200 more bytes, verify pointer advanced >= 100 ---
        test_begin "arena_alloc: pointer advances correctly"
        mov rdi, 200
        call arena_alloc
        ; rax = new pointer, rbx = old pointer from test 2
        sub rax, rbx
        cmp rax, 100
        jl .fail_3
        test_pass
        jmp .test_4
.fail_3:
        test_fail
.test_4:

; --- Test 4: reset, alloc, verify pointer near base ---
        test_begin "arena_reset: pointer returns to base"
        call arena_reset
        mov rdi, 8
        call arena_alloc
        mov rbx, [arena_base]
        cmp rax, rbx
        jne .fail_4
        test_pass
        jmp .test_5
.fail_4:
        test_fail
.test_5:

; --- Test 5: destroy arena ---
        test_begin "arena_destroy: cleans up"
        call arena_destroy
        mov rax, [arena_base]
        assert_eq rax, 0

        tests_done

segment readable writeable

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0
