format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/strings.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/arena.inc'
include '../src/json_build.inc'
include '../src/json_scan.inc'
include '../src/config.inc'
include '../src/log.inc'
include '../src/http.inc'
include '../src/shell.inc'
include '../src/agent.inc'

entry $
        ; init arena
        call arena_init

; === Test 1: msg_list_init sets count=0, cap=256 ===
        test_begin "msg_list_init: count=0, cap=256"
        call msg_list_init
        mov rax, [msg_list_count]
        test rax, rax
        jnz .fail1
        mov rax, [msg_list_cap]
        cmp rax, 256
        jne .fail1
        mov rax, [msg_list_ptr]
        test rax, rax
        jz .fail1
        test_pass
        jmp .test2
.fail1: test_fail
.test2:

; === Test 2: msg_list_append increments count ===
        test_begin "msg_list_append: count goes to 1"
        lea rdi, [dummy_json]
        mov rsi, dummy_json_len
        call msg_list_append
        mov rax, [msg_list_count]
        cmp rax, 1
        jne .fail2
        ; verify stored pointer
        mov rcx, [msg_list_ptr]
        mov rax, [rcx + MSG_JSON_PTR]
        lea rbx, [dummy_json]
        cmp rax, rbx
        jne .fail2
        ; verify stored length
        mov rax, [rcx + MSG_JSON_LEN]
        cmp rax, dummy_json_len
        jne .fail2
        test_pass
        jmp .test3
.fail2: test_fail
.test3:

; === Test 3: msg_list_append_role builds valid JSON ===
        test_begin "msg_list_append_role: builds user message JSON"
        ; reset for clean state
        call arena_reset
        call msg_list_init
        lea rdi, [role_user]
        lea rsi, [content_hello]
        call msg_list_append_role
        ; count should be 1
        mov rax, [msg_list_count]
        cmp rax, 1
        jne .fail3
        ; check that stored JSON starts with {"role":"
        mov rcx, [msg_list_ptr]
        mov rax, [rcx + MSG_JSON_PTR]
        cmp byte [rax], '{'
        jne .fail3
        cmp byte [rax + 1], '"'
        jne .fail3
        cmp byte [rax + 2], 'r'
        jne .fail3
        test_pass
        jmp .test4
.fail3: test_fail
.test4:

; === Test 4: multiple appends increase count ===
        test_begin "msg_list_append: multiple appends"
        call arena_reset
        call msg_list_init
        ; append 5 messages
        mov r12d, 0
.append_loop:
        cmp r12d, 5
        jge .append_done
        lea rdi, [dummy_json]
        mov rsi, dummy_json_len
        call msg_list_append
        inc r12d
        jmp .append_loop
.append_done:
        mov rax, [msg_list_count]
        cmp rax, 5
        jne .fail4
        test_pass
        jmp .test5
.fail4: test_fail
.test5:

; === Test 5: agent_compact — no compaction when under limit ===
        test_begin "agent_compact: no-op when under max_messages"
        call arena_reset
        call msg_list_init
        ; add 5 messages
        mov r12d, 0
.add5:
        cmp r12d, 5
        jge .add5_done
        lea rdi, [dummy_json]
        mov rsi, dummy_json_len
        call msg_list_append
        inc r12d
        jmp .add5
.add5_done:
        ; config with max_messages=40
        lea rdi, [test_config]
        xor esi, esi                ; log_fd=0 (no logging)
        call agent_compact
        ; count should still be 5
        mov rax, [msg_list_count]
        cmp rax, 5
        jne .fail5
        test_pass
        jmp .test6
.fail5: test_fail
.test6:

; === Test 6: agent_compact — compacts when over limit ===
        test_begin "agent_compact: compacts 50 messages to 11"
        call arena_reset
        call msg_list_init
        ; add 50 messages
        mov r12d, 0
.add50:
        cmp r12d, 50
        jge .add50_done
        lea rdi, [dummy_json]
        mov rsi, dummy_json_len
        call msg_list_append
        inc r12d
        jmp .add50
.add50_done:
        ; verify we have 50
        mov rax, [msg_list_count]
        cmp rax, 50
        jne .fail6
        ; compact with max_messages=40
        lea rdi, [test_config]
        xor esi, esi
        call agent_compact
        ; should be 11: system msg (0) + last 10
        mov rax, [msg_list_count]
        cmp rax, 11
        jne .fail6
        ; system message (index 0) should still point to dummy_json
        mov rcx, [msg_list_ptr]
        mov rax, [rcx + MSG_JSON_PTR]
        lea rbx, [dummy_json]
        cmp rax, rbx
        jne .fail6
        test_pass
        jmp .test7
.fail6: test_fail
.test7:

; === Test 7: msg_list_append_tool_result builds tool message ===
        test_begin "msg_list_append_tool_result: builds tool message"
        call arena_reset
        call msg_list_init
        lea rdi, [tc_id]
        lea rsi, [tc_output]
        call msg_list_append_tool_result
        ; count should be 1
        mov rax, [msg_list_count]
        cmp rax, 1
        jne .fail7
        ; check JSON starts with {"role":"tool"
        mov rcx, [msg_list_ptr]
        mov rax, [rcx + MSG_JSON_PTR]
        cmp byte [rax], '{'
        jne .fail7
        ; find "tool" in the first 20 bytes
        cmp byte [rax + 9], 't'
        jne .fail7
        cmp byte [rax + 10], 'o'
        jne .fail7
        cmp byte [rax + 11], 'o'
        jne .fail7
        cmp byte [rax + 12], 'l'
        jne .fail7
        test_pass
        jmp .test8
.fail7: test_fail
.test8:

; === Test 8: sys_nanosleep returns 0 for short sleep ===
        test_begin "sys_nanosleep: 1ms sleep returns 0"
        mov qword [timespec_buf], 0         ; tv_sec = 0
        mov qword [timespec_buf + 8], 1000000 ; tv_nsec = 1ms
        lea rdi, [timespec_buf]
        xor esi, esi                        ; remaining = NULL
        call sys_nanosleep
        test rax, rax
        jnz .fail8
        test_pass
        jmp .test_done
.fail8: test_fail
.test_done:

        call arena_destroy
        tests_done

segment readable writeable

; arena
arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

; message list
msg_list_ptr   dq 0
msg_list_count dq 0
msg_list_cap   dq 0

; shared state for other modules
pipe_fds    dd 0, 0
wait_status dd 0
envp_ptr    dq 0
timespec_buf dq 0, 0

; json scanner buffers
json_resp_buf rb RESP_SIZE
json_tc_buf   rb TC_SIZE

; retry state
retry_count     dq 0
retry_backoff   dq 0
retry_digit_buf db 0, 0

; test data
dummy_json db '{"role":"user","content":"test"}', 0
dummy_json_len = $ - dummy_json - 1

role_user      db 'user', 0
content_hello  db 'hello', 0
tc_id          db 'call_123', 0
tc_output      db 'file1.txt', 0

; test config — only max_messages field matters for compact test
test_config:
        rb CONFIG_MAX_MSGS         ; pad up to max_msgs offset
        dq 40                      ; max_messages = 40
        rb CONFIG_SIZE - CONFIG_MAX_MSGS - 8
