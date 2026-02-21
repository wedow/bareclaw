; json_scan.inc — cursor-based JSON scanner for OpenAI chat completion responses
; Requires: strings.inc

; Response struct offsets
RESP_FINISH_REASON = 0    ; pointer to string (8 bytes)
RESP_FINISH_LEN    = 8    ; length (8 bytes)
RESP_CONTENT       = 16   ; pointer to content string (8 bytes)
RESP_CONTENT_LEN   = 24   ; length (8 bytes)
RESP_TOOL_CALLS    = 32   ; pointer to tool_calls array start (8 bytes), or 0
RESP_TOOL_CALLS_END= 40   ; pointer to tool_calls array end (8 bytes)
RESP_ERROR         = 48   ; 1 if error response, 0 if normal (8 bytes)
RESP_ERROR_MSG     = 56   ; pointer to error message (8 bytes)
RESP_ERROR_MSG_LEN = 64   ; length (8 bytes)
RESP_SIZE          = 72

; Tool call struct offsets
TC_ID     = 0     ; ptr (8), len (8)
TC_NAME   = 16    ; ptr (8), len (8)
TC_ARGS   = 32    ; ptr (8), len (8)
TC_NEXT   = 48    ; cursor after this tool_call object (8)
TC_SIZE   = 56

; json_skip_whitespace — rdi=cursor, returns rax=new cursor past whitespace
json_skip_whitespace:
        mov rax, rdi
.loop:
        movzx ecx, byte [rax]
        cmp cl, ' '
        je .skip
        cmp cl, 9
        je .skip
        cmp cl, 10
        je .skip
        cmp cl, 13
        je .skip
        ret
.skip:
        inc rax
        jmp .loop

