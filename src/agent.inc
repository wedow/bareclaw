; agent.inc — agent loop with lazy-loaded skills
; Requires: syscalls.inc, strings.inc, arena.inc, run_capture.inc,
;           config.inc, session.inc, json.inc

MAX_TURNS    = 200
MAX_MESSAGES = 40
MAX_RETRIES  = 5

; shell_exec — rdi=command string. Returns rax=output string.
shell_exec:
        push rbx
        push r12
        mov r12, rdi

        ; build "command 2>&1" in arena
        mov rdi, r12
        call str_len
        lea rdi, [rax + 8]         ; room for " 2>&1\0"
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, r12
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov rsi, .suffix_redir
        call str_copy

        ; build argv = ["/bin/sh", "-c", cmd, NULL]
        mov rdi, 32
        call arena_alloc
        mov qword [rax], .bin_sh
        mov qword [rax + 8], .arg_c
        mov qword [rax + 16], rbx
        mov qword [rax + 24], 0
        mov rsi, rax
        lea rdi, [.bin_sh]
        call run_capture

        pop r12
        pop rbx
        ret

.bin_sh       db '/bin/sh', 0
.arg_c        db '-c', 0
.suffix_redir db ' 2>&1', 0

; http_post — rdi=request_body string. Returns rax=response string.
http_post:
        push rbx
        push r12
        mov r12, rdi

        ; build auth header: "Authorization: Bearer <key>"
        mov rdi, 512
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, .auth_prefix
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov rsi, [config_api_key]
        call str_copy

        ; build argv for curl
        mov rdi, 96                ; 11 pointers + null
        call arena_alloc
        mov qword [rax],      .curl_path
        mov qword [rax + 8],  .curl_sS
        mov qword [rax + 16], .curl_m
        mov qword [rax + 24], .curl_timeout
        mov qword [rax + 32], .curl_H
        mov qword [rax + 40], .curl_content_type
        mov qword [rax + 48], .curl_H
        mov qword [rax + 56], rbx          ; auth header
        mov qword [rax + 64], .curl_d
        mov qword [rax + 72], r12          ; body
        mov r8, [config_endpoint]
        mov qword [rax + 80], r8
        mov qword [rax + 88], 0
        mov rsi, rax
        lea rdi, [.curl_path]
        call run_capture

        pop r12
        pop rbx
        ret

.curl_path         db '/usr/bin/curl', 0
.curl_sS           db '-sS', 0
.curl_m            db '-m', 0
.curl_timeout      db '120', 0
.curl_H            db '-H', 0
.curl_content_type db 'Content-Type: application/json', 0
.curl_d            db '-d', 0
.auth_prefix       db 'Authorization: Bearer ', 0

; build_system_prompt — builds prompt with skill index in arena.
; Returns rax=pointer to prompt string.
build_system_prompt:
        push rbx
        push r12

        ; collect skill index via shell
        mov rdi, 1024
        call arena_alloc
        mov rbx, rax               ; rbx = cmd buffer
        mov rdi, rax
        mov rsi, .skill_cmd_pre
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov rsi, [config_skills_dir]
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov rsi, .skill_cmd_suf
        call str_copy

        ; execute skill index command
        mov rdi, rbx
        call shell_exec
        mov r12, rax               ; r12 = skill index output

        ; build prompt string
        mov rdi, 4096
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, .prompt_base
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov rsi, r12
        call str_copy
        ; append trailing newline
        mov rdi, rbx
        call str_len
        mov byte [rbx + rax], 10
        mov byte [rbx + rax + 1], 0

        mov rax, rbx
        pop r12
        pop rbx
        ret

.skill_cmd_pre db 'for f in ', 0
.skill_cmd_suf db '/*.md; do [ -f "$f" ] || continue; n=$(sed -n ', 27h, 's/^name: *//p', 27h, ' "$f" | head -1); d=$(sed -n ', 27h, 's/^description: *//p', 27h, ' "$f" | head -1); [ -n "$n" ] && echo "- $n: $d ($f)"; done 2>/dev/null', 0

.prompt_base db 'You are BareClaw, a minimal agentic assistant.', 10, \
                'You have one tool: shell. Use it to run any command.', 10, \
                'Be concise. Just do it.', 10, 10, \
                'Available skills (read the full file with cat if relevant):', 10, 0

