format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/strings.inc'

segment readable executable

include '../src/json_scan.inc'

entry $

; === Test 1: parse resp_stop ===
        test_begin "parse resp_stop: finish_reason=stop, content=Hello, error=0"
        lea rdi, [resp_stop]
        mov rsi, resp_stop_len
        call json_parse_response
        test rax, rax
        jnz .fail1
        ; error should be 0
        mov rax, qword [json_resp_buf + RESP_ERROR]
        test rax, rax
        jnz .fail1
        ; finish_reason length = 4
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 4
        jne .fail1
        ; first char = 's'
        mov rax, qword [json_resp_buf + RESP_FINISH_REASON]
        movzx ebx, byte [rax]
        cmp bl, 's'
        jne .fail1
        ; content length = 11
        mov rax, qword [json_resp_buf + RESP_CONTENT_LEN]
        cmp rax, 11
        jne .fail1
        ; content starts with 'H'
        mov rax, qword [json_resp_buf + RESP_CONTENT]
        movzx ebx, byte [rax]
        cmp bl, 'H'
        jne .fail1
        ; tool_calls = 0
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        test rax, rax
        jnz .fail1
        test_pass
        jmp .test2
.fail1: test_fail
.test2:

; === Test 2: parse resp_tool ===
        test_begin "parse resp_tool: finish_reason=tool_calls, tool_calls non-null"
        lea rdi, [resp_tool]
        mov rsi, resp_tool_len
        call json_parse_response
        test rax, rax
        jnz .fail2
        ; finish_reason length = 10
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 10
        jne .fail2
        ; tool_calls non-null
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        test rax, rax
        je .fail2
        ; content pointer = 0 (null)
        mov rax, qword [json_resp_buf + RESP_CONTENT]
        test rax, rax
        jnz .fail2
        test_pass
        jmp .test3
.fail2: test_fail
.test3:

; === Test 3: parse tool call ===
        test_begin "parse_tool_call: name=shell, args has command"
        lea rdi, [resp_tool]
        mov rsi, resp_tool_len
        call json_parse_response
        mov rdi, qword [json_resp_buf + RESP_TOOL_CALLS]
        inc rdi                     ; skip '['
        call json_parse_tool_call
        test rax, rax
        jnz .fail3
        ; name length = 5
        mov rax, qword [json_tc_buf + TC_NAME + 8]
        cmp rax, 5
        jne .fail3
        ; name first char = 's'
        mov rax, qword [json_tc_buf + TC_NAME]
        movzx ebx, byte [rax]
        cmp bl, 's'
        jne .fail3
        ; args length > 0
        mov rax, qword [json_tc_buf + TC_ARGS + 8]
        test rax, rax
        jz .fail3
        ; id starts with 'c'
        mov rax, qword [json_tc_buf + TC_ID]
        movzx ebx, byte [rax]
        cmp bl, 'c'
        jne .fail3
        test_pass
        jmp .test4
.fail3: test_fail
.test4:

; === Test 4: parse error response ===
        test_begin "parse resp_err: error=1, message=rate limited"
        lea rdi, [resp_err]
        mov rsi, resp_err_len
        call json_parse_response
        test rax, rax
        jnz .fail4
        ; error = 1
        mov rax, qword [json_resp_buf + RESP_ERROR]
        cmp rax, 1
        jne .fail4
        ; error message length = 12
        mov rax, qword [json_resp_buf + RESP_ERROR_MSG_LEN]
        cmp rax, 12
        jne .fail4
        ; first char = 'r'
        mov rax, qword [json_resp_buf + RESP_ERROR_MSG]
        movzx ebx, byte [rax]
        cmp bl, 'r'
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
        ; expected: {"command":"ls"} = 16 bytes
        cmp rax, 16
        jne .fail5
        ; first char = '{'
        movzx ebx, byte [unescape_buf]
        cmp bl, '{'
        jne .fail5
        ; char at 1 = '"'
        movzx ebx, byte [unescape_buf + 1]
        cmp bl, '"'
        jne .fail5
        ; null terminator
        movzx ebx, byte [unescape_buf + 16]
        test bl, bl
        jnz .fail5
        test_pass
        jmp .test6
.fail5: test_fail
.test6:

; === Test 6: json_find_key ===
        test_begin "json_find_key: find content in resp_stop"
        lea rdi, [resp_stop]
        lea rsi, [key_content]
        lea rdx, [resp_stop + resp_stop_len]
        call json_find_key
        test rax, rax
        jz .fail6
        mov rdi, rax
        call json_skip_whitespace
        movzx ebx, byte [rax]
        cmp bl, '"'
        jne .fail6
        movzx ebx, byte [rax + 1]
        cmp bl, 'H'
        jne .fail6
        test_pass
        jmp .test_done
.fail6: test_fail
.test_done:

        tests_done

segment readable writeable

resp_stop db '{"choices":[{"message":{"role":"assistant","content":"Hello world"},"finish_reason":"stop"}]}',0
resp_stop_len = $ - resp_stop - 1

resp_tool db '{"choices":[{"message":{"role":"assistant","content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"shell","arguments":"{\"command\":\"ls -la\"}"}}]},"finish_reason":"tool_calls"}]}',0
resp_tool_len = $ - resp_tool - 1

resp_err db '{"error":{"message":"rate limited"}}',0
resp_err_len = $ - resp_err - 1

esc_input db '{\"command\":\"ls\"}',0
esc_input_len = 20

key_content db 'content',0

json_resp_buf rb RESP_SIZE
json_tc_buf   rb TC_SIZE
unescape_buf  rb 256