; json_scan_string — rdi=cursor pointing at opening "
; Returns rax=pointer to first char after opening quote
;         rdx=length of string content (up to closing quote)
; Handles \" escapes. Next cursor = rax + rdx + 1 (past closing quote).
json_scan_string:
        lea rax, [rdi + 1]         ; rax = start of content (past opening ")
        mov rcx, rax               ; rcx = scan cursor
.scan:
        movzx edx, byte [rcx]
        test dl, dl
        jz .eof
        cmp dl, '\'
        je .escape
        cmp dl, '"'
        je .close
        inc rcx
        jmp .scan
.escape:
        add rcx, 2                 ; skip \ and next char
        jmp .scan
.close:
        mov rdx, rcx
        sub rdx, rax               ; rdx = length
        ret
.eof:
        mov rdx, rcx
        sub rdx, rax
        ret

; json_find_key — rdi=buffer start, rsi=key string (null-terminated), rdx=buffer end
; Scans for "key": pattern. Returns rax=cursor right after colon, or 0.
json_find_key:
        push rbx
        push r12
        push r13
        push r14
        mov r12, rdi                ; r12 = scan cursor
        mov r13, rsi                ; r13 = key string
        mov r14, rdx                ; r14 = buffer end
        mov rdi, r13
        call str_len
        mov rbx, rax                ; rbx = key length
.scan:
        cmp r12, r14
        jge .not_found
        cmp byte [r12], '"'
        je .check_key
        inc r12
        jmp .scan
.check_key:
        lea rcx, [r12 + 1]         ; past opening "
        xor r8, r8
.cmp_loop:
        cmp r8, rbx
        je .cmp_done
        lea rax, [rcx + r8]
        cmp rax, r14
        jge .not_found
        movzx eax, byte [rcx + r8]
        movzx edx, byte [r13 + r8]
        cmp al, dl
        jne .no_match
        inc r8
        jmp .cmp_loop
.cmp_done:
        lea rax, [rcx + rbx]
        cmp rax, r14
        jge .not_found
        cmp byte [rax], '"'
        jne .no_match
        inc rax                     ; past closing "
.find_colon:
        cmp rax, r14
        jge .not_found
        cmp byte [rax], ':'
        je .found_colon
        cmp byte [rax], ' '
        je .skip_ws
        cmp byte [rax], 9
        je .skip_ws
        cmp byte [rax], 10
        je .skip_ws
        cmp byte [rax], 13
        je .skip_ws
        jmp .no_match
.skip_ws:
        inc rax
        jmp .find_colon
.found_colon:
        inc rax                     ; past ':'
        pop r14
        pop r13
        pop r12
        pop rbx
        ret
.no_match:
        inc r12
        jmp .scan
.not_found:
        xor eax, eax
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; json_extract_string_value — rdi=cursor pointing at opening "
; Returns rax=pointer to string content, rdx=length
json_extract_string_value:
        jmp json_scan_string

; json_skip_value — rdi=cursor. Skips one JSON value. Returns rax=cursor after value.
json_skip_value:
        push rbx
        call json_skip_whitespace
        mov rdi, rax
        movzx ecx, byte [rdi]
        cmp cl, '"'
        je .skip_string
        cmp cl, '{'
        je .skip_brace
        cmp cl, '['
        je .skip_bracket
.skip_literal:
        inc rdi
        movzx ecx, byte [rdi]
        cmp cl, ','
        je .done_literal
        cmp cl, '}'
        je .done_literal
        cmp cl, ']'
        je .done_literal
        cmp cl, ' '
        je .done_literal
        cmp cl, 9
        je .done_literal
        cmp cl, 10
        je .done_literal
        cmp cl, 13
        je .done_literal
        test cl, cl
        jz .done_literal
        jmp .skip_literal
.done_literal:
        mov rax, rdi
        pop rbx
        ret
.skip_string:
        call json_scan_string
        lea rax, [rax + rdx + 1]
        pop rbx
        ret
.skip_brace:
        mov ebx, 1
        inc rdi
.brace_loop:
        movzx ecx, byte [rdi]
        test cl, cl
        jz .brace_end
        cmp cl, '"'
        je .brace_string
        cmp cl, '{'
        je .brace_open
        cmp cl, '}'
        je .brace_close
        inc rdi
        jmp .brace_loop
.brace_string:
        call json_scan_string
        lea rdi, [rax + rdx + 1]
        jmp .brace_loop
.brace_open:
        inc ebx
        inc rdi
        jmp .brace_loop
.brace_close:
        dec ebx
        inc rdi
        test ebx, ebx
        jnz .brace_loop
.brace_end:
        mov rax, rdi
        pop rbx
        ret
.skip_bracket:
        mov ebx, 1
        inc rdi
.bracket_loop:
        movzx ecx, byte [rdi]
        test cl, cl
        jz .bracket_end
        cmp cl, '"'
        je .bracket_string
        cmp cl, '['
        je .bracket_open
        cmp cl, ']'
        je .bracket_close
        inc rdi
        jmp .bracket_loop
.bracket_string:
        call json_scan_string
        lea rdi, [rax + rdx + 1]
        jmp .bracket_loop
.bracket_open:
        inc ebx
        inc rdi
        jmp .bracket_loop
.bracket_close:
        dec ebx
        inc rdi
        test ebx, ebx
        jnz .bracket_loop
.bracket_end:
        mov rax, rdi
        pop rbx
        ret

; json_parse_response — rdi=response buffer, rsi=response length
; Returns rax=0 on success, -1 on parse failure
; Result stored in json_resp_buf
json_parse_response:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; r12 = buffer start
        lea r13, [rdi + rsi]        ; r13 = buffer end

        ; zero the result struct
        lea rdi, [json_resp_buf]
        xor sil, sil
        mov rdx, RESP_SIZE
        call mem_set

        ; check for error key
        mov rdi, r12
        lea rsi, [.key_error]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jnz .parse_error

        ; find finish_reason
        mov rdi, r12
        lea rsi, [.key_finish_reason]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .fail
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .fail
        call json_scan_string
        mov qword [json_resp_buf + RESP_FINISH_REASON], rax
        mov qword [json_resp_buf + RESP_FINISH_LEN], rdx

        ; find content
        mov rdi, r12
        lea rsi, [.key_content]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .fail
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], 'n'
        je .content_null
        cmp byte [rdi], '"'
        jne .fail
        call json_scan_string
        mov qword [json_resp_buf + RESP_CONTENT], rax
        mov qword [json_resp_buf + RESP_CONTENT_LEN], rdx
        jmp .check_tool_calls
.content_null:
        mov qword [json_resp_buf + RESP_CONTENT], 0
        mov qword [json_resp_buf + RESP_CONTENT_LEN], 0