; agent_run — rdi=user_input, rsi=session_fd
; Main agent loop. Returns 0 on success, -1 on error.
agent_run:
        push rbx
        push r12
        push r13
        push r14
        push r15
        push rbp
        sub rsp, 8                 ; align stack
        mov r12, rdi               ; user input
        mov r13d, esi              ; session_fd

        ; build user message JSON
        mov rdi, 131072
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        lea rsi, [.role_user]
        mov rdx, r12
        call json_build_message
        mov r14, rax               ; r14 = msg length

        ; append to session
        mov edi, r13d
        mov rsi, rbx
        mov rdx, r14
        call session_append

        ; reload messages
        mov edi, r13d
        call session_load_messages

        ; loop for max turns
        xor r15d, r15d             ; turn counter
.turn_loop:
        cmp r15d, MAX_TURNS
        jge .max_turns

        ; compact if needed
        mov rax, [msg_list_count]
        cmp rax, MAX_MESSAGES
        jle .no_compact
        mov edi, r13d
        mov rsi, MAX_MESSAGES
        call session_compact
.no_compact:

        ; build request JSON
        mov rdi, 524288
        call arena_alloc
        mov rbp, rax
        mov rdi, rax
        mov rsi, [config_model]
        mov rdx, [msg_list_ptr]
        mov rcx, [msg_list_count]
        mov r8, 1                  ; include tools
        call json_build_request

        ; reset retry state
        mov qword [retry_count], 0
        mov qword [retry_backoff], 1

.retry_request:
        ; call API
        mov rdi, rbp
        call http_post
        test rax, rax
        jz .http_error
        mov r12, rax               ; r12 = response

        ; get response length
        mov rdi, r12
        call str_len
        mov r14, rax

        ; parse response
        mov rdi, r12
        mov rsi, r14
        call json_parse_response
        test rax, rax
        jnz .parse_error

        ; check for API error
        cmp qword [json_resp_buf + RESP_ERROR], 0
        jne .api_error

        ; check finish_reason = "stop"
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 4
        jne .check_tool_calls
        mov rdi, qword [json_resp_buf + RESP_FINISH_REASON]
        lea rsi, [.str_stop]
        call .memcmp4
        test rax, rax
        jnz .check_tool_calls

        ; --- finish_reason = "stop" ---
        ; null-terminate content
        mov rax, qword [json_resp_buf + RESP_CONTENT_LEN]
        test rax, rax
        jz .stop_null

        push rax
        lea rdi, [rax + 1]
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, qword [json_resp_buf + RESP_CONTENT]
        pop rdx
        push rdx
        call mem_copy
        pop rdx
        mov byte [rbx + rdx], 0

        ; build assistant message, append to session
        mov rdi, 131072
        call arena_alloc
        mov r14, rax
        mov rdi, rax
        lea rsi, [.role_assistant]
        mov rdx, rbx
        call json_build_message
        mov rdx, rax               ; length
        mov edi, r13d
        mov rsi, r14
        call session_append

        ; print content to stdout
        mov rdi, rbx
        call str_len
        mov rdx, rax
        mov eax, 1
        mov edi, 1
        mov rsi, rbx
        syscall
        mov eax, 1
        mov edi, 1
        lea rsi, [.newline]
        mov edx, 1
        syscall

        xor eax, eax
        jmp .agent_done

.stop_null:
        ; append null-content assistant msg
        mov rdi, 256
        call arena_alloc
        mov r14, rax
        mov rdi, rax
        lea rsi, [.role_assistant]
        xor edx, edx
        call json_build_message
        mov rdx, rax
        mov edi, r13d
        mov rsi, r14
        call session_append
        xor eax, eax
        jmp .agent_done

.check_tool_calls:
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 10
        jne .other_finish

        ; build REDACTED assistant message
        mov rdi, 65536
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, .tc_msg_prefix
        call str_copy
        mov rdi, rbx
        call str_len
        push r15                   ; save turn counter
        mov r15, rax               ; r15 = write cursor

        ; iterate tool calls to build redacted array
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        push rax                   ; [rsp] = tc cursor
        mov byte [rbx + r15], '['
        inc r15
        xor r8d, r8d              ; tool call index

.redact_loop:
        mov rdi, [rsp]
        call json_skip_whitespace
        mov [rsp], rax
        cmp byte [rax], '['
        jne .redact_not_bracket
        inc qword [rsp]
        jmp .redact_loop
.redact_not_bracket:
        cmp byte [rax], ']'
        je .redact_done
        cmp byte [rax], ','
        jne .redact_parse
        inc qword [rsp]
        jmp .redact_loop
.redact_parse:
        cmp byte [rax], '{'
        jne .redact_done

        mov rdi, rax
        call json_parse_tool_call
        test rax, rax
        jnz .redact_done

        test r8d, r8d
        jz .redact_no_comma
        mov byte [rbx + r15], ','
        inc r15
