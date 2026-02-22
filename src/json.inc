; json.inc — JSON scanning and building for OpenAI chat completion API
; Requires: strings.inc

; --- Response struct (from scanning) ---
RESP_FINISH_REASON = 0    ; ptr (8), len (8)
RESP_FINISH_LEN    = 8
RESP_CONTENT       = 16   ; ptr (8), len (8)
RESP_CONTENT_LEN   = 24
RESP_TOOL_CALLS    = 32   ; ptr (8), end ptr (8)
RESP_TOOL_CALLS_END= 40
RESP_ERROR         = 48   ; 1 if error, 0 if normal
RESP_ERROR_MSG     = 56   ; ptr (8), len (8)
RESP_ERROR_MSG_LEN = 64
RESP_SIZE          = 72

; --- Tool call struct ---
TC_ID     = 0             ; ptr (8), len (8)
TC_NAME   = 16            ; ptr (8), len (8)
TC_ARGS   = 32            ; ptr (8), len (8)
TC_NEXT   = 48            ; cursor after this object (8)
TC_SIZE   = 56

; --- Message entry struct (for building) ---
; msg_entry: ptr dq, len dq  (16 bytes per entry)

; ============================================================
; SCANNING — cursor-based JSON scanner for OpenAI responses
; ============================================================

; json_skip_whitespace — rdi=cursor, returns rax=cursor past whitespace
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

; json_scan_string — rdi=cursor at opening "
; Returns rax=ptr past opening quote, rdx=length to closing quote
; Handles \" escapes. Next cursor = rax + rdx + 1.
json_scan_string:
        lea rax, [rdi + 1]
        mov rcx, rax
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
        add rcx, 2
        jmp .scan
.close:
        mov rdx, rcx
        sub rdx, rax
        ret
.eof:
        mov rdx, rcx
        sub rdx, rax
        ret

; json_find_key — rdi=buf start, rsi=key (null-term), rdx=buf end
; Scans for "key": pattern. Returns rax=cursor after colon, or 0.
json_find_key:
        push rbx
        push r12
        push r13
        push r14
        mov r12, rdi
        mov r13, rsi
        mov r14, rdx
        mov rdi, r13
        call str_len
        mov rbx, rax
.scan:
        cmp r12, r14
        jge .not_found
        cmp byte [r12], '"'
        je .check_key
        inc r12
        jmp .scan
.check_key:
        lea rcx, [r12 + 1]
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
        inc rax
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
        inc rax
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

; json_skip_value — rdi=cursor. Skips one JSON value. Returns rax=cursor after.
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
        je .done_lit
        cmp cl, '}'
        je .done_lit
        cmp cl, ']'
        je .done_lit
        cmp cl, ' '
        je .done_lit
        cmp cl, 9
        je .done_lit
        cmp cl, 10
        je .done_lit
        cmp cl, 13
        je .done_lit
        test cl, cl
        jz .done_lit
        jmp .skip_literal
.done_lit:
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
        je .brace_str
        cmp cl, '{'
        je .brace_open
        cmp cl, '}'
        je .brace_close
        inc rdi
        jmp .brace_loop
.brace_str:
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
        je .bracket_str
        cmp cl, '['
        je .bracket_open
        cmp cl, ']'
        je .bracket_close
        inc rdi
        jmp .bracket_loop
.bracket_str:
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

; json_parse_response — rdi=buffer, rsi=length
; Result in json_resp_buf. Returns rax=0 on success, -1 on parse failure.
; Error responses return 0 with RESP_ERROR=1 (not -1).
json_parse_response:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi
        lea r13, [rdi + rsi]

        lea rdi, [json_resp_buf]
        xor sil, sil
        mov rdx, RESP_SIZE
        call mem_set

        ; check for error
        mov rdi, r12
        lea rsi, [.k_error]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jnz .parse_error

        ; finish_reason
        mov rdi, r12
        lea rsi, [.k_finish_reason]
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

        ; content
        mov rdi, r12
        lea rsi, [.k_content]
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
        lea rsi, [.k_tool_calls]
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
        lea rsi, [.k_message]
        mov rdx, r13
        call json_find_key
        test rax, rax
        jz .error_done
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .error_done
        call json_scan_string
        mov qword [json_resp_buf + RESP_ERROR_MSG], rax
        mov qword [json_resp_buf + RESP_ERROR_MSG_LEN], rdx
