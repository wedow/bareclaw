format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/strings.inc'

segment readable executable

include '../src/json_build.inc'

entry $

; === Test 1: json_escape_string basic ===
        test_begin "json_escape_string: quotes and backslash"
        lea rdi, [esc_src]
        lea rsi, [esc_buf]
        mov rdx, 256
        call json_escape_string
        ; input: He said "hi"\  (13 chars)
        ; output: He said \"hi\"\\  (16 chars)
        cmp rax, 16
        jne .fail1
        ; verify dest starts with 'H'
        movzx ebx, byte [esc_buf]
        cmp bl, 'H'
        jne .fail1
        ; verify null terminator
        movzx ebx, byte [esc_buf + 16]
        test bl, bl
        jnz .fail1
        test_pass
        jmp .test2
.fail1: test_fail
.test2:

; === Test 2: json_escape_string newline and tab ===
        test_begin "json_escape_string: newline and tab"
        lea rdi, [esc_nl_src]
        lea rsi, [esc_buf]
        mov rdx, 256
        call json_escape_string
        ; input: "a\nb\t" (4 chars: a, newline, b, tab)
        ; output: "a\nb\t" (6 chars: a, \, n, b, \, t)
        cmp rax, 6
        jne .fail2
        ; check byte 1 = '\'
        movzx ebx, byte [esc_buf + 1]
        cmp bl, '\'
        jne .fail2
        ; check byte 2 = 'n'
        movzx ebx, byte [esc_buf + 2]
        cmp bl, 'n'
        jne .fail2
        test_pass
        jmp .test3
.fail2: test_fail
.test3:

; === Test 3: json_build_message with content ===
        test_begin "json_build_message: user message"
        lea rdi, [msg_buf]
        lea rsi, [role_user]
        lea rdx, [content_hello]
        call json_build_message
        ; should start with {"role":"user"
        mov r12, rax                ; save length
        lea rdi, [msg_buf]
        lea rsi, [expect_role_user]
        call str_starts_with
        test rax, rax
        jz .fail3
        ; should contain "content":"hello"
        lea rdi, [msg_buf]
        lea rsi, [expect_content_hello]
        call str_contains
        test rax, rax
        jz .fail3
        ; should end with }
        movzx ebx, byte [msg_buf + r12 - 1]
        cmp bl, '}'
        jne .fail3
        test_pass
        jmp .test4
.fail3: test_fail
.test4:

; === Test 4: json_build_message with NULL content ===
        test_begin "json_build_message: null content"
        lea rdi, [msg_buf]
        lea rsi, [role_assistant]
        xor rdx, rdx                ; NULL
        call json_build_message
        lea rdi, [msg_buf]
        lea rsi, [expect_null]
        call str_contains
        test rax, rax
        jz .fail4
        test_pass
        jmp .test5
.fail4: test_fail
.test5:

; === Test 5: json_build_tool_message ===
        test_begin "json_build_tool_message: correct format"
        lea rdi, [msg_buf]
        lea rsi, [tc_id]
        lea rdx, [tc_content]
        call json_build_tool_message
        ; starts with {"role":"tool"
        lea rdi, [msg_buf]
        lea rsi, [expect_role_tool]
        call str_starts_with
        test rax, rax
        jz .fail5
        ; contains "tool_call_id":"call_1"
        lea rdi, [msg_buf]
        lea rsi, [expect_tcid]
        call str_contains
        test rax, rax
        jz .fail5
        test_pass
        jmp .test6
.fail5: test_fail
.test6:

; === Test 6: json_build_request ===
        test_begin "json_build_request: 2 messages with tools"
        ; build msg1 and msg2 first
        lea rdi, [premsg1_buf]
        lea rsi, [role_user]
        lea rdx, [content_hello]
        call json_build_message
        lea rcx, [premsg1_buf]
        mov qword [msg_arr], rcx
        mov qword [msg_arr + 8], rax

        lea rdi, [premsg2_buf]
        lea rsi, [role_assistant]
        lea rdx, [content_hi]
        call json_build_message
        lea rcx, [premsg2_buf]
        mov qword [msg_arr + 16], rcx
        mov qword [msg_arr + 24], rax

        lea rdi, [req_buf]
        lea rsi, [model_str]
        lea rdx, [msg_arr]
        mov rcx, 2
        mov r8, 1
        call json_build_request
        mov r12, rax

        ; starts with {"model":"
        lea rdi, [req_buf]
        lea rsi, [expect_model]
        call str_starts_with
        test rax, rax
        jz .fail6
        ; contains "messages":[
        lea rdi, [req_buf]
        lea rsi, [expect_msgs]
        call str_contains
        test rax, rax
        jz .fail6
        ; contains "tools":[
        lea rdi, [req_buf]
        lea rsi, [expect_tools]
        call str_contains
        test rax, rax
        jz .fail6
        ; ends with }
        movzx ebx, byte [req_buf + r12 - 1]
        cmp bl, '}'
        jne .fail6
        test_pass
        jmp .test7
.fail6: test_fail
.test7:

; === Test 7: json_build_request without tools ===
        test_begin "json_build_request: no tools"
        lea rdi, [req_buf]
        lea rsi, [model_str]
        lea rdx, [msg_arr]
        mov rcx, 1
        xor r8d, r8d               ; no tools
        call json_build_request
        mov r12, rax
        ; should NOT contain "tools"
        lea rdi, [req_buf]
        lea rsi, [expect_tools]
        call str_contains
        test rax, rax
        jnz .fail7
        test_pass
        jmp .test_done
.fail7: test_fail
.test_done:

        tests_done

; --- str_contains helper: rdi=haystack, rsi=needle, returns rax=1 if found, 0 if not ---
str_contains:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; haystack
        mov r13, rsi                ; needle
        ; get needle length
        mov rdi, r13
        call str_len
        mov rbx, rax                ; rbx = needle len
        test rbx, rbx
        jz .sc_yes                  ; empty needle always found
.sc_scan:
        movzx eax, byte [r12]
        test al, al
        jz .sc_no                   ; end of haystack
        ; compare needle at this position
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

; escape test data
esc_src     db 'He said "hi"\', 0
esc_nl_src  db 'a', 10, 'b', 9, 0

; roles and content
role_user      db 'user', 0
role_assistant db 'assistant', 0
content_hello  db 'hello', 0
content_hi     db 'hi', 0
tc_id          db 'call_1', 0
tc_content     db 'done', 0
model_str      db 'gpt-4', 0

; expected substrings
expect_role_user    db '{"role":"user"', 0
expect_content_hello db '"content":"hello"', 0
expect_null         db '"content":null', 0
expect_role_tool    db '{"role":"tool"', 0
expect_tcid         db '"tool_call_id":"call_1"', 0
expect_model        db '{"model":"', 0
expect_msgs         db '"messages":[', 0
expect_tools        db '"tools":[', 0

; buffers
esc_buf     rb 256
msg_buf     rb 4096
premsg1_buf rb 1024
premsg2_buf rb 1024
req_buf     rb 8192
msg_arr     rb 128
