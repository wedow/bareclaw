format ELF64 executable 3

include '../src/strings.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/test_macros.inc'
include '../src/arena.inc'
include '../src/config.inc'
include '../src/session.inc'

entry _start
_start:
        call arena_init

        ; set config_sessions_dir for tests
        lea rax, [test_sessions_dir]
        mov [config_sessions_dir], rax

        ; set up envp (needed for env_get in config)
        lea rdi, [fake_envp]
        call env_init

        ; === Test 1: create session, append 3 messages, close ===
        test_begin 'session: create and append 3 messages'
        lea rdi, [test_sid]
        call session_create
        cmp rax, 0
        jl .fail1
        mov [test_fd], eax

        mov edi, [test_fd]
        lea rsi, [msg1]
        mov rdx, msg1_len
        call session_append

        mov edi, [test_fd]
        lea rsi, [msg2]
        mov rdx, msg2_len
        call session_append

        mov edi, [test_fd]
        lea rsi, [msg3]
        mov rdx, msg3_len
        call session_append

        ; close
        mov edi, [test_fd]
        call sys_close
        test_pass
        jmp .test2
.fail1: test_fail

.test2:
        ; === Test 2: open session, load messages, verify count=3 ===
        test_begin 'session: load messages count=3'
        lea rdi, [test_sid]
        call session_open
        cmp rax, 0
        jl .fail2
        mov [test_fd], eax

        mov edi, [test_fd]
        call session_load_messages

        mov rax, [msg_list_count]
        cmp rax, 3
        jne .fail2
        test_pass
        jmp .test3
.fail2: test_fail

.test3:
        ; === Test 3: verify first message content ===
        test_begin 'session: first message matches'
        mov rax, [msg_list_ptr]
        mov rdi, [rax + MSG_JSON_PTR]
        lea rsi, [msg1]
        ; compare first few chars
        call str_starts_with
        assert_eq rax, 1

        ; close fd
        mov edi, [test_fd]
        call sys_close

.test4:
        ; === Test 4: compact test ===
        ; create new session with 12 messages
        test_begin 'session: compact to 5'
        lea rdi, [test_sid2]
        call session_create
        cmp rax, 0
        jl .fail4
        mov [test_fd], eax

        ; append 12 messages
        mov r12d, 0
.append_loop:
        cmp r12d, 12
        jge .append_done
        mov edi, [test_fd]
        lea rsi, [msg1]
        mov rdx, msg1_len
        test r12d, r12d
        jnz .not_first
        ; first message = system message
        lea rsi, [msg_sys]
        mov rdx, msg_sys_len
.not_first:
        call session_append
        inc r12d
        jmp .append_loop
.append_done:
        ; close and reopen with O_RDWR|O_APPEND
        mov edi, [test_fd]
        call sys_close
        lea rdi, [test_sid2]
        call session_open
        mov [test_fd], eax

        ; load
        mov edi, [test_fd]
        call session_load_messages
        mov rax, [msg_list_count]
        cmp rax, 12
        jne .fail4

        ; compact to 5
        mov edi, [test_fd]
        mov rsi, 5
        call session_compact

        mov rax, [msg_list_count]
        cmp rax, 5
        jne .fail4
        test_pass
        jmp .test5
.fail4: test_fail

.test5:
        ; verify first line is still system message after compact
        test_begin 'session: compact preserves system msg'
        mov rax, [msg_list_ptr]
        mov rdi, [rax + MSG_JSON_PTR]
        lea rsi, [msg_sys]
        call str_starts_with
        assert_eq rax, 1

        ; cleanup test_fd from compact test
        mov edi, [test_fd]
        call sys_close

.test6:
        ; === Test 6: session_open_log creates .log file ===
        test_begin 'session: open_log returns valid fd'
        lea rdi, [test_sid]
        call session_open_log
        cmp rax, 0
        jl .fail6
        mov [test_log_fd], eax

        ; write something to verify it's writable
        mov edi, eax
        lea rsi, [.log_data]
        mov edx, .log_data_len
        call sys_write
        cmp rax, .log_data_len
        jne .fail6

        mov edi, [test_log_fd]
        call sys_close
        test_pass
        jmp .test_done
.fail6: test_fail

.log_data db 'test log entry', 10
.log_data_len = $ - .log_data

.test_done:
        call arena_destroy

        ; remove test files
        lea rdi, [.rm_path]
        lea rsi, [.rm_argv]
        xor edx, edx
        call sys_execve
        ; if execve fails, just exit
        tests_done

.rm_path db '/bin/rm', 0
.rm_arg1 db '-rf', 0

align 8
.rm_argv dq .rm_path, .rm_arg1, test_sessions_dir, 0

segment readable writeable

test_sessions_dir db '/tmp/szc_test_sessions', 0
test_sid          db 'test_session_01', 0
test_sid2         db 'test_session_02', 0

msg_sys db '{"role":"system","content":"you are helpful"}', 0
msg_sys_len = $ - msg_sys - 1

msg1 db '{"role":"user","content":"hello"}', 0
msg1_len = $ - msg1 - 1

msg2 db '{"role":"assistant","content":"hi there"}', 0
msg2_len = $ - msg2 - 1

msg3 db '{"role":"user","content":"bye"}', 0
msg3_len = $ - msg3 - 1

align 8
fake_envp:
        dq .env1, 0
.env1 db 'HOME=/tmp', 0

test_fd     dd 0
test_log_fd dd 0

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

envp_ptr   dq 0

config_api_key      dq 0
config_model        dq 0
config_endpoint     dq 0
config_skills_dir   dq 0
config_sessions_dir dq 0

msg_list_ptr   dq 0
msg_list_count dq 0
