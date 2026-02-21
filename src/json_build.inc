; json_build.inc — build OpenAI chat completion request JSON
; Requires: strings.inc

; json_escape_string — rdi=src, rsi=dest, rdx=max dest size
; Escapes " \ newline tab cr. Null-terminates. Returns rax=bytes written.
json_escape_string:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; src
        mov r13, rsi                ; dest
        mov rbx, rdx                ; max size
        xor ecx, ecx               ; src index
        xor r8d, r8d               ; dest index
.loop:
        movzx eax, byte [r12 + rcx]
        test al, al
        jz .done
        lea rdx, [r8 + 2]
        cmp rdx, rbx
        ja .done                    ; need room for escape + null
        cmp al, '"'
        je .esc_quote
        cmp al, '\'
        je .esc_backslash
        cmp al, 10
        je .esc_newline
        cmp al, 9
        je .esc_tab
        cmp al, 13
        je .esc_cr
        ; regular char
        lea rdx, [r8 + 1]
        cmp rdx, rbx
        ja .done
        mov byte [r13 + r8], al
        inc r8
        inc rcx
        jmp .loop
.esc_quote:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], '"'
        add r8, 2
        inc rcx
        jmp .loop
.esc_backslash:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], '\'
        add r8, 2
        inc rcx
        jmp .loop
.esc_newline:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], 'n'
        add r8, 2
        inc rcx
        jmp .loop
.esc_tab:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], 't'
        add r8, 2
        inc rcx
        jmp .loop
.esc_cr:
        mov byte [r13 + r8], '\'
        mov byte [r13 + r8 + 1], 'r'
        add r8, 2
        inc rcx
        jmp .loop
.done:
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
        mov r12, rdi                ; dest
        mov r13, rsi                ; role
        mov r14, rdx                ; content
        xor ebx, ebx               ; write cursor

        ; {"role":"
        lea rsi, [.pre_role]
        mov rdi, r12
        call str_copy
        mov rdi, r12
        call str_len
        add ebx, eax

        ; role string
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

        ; content value
        test r14, r14
        jz .null_content

        ; "ESCAPED"
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
        mov r12, rdi                ; dest
        mov r13, rsi                ; tool_call_id
        mov r14, rdx                ; content
        xor ebx, ebx

        ; {"role":"tool","tool_call_id":"
        lea rsi, [.pre_tcid]
        mov rdi, r12
        call str_copy
        mov rdi, r12
        call str_len
        add ebx, eax

        ; tool_call_id
        lea rdi, [r12 + rbx]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        ; ","content":"
        lea rsi, [.mid_tc_content]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        ; escaped content
        mov rdi, r14
        lea rsi, [r12 + rbx]
        mov rdx, 65536
        call json_escape_string
        add ebx, eax

        ; "}
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
; msg array: array of {ptr dq, len dq} entries (pre-built JSON strings)
; Returns rax=bytes written
json_build_request:
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; dest
        mov r13, rsi                ; model
        mov r14, rdx                ; msg array
        mov r15, rcx                ; msg count
        mov rbp, r8                 ; include tools flag
        xor ebx, ebx               ; write cursor

        ; {"model":"
        lea rsi, [.pre_model]
        mov rdi, r12
        call str_copy
        mov rdi, r12
        call str_len
        add ebx, eax

        ; model string
        lea rdi, [r12 + rbx]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        ; ","messages":[
        lea rsi, [.mid_messages]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        ; iterate messages
        xor ecx, ecx               ; index
.msg_loop:
        cmp rcx, r15
        jge .msg_done
        push rcx

        ; comma separator after first
        test rcx, rcx
        jz .no_comma
        mov byte [r12 + rbx], ','
        inc ebx
.no_comma:
        ; copy message: ptr at [r14 + rcx*16], len at [r14 + rcx*16 + 8]
        pop rcx
        push rcx
        shl rcx, 4                 ; rcx * 16
        mov rsi, [r14 + rcx]       ; ptr
        mov rdx, [r14 + rcx + 8]   ; len
        ; mem_copy dest, src, count
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
        ; ]
        mov byte [r12 + rbx], ']'
        inc ebx

        ; tools?
        test rbp, rbp
        jz .no_tools

        ; ,"tools":
        lea rsi, [.mid_tools]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

        ; tools JSON constant
        lea rsi, [json_tools_def]
        lea rdi, [r12 + rbx]
        call str_copy
        lea rdi, [r12 + rbx]
        call str_len
        add ebx, eax

.no_tools:
        ; }
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
