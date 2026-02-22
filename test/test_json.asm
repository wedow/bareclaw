format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/strings.inc'

segment readable executable

include '../src/json.inc'

entry $

; === Test 1: parse stop response ===
        test_begin "parse stop: finish_reason=stop, content starts with H"
        lea rdi, [resp_stop]
        mov rsi, resp_stop_len
        call json_parse_response
        test rax, rax
        jnz .fail1
        mov rax, qword [json_resp_buf + RESP_ERROR]
        test rax, rax
        jnz .fail1
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 4
        jne .fail1
        mov rax, qword [json_resp_buf + RESP_CONTENT]
        movzx ebx, byte [rax]
        cmp bl, 'H'
        jne .fail1
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        test rax, rax
        jnz .fail1
        test_pass
        jmp .test2
.fail1: test_fail
.test2:

; === Test 2: parse tool_calls response ===
        test_begin "parse tool_calls: finish_reason=tool_calls, tool_calls non-null"
        lea rdi, [resp_tool]
        mov rsi, resp_tool_len
        call json_parse_response
        test rax, rax
        jnz .fail2
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 10
        jne .fail2
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        test rax, rax
        jz .fail2
        mov rax, qword [json_resp_buf + RESP_CONTENT]
        test rax, rax
        jnz .fail2
        test_pass
        jmp .test3
.fail2: test_fail
.test3:

; === Test 3: parse tool call — name=shell ===
        test_begin "parse_tool_call: name=shell"
        lea rdi, [resp_tool]
        mov rsi, resp_tool_len
        call json_parse_response
        mov rdi, qword [json_resp_buf + RESP_TOOL_CALLS]
        inc rdi
        call json_parse_tool_call
        test rax, rax
        jnz .fail3
        mov rax, qword [json_tc_buf + TC_NAME + 8]
        cmp rax, 5
        jne .fail3
        mov rax, qword [json_tc_buf + TC_NAME]
        movzx ebx, byte [rax]
        cmp bl, 's'
        jne .fail3
        test_pass
        jmp .test4
.fail3: test_fail
.test4:

; === Test 4: parse error response ===
        test_begin "parse error: error=1"
        lea rdi, [resp_err]
        mov rsi, resp_err_len
        call json_parse_response
        test rax, rax
        jnz .fail4
        mov rax, qword [json_resp_buf + RESP_ERROR]
        cmp rax, 1
        jne .fail4
        test_pass
        jmp .test5
.fail4: test_fail
.test5:

; === Test 5: json_unescape ===
        test_begin "json_unescape: escaped quotes to plain"
        lea rdi, [esc_input]
        mov rsi, esc_input_len
        lea rdx, [unescape_buf]
        call json_unescape
        cmp rax, 16
        jne .fail5
        movzx ebx, byte [unescape_buf + 1]
        cmp bl, '"'
        jne .fail5
        test_pass
        jmp .test6
.fail5: test_fail
.test6:

; === Test 6: json_escape_string ===
        test_begin "json_escape_string: quote and backslash"
        lea rdi, [esc_src]
        lea rsi, [escape_buf]
        mov rdx, 256
        call json_escape_string
        ; input: He said "hi"\  (13 chars) -> He said \"hi\"\\  (16 chars)
        cmp rax, 16
        jne .fail6
        movzx ebx, byte [escape_buf]
        cmp bl, 'H'
        jne .fail6
        test_pass
        jmp .test7
.fail6: test_fail
.test7:

; === Test 7: json_build_message ===
        test_begin "json_build_message: user message has role:user"
        lea rdi, [msg_buf]
        lea rsi, [role_user]
        lea rdx, [content_hello]
        call json_build_message
        lea rdi, [msg_buf]
        lea rsi, [expect_role_user]
        call str_starts_with
        test rax, rax
        jz .fail7
        test_pass
        jmp .test8
.fail7: test_fail
.test8:

; === Test 8: json_build_request ===
        test_begin "json_build_request: contains model and messages"
        lea rdi, [premsg_buf]
        lea rsi, [role_user]
        lea rdx, [content_hello]
        call json_build_message
        lea rcx, [premsg_buf]
        mov qword [msg_arr], rcx
        mov qword [msg_arr + 8], rax

        lea rdi, [req_buf]
        lea rsi, [model_str]
        lea rdx, [msg_arr]
        mov rcx, 1
        xor r8d, r8d
        call json_build_request
        mov r12, rax

        lea rdi, [req_buf]
        lea rsi, [expect_model]
        call str_starts_with
        test rax, rax
        jz .fail8
        lea rdi, [req_buf]
        lea rsi, [expect_msgs]
        call str_contains
        test rax, rax
        jz .fail8
        test_pass
        jmp .test_done
.fail8: test_fail
.test_done:

        tests_done

; --- str_contains helper ---
str_contains:
        push rbx
        push r12
        push r13
        mov r12, rdi
        mov r13, rsi
.sc_scan:
        movzx eax, byte [r12]
        test al, al
        jz .sc_no
        mov rdi, r12
        mov rsi, r13
        call str_starts_with
        test rax, rax
        jnz .sc_yes
        inc r12
        jmp .sc_scan
.sc_yes:
        mov eax, 1
        pop r13
        pop r12
        pop rbx
        ret
.sc_no:
        xor eax, eax
        pop r13
        pop r12
        pop rbx
        ret

segment readable writeable

resp_stop db '{"choices":[{"message":{"role":"assistant","content":"Hello world"},"finish_reason":"stop"}]}',0
resp_stop_len = $ - resp_stop - 1

resp_tool db '{"choices":[{"message":{"role":"assistant","content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"shell","arguments":"{\"command\":\"ls -la\"}"}}]},"finish_reason":"tool_calls"}]}',0
resp_tool_len = $ - resp_tool - 1

resp_err db '{"error":{"message":"rate limited"}}',0
resp_err_len = $ - resp_err - 1

esc_input db '{\"command\":\"ls\"}',0
esc_input_len = 20

esc_src db 'He said "hi"\', 0

role_user      db 'user', 0
content_hello  db 'hello', 0
model_str      db 'gpt-4', 0

expect_role_user db '{"role":"user"', 0
expect_model     db '{"model":"', 0
expect_msgs      db '"messages":[', 0

; buffers
escape_buf  rb 256
unescape_buf rb 256
msg_buf     rb 4096
premsg_buf  rb 1024
req_buf     rb 8192
msg_arr     rb 128

json_resp_buf rb RESP_SIZE
json_tc_buf   rb TC_SIZE