.check_tool_calls:
        mov rdi, r12
        lea rsi, [.key_tool_calls]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .done_ok
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '['
        jne .done_ok
        mov qword [json_resp_buf + RESP_TOOL_CALLS], rdi
        call json_skip_value
        mov qword [json_resp_buf + RESP_TOOL_CALLS_END], rax

.done_ok:
        xor eax, eax
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.parse_error:
        mov qword [json_resp_buf + RESP_ERROR], 1
        mov r14, rax
        mov rdi, r14
        lea rsi, [.key_message]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .done_ok
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .done_ok
        call json_scan_string
        mov qword [json_resp_buf + RESP_ERROR_MSG], rax
        mov qword [json_resp_buf + RESP_ERROR_MSG_LEN], rdx
        xor eax, eax
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.fail:
        mov rax, -1
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.key_error         db 'error', 0
.key_finish_reason db 'finish_reason', 0
.key_content       db 'content', 0
.key_tool_calls    db 'tool_calls', 0
.key_message       db 'message', 0

; json_parse_tool_call — rdi=cursor pointing at { of a tool call object
; Result stored in json_tc_buf. Returns rax=0 on success, -1 on failure.
json_parse_tool_call:
        push rbx
        push r12
        push r13
        mov r12, rdi

        call json_skip_value
        mov r13, rax                ; r13 = end of object

        lea rdi, [json_tc_buf]
        xor sil, sil
        mov rdx, TC_SIZE
        call mem_set
        mov qword [json_tc_buf + TC_NEXT], r13

        ; find "id"
        mov rdi, r12
        lea rsi, [.tc_key_id]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .tc_fail
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .tc_fail
        call json_scan_string
        mov qword [json_tc_buf + TC_ID], rax
        mov qword [json_tc_buf + TC_ID + 8], rdx

        ; find "name"
        mov rdi, r12
        lea rsi, [.tc_key_name]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .tc_fail
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .tc_fail
        call json_scan_string
        mov qword [json_tc_buf + TC_NAME], rax
        mov qword [json_tc_buf + TC_NAME + 8], rdx

        ; find "arguments"
        mov rdi, r12
        lea rsi, [.tc_key_arguments]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .tc_fail
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .tc_fail
        call json_scan_string
        mov qword [json_tc_buf + TC_ARGS], rax
        mov qword [json_tc_buf + TC_ARGS + 8], rdx

        xor eax, eax
        pop r13
        pop r12
        pop rbx
        ret

.tc_fail:
        mov rax, -1
        pop r13
        pop r12
        pop rbx
        ret

.tc_key_id        db 'id', 0
.tc_key_name      db 'name', 0
.tc_key_arguments db 'arguments', 0

; json_unescape — rdi=src, rsi=src_len, rdx=dest
; Converts \" to ", \\ to \, \n to newline, \/ to /
; Null-terminates dest. Returns rax=unescaped length.
json_unescape:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; src
        mov r13, rsi                ; src_len
        mov rbx, rdx                ; dest
        xor ecx, ecx               ; src index
        xor r8d, r8d               ; dest index
.uloop:
        cmp rcx, r13
        jge .udone
        movzx eax, byte [r12 + rcx]
        cmp al, '\'
        jne .copy_char
        lea rdx, [rcx + 1]
        cmp rdx, r13
        jge .copy_char
        movzx edx, byte [r12 + rcx + 1]
        cmp dl, '"'
        je .esc_quote
        cmp dl, '\'
        je .esc_backslash
        cmp dl, 'n'
        je .esc_newline
        cmp dl, '/'
        je .esc_slash
        jmp .copy_char
.esc_quote:
        mov byte [rbx + r8], '"'
        inc r8
        add rcx, 2
        jmp .uloop
.esc_backslash:
        mov byte [rbx + r8], '\'
        inc r8
        add rcx, 2
        jmp .uloop
.esc_newline:
        mov byte [rbx + r8], 10
        inc r8
        add rcx, 2
        jmp .uloop
.esc_slash:
        mov byte [rbx + r8], '/'
        inc r8
        add rcx, 2
        jmp .uloop
.copy_char:
        mov byte [rbx + r8], al
        inc r8
        inc rcx
        jmp .uloop
.udone:
        mov byte [rbx + r8], 0
        mov rax, r8
        pop r13
        pop r12
        pop rbx
        ret
