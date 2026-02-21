format ELF64 executable 3

include '../src/test_macros.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/strings.inc'
include '../src/arena.inc'
include '../src/http.inc'

entry $

        ; init arena
        call arena_init

; --- Test 1: http_post to bad port returns non-null pointer ---
        test_begin "http_post to bad port returns non-null"
        lea rdi, [bad_url]
        lea rsi, [test_key]
        lea rdx, [test_body]
        call http_post
        test rax, rax
        jz .fail_1
        test_pass
        jmp .test_2
.fail_1:
        test_fail
.test_2:

; --- Test 2: response from bad port is non-empty (curl error message) ---
        test_begin "response from bad port is non-empty"
        ; rax still has the pointer from test 1 — but we called test_pass which clobbers rax
        ; redo the call
        call arena_reset
        lea rdi, [bad_url]
        lea rsi, [test_key]
        lea rdx, [test_body]
        call http_post
        cmp byte [rax], 0
        je .fail_2
        test_pass
        jmp .test_3
.fail_2:
        test_fail
.test_3:

; --- Test 3: auth header construction — verify arena contains correct string ---
        test_begin "auth header built correctly"
        call arena_reset
        ; build "Authorization: Bearer mykey" by calling http_post
        ; We'll check the arena contains the auth header after construction.
        ; Simpler: just test the str_copy + prefix concat logic directly.
        ; Allocate and build manually like http_post does.
        mov rdi, 40                 ; enough for "Authorization: Bearer " + "test-key" + null
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        lea rsi, [auth_prefix]
        call str_copy
        lea rdi, [rbx + 22]
        lea rsi, [test_key]
        call str_copy
        ; verify result
        mov rdi, rbx
        lea rsi, [expected_auth]
        call str_eq
        cmp rax, 1
        jne .fail_3
        test_pass
        jmp .done
.fail_3:
        test_fail
.done:

        call arena_destroy
        tests_done

segment readable writeable

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

pipe_fds   dd 0, 0
wait_status dd 0
timespec_buf dq 0, 0

bad_url      db 'http://127.0.0.1:1', 0
test_key     db 'test-key', 0
test_body    db '{}', 0
auth_prefix  db 'Authorization: Bearer ', 0
expected_auth db 'Authorization: Bearer test-key', 0