.redact_no_comma:
        ; {"id":"
        lea rsi, [.redact_pre_id]
        lea rdi, [rbx + r15]
        call str_copy
        lea rdi, [rbx + r15]
        call str_len
        add r15, rax
        ; id value
        lea rdi, [rbx + r15]
        mov rsi, qword [json_tc_buf + TC_ID]
        mov rdx, qword [json_tc_buf + TC_ID + 8]
        call mem_copy
        add r15, qword [json_tc_buf + TC_ID + 8]
        ; ","type":"function","function":{"name":"
        lea rsi, [.redact_mid_name]
        lea rdi, [rbx + r15]
        call str_copy
        lea rdi, [rbx + r15]
        call str_len
        add r15, rax
        ; name value
        lea rdi, [rbx + r15]
        mov rsi, qword [json_tc_buf + TC_NAME]
        mov rdx, qword [json_tc_buf + TC_NAME + 8]
        call mem_copy
        add r15, qword [json_tc_buf + TC_NAME + 8]
        ; ","arguments":"{}"}}
        lea rsi, [.redact_suffix]
        lea rdi, [rbx + r15]
        call str_copy
        lea rdi, [rbx + r15]
        call str_len
        add r15, rax

        inc r8d
        mov rax, qword [json_tc_buf + TC_NEXT]
        mov [rsp], rax
        jmp .redact_loop

.redact_done:
        pop rax                    ; discard tc cursor
        mov byte [rbx + r15], ']'
        inc r15
        mov byte [rbx + r15], '}'
        inc r15
        mov byte [rbx + r15], 0

        ; append redacted assistant msg to session
        mov edi, r13d
        mov rsi, rbx
        mov rdx, r15
        call session_append

        ; now process each tool call: execute and append result
        ; re-parse from the original response tool_calls region
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        mov [tc_cursor], rax

.tc_exec_loop:
        mov rdi, [tc_cursor]
        call json_skip_whitespace
        mov [tc_cursor], rax
        cmp byte [rax], '['
        jne .tc_not_bracket
        inc qword [tc_cursor]
        jmp .tc_exec_loop
.tc_not_bracket:
        cmp byte [rax], ']'
        je .tc_exec_done
        cmp byte [rax], ','
        jne .tc_try_parse
        inc qword [tc_cursor]
        jmp .tc_exec_loop
.tc_try_parse:
        cmp byte [rax], '{'
        jne .tc_exec_done

        mov rdi, rax
        call json_parse_tool_call
        test rax, rax
        jnz .tc_exec_done

        ; null-terminate id
        mov rdi, qword [json_tc_buf + TC_ID + 8]
        inc rdi
        call arena_alloc
        push rax                   ; save id copy ptr
        mov rdi, rax
        mov rsi, qword [json_tc_buf + TC_ID]
        mov rdx, qword [json_tc_buf + TC_ID + 8]
        call mem_copy
        pop rax
        push rax
        mov rcx, qword [json_tc_buf + TC_ID + 8]
        mov byte [rax + rcx], 0

        ; unescape arguments
        mov rdi, qword [json_tc_buf + TC_ARGS + 8]
        add rdi, 256
        call arena_alloc
        mov rbx, rax
        mov rdi, qword [json_tc_buf + TC_ARGS]
        mov rsi, qword [json_tc_buf + TC_ARGS + 8]
        mov rdx, rbx
        call json_unescape

        ; find "command" in unescaped args
        mov rdi, rbx
        call str_len
        lea rdx, [rbx + rax]
        mov rdi, rbx
        lea rsi, [.key_command]
        call json_find_key
        test rax, rax
        jz .tc_advance

        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .tc_advance
        call json_scan_string
        mov byte [rax + rdx], 0

        ; execute command
        mov rdi, rax
        call shell_exec
        mov rbx, rax               ; rbx = output

        ; build tool result message
        pop rax                    ; rax = id copy
        push rax
        mov rdi, 131072
        call arena_alloc
        mov r14, rax
        mov rdi, rax
        pop rsi                    ; tool_call_id
        push rsi
        mov rdx, rbx
        call json_build_tool_message

        ; append to session
        mov rdx, rax               ; length
        mov edi, r13d
        mov rsi, r14
        call session_append

.tc_advance:
        pop rax                    ; discard saved id ptr
        mov rax, qword [json_tc_buf + TC_NEXT]
        mov [tc_cursor], rax
        jmp .tc_exec_loop

.tc_exec_done:
        ; reload messages from session file
        mov edi, r13d
        call session_load_messages

        ; restore turn counter, increment, continue
        pop r15
        inc r15d
        jmp .turn_loop

.other_finish:
        ; print content if any, return
        mov rax, qword [json_resp_buf + RESP_CONTENT]
        test rax, rax
        jz .agent_ret_ok
        mov rdi, qword [json_resp_buf + RESP_CONTENT_LEN]
        test rdi, rdi
        jz .agent_ret_ok
        mov eax, 1
        mov edi, 1
        mov rsi, qword [json_resp_buf + RESP_CONTENT]
        mov rdx, qword [json_resp_buf + RESP_CONTENT_LEN]
        syscall
.agent_ret_ok:
        xor eax, eax
        jmp .agent_done

.http_error:
        mov eax, 1
        mov edi, 2
        lea rsi, [.err_http]
        mov edx, .err_http_end - .err_http
        syscall
        mov rax, -1
        jmp .agent_done

.parse_error:
        mov eax, 1
        mov edi, 2
        lea rsi, [.err_parse]
        mov edx, .err_parse_end - .err_parse
        syscall
        mov rax, -1
        jmp .agent_done

.api_error:
        cmp qword [retry_count], MAX_RETRIES
        jge .api_error_final

        ; print retry message to stderr
        mov eax, 1
        mov edi, 2
        lea rsi, [.retry_prefix]
        mov edx, .retry_prefix_end - .retry_prefix
        syscall
        mov rax, qword [json_resp_buf + RESP_ERROR_MSG]
        test rax, rax
        jz .retry_no_msg
        mov eax, 1
        mov edi, 2
        mov rsi, qword [json_resp_buf + RESP_ERROR_MSG]
        mov rdx, qword [json_resp_buf + RESP_ERROR_MSG_LEN]
        syscall
.retry_no_msg:
        mov eax, 1
        mov edi, 2
        lea rsi, [.retry_wait]
        mov edx, .retry_wait_end - .retry_wait
        syscall
        ; print backoff digit
        mov rax, [retry_backoff]
        lea rdi, [retry_digit_buf]
        cmp rax, 10
        jge .two_digits
        add al, '0'
        mov byte [rdi], al
        mov ecx, 1
        jmp .print_backoff
.two_digits:
        xor edx, edx
        mov r8, 10
        div r8
        add al, '0'
        mov byte [rdi], al
        add dl, '0'
        mov byte [rdi + 1], dl
        mov ecx, 2
.print_backoff:
        mov eax, 1
        mov edi, 2
        lea rsi, [retry_digit_buf]
        mov edx, ecx
        syscall
        mov eax, 1
        mov edi, 2
        lea rsi, [.retry_suffix]
        mov edx, .retry_suffix_end - .retry_suffix
        syscall

        ; nanosleep
        mov rax, [retry_backoff]
        mov qword [timespec_buf], rax
        mov qword [timespec_buf + 8], 0
        lea rdi, [timespec_buf]
        xor esi, esi
        call sys_nanosleep
        shl qword [retry_backoff], 1
        inc qword [retry_count]
        jmp .retry_request

.api_error_final:
        mov eax, 1
        mov edi, 2
        lea rsi, [.err_api_prefix]
        mov edx, .err_api_prefix_end - .err_api_prefix
        syscall
        mov rax, qword [json_resp_buf + RESP_ERROR_MSG]
        test rax, rax
        jz .api_err_nl
        mov eax, 1
        mov edi, 2
        mov rsi, qword [json_resp_buf + RESP_ERROR_MSG]
        mov rdx, qword [json_resp_buf + RESP_ERROR_MSG_LEN]
        syscall
.api_err_nl:
        mov eax, 1
        mov edi, 2
        lea rsi, [.newline]
        mov edx, 1
        syscall
        mov rax, -1
        jmp .agent_done

.max_turns:
        mov eax, 1
        mov edi, 2
        lea rsi, [.err_turns]
        mov edx, .err_turns_end - .err_turns
        syscall
        mov rax, -1

.agent_done:
        add rsp, 8
        pop rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.memcmp4:
        mov eax, [rdi]
        mov ecx, [rsi]
        xor eax, ecx
        ret

.role_user       db 'user', 0
.role_assistant  db 'assistant', 0
.str_stop        db 'stop'
.newline         db 10
.key_command     db 'command', 0
.tc_msg_prefix   db '{"role":"assistant","content":null,"tool_calls":', 0
.redact_pre_id   db '{"id":"', 0
.redact_mid_name db '","type":"function","function":{"name":"', 0
.redact_suffix   db '","arguments":"{}"}}', 0
.err_http        db 'error: http request failed', 10
.err_http_end:
.err_parse       db 'error: failed to parse response', 10
.err_parse_end:
.err_api_prefix  db 'error: API returned error: '
.err_api_prefix_end:
.retry_prefix    db '[retry] '
.retry_prefix_end:
.retry_wait      db ', waiting '
.retry_wait_end:
.retry_suffix    db 's...', 10
.retry_suffix_end:
.err_turns       db 'error: max turns exceeded', 10
.err_turns_end:
