format ELF64 executable 3

include '../src/test_macros.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/arena.inc'
include '../src/shell.inc'

entry $

        ; init arena
        call arena_init

; --- Test 1: echo hello — output starts with "hello" ---
        test_begin "shell_exec('echo hello') starts with 'hello'"
        lea rdi, [cmd_echo_hello]
        call shell_exec
        mov rbx, rax
        cmp byte [rbx], 'h'
        jne .fail_1
        cmp byte [rbx+1], 'e'
        jne .fail_1
        cmp byte [rbx+2], 'l'
        jne .fail_1
        cmp byte [rbx+3], 'l'
        jne .fail_1
        cmp byte [rbx+4], 'o'
        jne .fail_1
        test_pass
        jmp .test_2
.fail_1:
        test_fail
.test_2:

; --- Test 2: echo -n abc — output is exactly "abc\0" ---
        test_begin "shell_exec('echo -n abc') == 'abc'"
        call arena_reset
        lea rdi, [cmd_echo_abc]
        call shell_exec
        mov rbx, rax
        cmp byte [rbx], 'a'
        jne .fail_2
        cmp byte [rbx+1], 'b'
        jne .fail_2
        cmp byte [rbx+2], 'c'
        jne .fail_2
        cmp byte [rbx+3], 0
        jne .fail_2
        test_pass
        jmp .test_3
.fail_2:
        test_fail
.test_3:

; --- Test 3: cat /dev/null — output is empty ---
        test_begin "shell_exec('cat /dev/null') is empty"
        call arena_reset
        lea rdi, [cmd_cat_null]
        call shell_exec
        mov rbx, rax
        cmp byte [rbx], 0
        jne .fail_3
        test_pass
        jmp .test_4
.fail_3:
        test_fail
.test_4:

; --- Test 4: echo error >&2 — stderr captured ---
        test_begin "shell_exec('echo error >&2') captures stderr"
        call arena_reset
        lea rdi, [cmd_echo_stderr]
        call shell_exec
        mov rbx, rax
        cmp byte [rbx], 'e'
        jne .fail_4
        cmp byte [rbx+1], 'r'
        jne .fail_4
        cmp byte [rbx+2], 'r'
        jne .fail_4
        cmp byte [rbx+3], 'o'
        jne .fail_4
        cmp byte [rbx+4], 'r'
        jne .fail_4
        test_pass
        jmp .done
.fail_4:
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

cmd_echo_hello  db 'echo hello', 0
cmd_echo_abc    db 'echo -n abc', 0
cmd_cat_null    db 'cat /dev/null', 0
cmd_echo_stderr db 'echo error >&2', 0