.error_done:
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

.k_error         db 'error', 0
.k_finish_reason db 'finish_reason', 0
.k_content       db 'content', 0
.k_tool_calls    db 'tool_calls', 0
.k_message       db 'message', 0

; json_parse_tool_call — rdi=cursor at { of tool call object
; Result in json_tc_buf. Returns rax=0 success, -1 failure.
json_parse_tool_call:
        push rbx
        push r12
        push r13
        mov r12, rdi

        call json_skip_value
        mov r13, rax

        lea rdi, [json_tc_buf]
        xor sil, sil
        mov rdx, TC_SIZE
        call mem_set
        mov qword [json_tc_buf + TC_NEXT], r13

        ; id
        mov rdi, r12
        lea rsi, [.tc_k_id]
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

        ; name
        mov rdi, r12
        lea rsi, [.tc_k_name]
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

        ; arguments
        mov rdi, r12
        lea rsi, [.tc_k_arguments]
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

.tc_k_id        db 'id', 0
.tc_k_name      db 'name', 0
.tc_k_arguments db 'arguments', 0

; json_unescape — rdi=src, rsi=src_len, rdx=dest
; Converts \" to ", \\ to \, \n to newline, \/ to /
; Null-terminates. Returns rax=unescaped length.
json_unescape:
        push rbx
        push r12
        push r13
        mov r12, rdi
        mov r13, rsi
        mov rbx, rdx
        xor ecx, ecx
        xor r8d, r8d
.uloop:
        cmp rcx, r13
        jge .udone
        movzx eax, byte [r12 + rcx]
        cmp al, '\'
        jne .ucopy
        lea rdx, [rcx + 1]
        cmp rdx, r13
        jge .ucopy
        movzx edx, byte [r12 + rcx + 1]
        cmp dl, '"'
        je .esc_quote
        cmp dl, '\'
        je .esc_backslash
        cmp dl, 'n'
        je .esc_newline
        cmp dl, '/'
        je .esc_slash
        jmp .ucopy
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
.ucopy:
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

; ============================================================
; BUILDING — construct OpenAI chat completion request JSON
; ============================================================

; json_escape_string — rdi=src (null-term), rsi=dest, rdx=max dest size
; Escapes " \ newline tab cr. Null-terminates. Returns rax=bytes written.
json_escape_string:
        push rbx
        push r12
        push r13
        mov r12, rdi
        mov r13, rsi
        mov rbx, rdx
        xor ecx, ecx
        xor r8d, r8d
.eloop:
        movzx eax, byte [r12 + rcx]
        test al, al
        jz .edone
        lea rdx, [r8 + 2]
        cmp rdx, rbx
        ja .edone
        cmp al, '"'
        je .eq
        cmp al, '\'
        je .ebs
        cmp al, 10
        je .enl
        cmp al, 9
        je .etab
        cmp al, 13
        je .ecr
        lea rdx, [r8 + 1]
        cmp rdx, rbx
        ja .edone
        mov byte [r13 + r8], al
        inc r8
        inc rcx
        jmp .eloop
.eq:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], '"'
        add r8, 2
        inc rcx
        jmp .eloop
.ebs:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], '\'
        add r8, 2
        inc rcx
        jmp .eloop
.enl:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], 'n'
        add r8, 2
        inc rcx
        jmp .eloop
.etab:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], 't'
        add r8, 2
        inc rcx
        jmp .eloop
.ecr:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], 'r'
        add r8, 2
        inc rcx
        jmp .eloop
.edone:
        mov byte [r13 + r8], 0
        mov rax, r8
        pop r13
        pop r12
        pop rbx
        ret

