format ELF64 executable 3

include '../src/strings.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/test_macros.inc'
include '../src/arena.inc'
include '../src/run_capture.inc'

entry _start
_start:
        call arena_init

        ; --- test 1: shell capture ---
        test_begin 'run_capture: echo hello'
        lea rdi, [.bin_sh]
        lea rsi, [.argv_echo]
        call run_capture
        ; verify output starts with "hello"
        mov rdi, rax
        lea rsi, [.expect_hello]
        call str_starts_with
        assert_eq rax, 1

        ; --- test 2: empty output ---
        test_begin 'run_capture: empty output'
        lea rdi, [.bin_sh]
        lea rsi, [.argv_empty]
        call run_capture
        ; verify first byte is null (empty string)
        movzx eax, byte [rax]
        assert_eq rax, 0

        ; --- test 3: stderr capture ---
        test_begin 'run_capture: stderr capture'
        lea rdi, [.bin_sh]
        lea rsi, [.argv_stderr]
        call run_capture
        mov rdi, rax
        lea rsi, [.expect_error]
        call str_starts_with
        assert_eq rax, 1

        call arena_destroy
        tests_done

; --- data ---
.bin_sh db '/bin/sh', 0
.arg_c  db '-c', 0
.cmd_echo   db 'echo hello 2>&1', 0
.cmd_empty  db 'cat /dev/null 2>&1', 0
.cmd_stderr db 'echo error >&2', 0

.expect_hello db 'hello', 0
.expect_error db 'error', 0

; argv arrays (pointers)
align 8
.argv_echo  dq .bin_sh, .arg_c, .cmd_echo, 0
.argv_empty dq .bin_sh, .arg_c, .cmd_empty, 0
.argv_stderr dq .bin_sh, .arg_c, .cmd_stderr, 0

segment readable writeable

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

pipe_fds    dd 0, 0
wait_status dd 0
envp_ptr    dq 0