; json_build_message — rdi=dest, rsi=role, rdx=content (NULL for null)
; Writes {"role":"ROLE","content":"ESCAPED"} or {"role":"ROLE","content":null}
; Returns rax=bytes written
json_build_message:
        push rbx
        push r12
        push r13
        push r14
        mov r12, rdi
        mov r13, rsi
        mov r14, rdx
        xor ebx, ebx

        ; {"role":"
        lea rsi, [.pre_role]
        mov rdi, r12
        call str_copy
        mov rdi, r12
        call str_len
        add ebx, eax

        ; role
        lea rdi, [r12 + rbx]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        ; ","content":
        lea rsi, [.mid_content]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        test r14, r14
        jz .null_content

        mov byte [r12 + rbx], '"'
        inc ebx
        mov rdi, r14
        lea rsi, [r12 + rbx]
        mov rdx, 65536
        call json_escape_string
        add ebx, eax
        mov byte [r12 + rbx], '"'
        inc ebx
        jmp .close_msg

.null_content:
        lea rsi, [.str_null]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

.close_msg:
        mov byte [r12 + rbx], '}'
        inc ebx
        mov byte [r12 + rbx], 0
        mov eax, ebx
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.pre_role    db '{"role":"', 0
.mid_content db '","content":', 0
.str_null    db 'null', 0

; json_build_tool_message — rdi=dest, rsi=tool_call_id, rdx=content
; Writes {"role":"tool","tool_call_id":"ID","content":"ESCAPED"}
; Returns rax=bytes written
json_build_tool_message:
        push rbx
        push r12
        push r13
        push r14
        mov r12, rdi
        mov r13, rsi
        mov r14, rdx
        xor ebx, ebx

        lea rsi, [.pre_tcid]
        mov rdi, r12
        call str_copy
        mov rdi, r12
        call str_len
        add ebx, eax

        lea rdi, [r12 + rbx]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        lea rsi, [.mid_tc_content]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        mov rdi, r14
        lea rsi, [r12 + rbx]
        mov rdx, 65536
        call json_escape_string
        add ebx, eax

        mov byte [r12 + rbx], '"'
        inc ebx
        mov byte [r12 + rbx], '}'
        inc ebx
        mov byte [r12 + rbx], 0
        mov eax, ebx
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.pre_tcid       db '{"role":"tool","tool_call_id":"', 0
.mid_tc_content db '","content":"', 0

; json_build_request — rdi=dest, rsi=model, rdx=msg array, rcx=msg count, r8=include tools
; msg array: array of {ptr dq, len dq} entries (16 bytes each)
; Returns rax=bytes written
json_build_request:
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi
        mov r13, rsi
        mov r14, rdx
        mov r15, rcx
        mov rbp, r8
        xor ebx, ebx

        lea rsi, [.pre_model]
        mov rdi, r12
        call str_copy
        mov rdi, r12
        call str_len
        add ebx, eax

        lea rdi, [r12 + rbx]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        lea rsi, [.mid_messages]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        xor ecx, ecx
.msg_loop:
        cmp rcx, r15
        jge .msg_done
        push rcx
        test rcx, rcx
        jz .no_comma
        mov byte [r12 + rbx], ','
        inc ebx
.no_comma:
        pop rcx
        push rcx
        shl rcx, 4
        mov rsi, [r14 + rcx]
        mov rdx, [r14 + rcx + 8]
        lea rdi, [r12 + rbx]
        call mem_copy
        pop rcx
        push rcx
        shl rcx, 4
        mov rax, [r14 + rcx + 8]
        add ebx, eax
        pop rcx
        inc rcx
        jmp .msg_loop

.msg_done:
        mov byte [r12 + rbx], ']'
        inc ebx

        test rbp, rbp
        jz .no_tools

        lea rsi, [.mid_tools]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        lea rsi, [json_tools_def]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

.no_tools:
        mov byte [r12 + rbx], '}'
        inc ebx
        mov byte [r12 + rbx], 0
        mov eax, ebx

        pop r15
        pop r14
        pop r13
        pop r12
        pop rbp
        pop rbx
        ret

.pre_model    db '{"model":"', 0
.mid_messages db '","messages":[', 0
.mid_tools    db ',"tools":', 0

json_tools_def db '[{"type":"function","function":{"name":"shell","description":"Run a shell command","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}]', 0
