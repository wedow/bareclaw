; agent.inc — message list, context compaction, and agent loop
; Requires: syscalls.inc, strings.inc, arena.inc, json_build.inc, json_scan.inc,
;           http.inc, shell.inc, log.inc, config.inc

MSG_JSON_PTR   = 0    ; pointer to pre-built JSON string (8 bytes)
MSG_JSON_LEN   = 8    ; length of JSON string (8 bytes)
MSG_ENTRY_SIZE = 16

MSG_LIST_INIT_CAP = 256

; msg_list_init — allocate initial capacity from arena, set count=0
msg_list_init:
        push rbx
        mov rdi, MSG_LIST_INIT_CAP * MSG_ENTRY_SIZE
        call arena_alloc
        mov [msg_list_ptr], rax
        mov qword [msg_list_count], 0
        mov qword [msg_list_cap], MSG_LIST_INIT_CAP
        pop rbx
        ret

; msg_list_append — rdi=json_ptr, rsi=json_len
; Appends entry to message list. Errors if full.
msg_list_append:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; json_ptr
        mov r13, rsi                ; json_len
        mov rax, [msg_list_count]
        cmp rax, [msg_list_cap]
        jge .append_full
        ; offset = count * MSG_ENTRY_SIZE
        mov rbx, [msg_list_ptr]
        shl rax, 4                  ; * 16
        mov qword [rbx + rax + MSG_JSON_PTR], r12
        mov qword [rbx + rax + MSG_JSON_LEN], r13
        inc qword [msg_list_count]
        pop r13
        pop r12
        pop rbx
        ret
.append_full:
        mov eax, 1
        mov edi, 2
        lea rsi, [.err_full]
        mov edx, .err_full_end - .err_full
        syscall
        mov eax, 231
        mov edi, 1
        syscall
        jmp .err_full_end
  .err_full db 'msg_list_append: list full', 10
  .err_full_end:

; msg_list_append_role — rdi=role, rsi=content
; Builds JSON message via json_build_message, appends to list.
; Returns rax=pointer to built JSON.
msg_list_append_role:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; role
        mov r13, rsi                ; content

        ; allocate buffer from arena for the message JSON
        mov rdi, 131072             ; 128KB max per message
        call arena_alloc
        mov rbx, rax                ; rbx = dest buffer

        ; build JSON message
        mov rdi, rbx
        mov rsi, r12
        mov rdx, r13
        call json_build_message
        ; rax = bytes written

        ; append to list
        mov rdi, rbx
        mov rsi, rax
        push rax
        call msg_list_append
        pop rax

        mov rax, rbx                ; return pointer to JSON
        pop r13
        pop r12
        pop rbx
        ret

; msg_list_append_tool_result — rdi=tool_call_id, rsi=content
; Builds tool message JSON and appends to list.
msg_list_append_tool_result:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; tool_call_id
        mov r13, rsi                ; content

        ; allocate buffer
        mov rdi, 131072
        call arena_alloc
        mov rbx, rax

        ; build tool message JSON
        mov rdi, rbx
        mov rsi, r12
        mov rdx, r13
        call json_build_tool_message
        ; rax = bytes written

        ; append
        mov rdi, rbx
        mov rsi, rax
        call msg_list_append

        mov rax, rbx
        pop r13
        pop r12
        pop rbx
        ret

; agent_process_tool_calls — rdi=tool_calls region start, rsi=tool_calls region end, rdx=log_fd
; Iterates tool calls, executes each, appends tool results to message list.
agent_process_tool_calls:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; cursor
        mov r13, rsi                ; end
        mov r14d, edx               ; log_fd

        ; skip past '[' to first object
        mov rdi, r12
        call json_skip_whitespace
        mov r12, rax
        cmp byte [r12], '['
        jne .ptc_done
        inc r12

.ptc_loop:
        ; skip whitespace
        mov rdi, r12
        call json_skip_whitespace
        mov r12, rax
        cmp r12, r13
        jge .ptc_done
        cmp byte [r12], ']'
        je .ptc_done
        cmp byte [r12], '{'
        jne .ptc_skip_comma

        ; parse this tool call
        mov rdi, r12
        call json_parse_tool_call
        test rax, rax
        jnz .ptc_done               ; parse failure, bail

        ; null-terminate id, name, args for use as C strings
        ; copy id to arena temp
        mov rdi, qword [json_tc_buf + TC_ID + 8]    ; id len
        inc rdi
        call arena_alloc
        mov r15, rax                 ; r15 = id copy
        mov rdi, rax
        mov rsi, qword [json_tc_buf + TC_ID]
        mov rdx, qword [json_tc_buf + TC_ID + 8]
        call mem_copy
        mov rax, qword [json_tc_buf + TC_ID + 8]
        mov byte [r15 + rax], 0

        ; unescape arguments to get inner JSON
        mov rdi, qword [json_tc_buf + TC_ARGS + 8]
        add rdi, 256                 ; extra room
        call arena_alloc
        mov rbx, rax                 ; rbx = unescaped args buffer
        mov rdi, qword [json_tc_buf + TC_ARGS]
        mov rsi, qword [json_tc_buf + TC_ARGS + 8]
        mov rdx, rbx
        call json_unescape
        ; rbx now holds unescaped JSON like {"command":"ls -la"}

        ; find "command" key in the unescaped args
        mov rdi, rbx
        call str_len
        lea rdx, [rbx + rax]        ; end
        mov rdi, rbx
        lea rsi, [.key_command]
        call json_find_key
        test rax, rax
        jz .ptc_advance              ; no command key, skip

        ; extract the command string value
        mov rdi, rax
        call json_skip_whitespace
        mov rdi, rax
        cmp byte [rdi], '"'
        jne .ptc_advance
        call json_scan_string
        ; rax = ptr to string content, rdx = length
        ; null-terminate the command
        mov byte [rax + rdx], 0
        push r15                     ; save id

        ; log the tool call
        push rax
        cmp r14d, 0
        jle .skip_log_tc
        mov edi, r14d
        lea rsi, [.log_tool]
        mov rdx, rax                 ; command string
        call log_write
.skip_log_tc:
        pop rax

        ; execute the command
        mov rdi, rax
        call shell_exec
        mov rbx, rax                 ; rbx = shell output

        ; log the result
        cmp r14d, 0
        jle .skip_log_result
        mov edi, r14d
        lea rsi, [.log_result]
        mov rdx, rbx
        call log_write
.skip_log_result:

        ; append tool result message
        pop r15                      ; r15 = id
        mov rdi, r15
        mov rsi, rbx
        call msg_list_append_tool_result

.ptc_advance:
        mov r12, qword [json_tc_buf + TC_NEXT]
.ptc_skip_comma:
        ; skip comma between tool calls
        mov rdi, r12
        call json_skip_whitespace
        mov r12, rax
        cmp byte [r12], ','
        jne .ptc_loop
        inc r12
        jmp .ptc_loop

.ptc_done:
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.key_command db 'command', 0
.log_tool    db 'tool', 0
.log_result  db 'result', 0

; agent_compact — rdi=config pointer, rsi=log_fd
; Simple compaction: keep system message (index 0) + last 10 messages.
; Returns 0 on success.
agent_compact:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; config
        mov r13d, esi               ; log_fd

        mov rax, [msg_list_count]
        cmp rax, [r12 + CONFIG_MAX_MSGS]
        jle .no_compact

        ; keep = 10 (or count-1 if less than 10 non-system messages)
        mov rbx, [msg_list_count]   ; total count
        mov r14, 10                 ; keep count
        lea rcx, [rbx - 1]         ; non-system msgs
        cmp rcx, r14
        jge .keep_ok
        mov r14, rcx               ; keep all non-system if < 10
.keep_ok:
        ; source start index = count - keep
        mov r15, rbx
        sub r15, r14                ; r15 = first index to keep from tail

        ; copy system msg (index 0) stays at index 0
        ; copy last r14 messages to indices 1..r14
        mov rcx, [msg_list_ptr]
        xor edx, edx               ; dest index = 1
.compact_copy:
        cmp rdx, r14
        jge .compact_done
        ; src = ptr + (r15 + edx) * 16
        mov rax, r15
        add rax, rdx
        shl rax, 4
        lea rsi, [rcx + rax]
        ; dest = ptr + (1 + edx) * 16
        lea rdi, [rdx + 1]
        shl rdi, 4
        add rdi, rcx
        ; copy 16 bytes
        mov rax, [rsi]
        mov [rdi], rax
        mov rax, [rsi + 8]
        mov [rdi + 8], rax
        inc rdx
        jmp .compact_copy

.compact_done:
        ; new count = 1 + r14
        lea rax, [r14 + 1]
        mov [msg_list_count], rax

.no_compact:
        xor eax, eax
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; agent_run — rdi=config pointer, rsi=user input, rdx=log_fd
; Main agent loop. Returns 0 on success, -1 on error.
agent_run:
        push rbx
        push r12
        push r13
        push r14
        push r15
        push rbp
        sub rsp, 8                  ; align stack to 16
        mov r12, rdi                ; config
        mov r13, rsi                ; user input
        mov r14d, edx               ; log_fd

        ; append user message
        lea rdi, [.role_user]
        mov rsi, r13
        call msg_list_append_role

        ; log user input
        cmp r14d, 0
        jle .skip_log_user
        mov edi, r14d
        lea rsi, [.role_user]
        mov rdx, r13
        call log_write
.skip_log_user:

        ; loop for max_turns
        xor r15d, r15d              ; turn counter
.turn_loop:
        mov eax, r15d
        cmp rax, [r12 + CONFIG_MAX_TURNS]
        jge .max_turns_exceeded

        ; check compaction
        mov rdi, r12
        mov esi, r14d
        call agent_compact

        ; build request JSON
        ; allocate buffer
        mov rdi, 524288             ; 512KB for request
        call arena_alloc
        mov rbp, rax                ; rbp = request buffer

        mov rdi, rbp
        lea rsi, [r12 + CONFIG_MODEL]
        mov rdx, [msg_list_ptr]
        mov rcx, [msg_list_count]
        mov r8, 1                   ; include tools
        call json_build_request
        mov rbx, rax                ; rbx = request length

        ; reset retry state for each turn
        mov qword [retry_count], 0
        mov qword [retry_backoff], 1
.retry_request:
        ; call http_post(endpoint, api_key, body)
        lea rdi, [r12 + CONFIG_ENDPOINT]
        lea rsi, [r12 + CONFIG_API_KEY]
        mov rdx, rbp
        call http_post
        test rax, rax
        jz .http_error
        mov r13, rax                ; r13 = response body

        ; get response length
        mov rdi, r13
        call str_len
        mov rbx, rax                ; rbx = response length

        ; parse response
        mov rdi, r13
        mov rsi, rbx
        call json_parse_response
        test rax, rax
        jnz .parse_error

        ; check for API error
        cmp qword [json_resp_buf + RESP_ERROR], 0
        jne .api_error

        ; check finish_reason
        ; compare with "stop"
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 4
        jne .check_tool_calls
        mov rdi, qword [json_resp_buf + RESP_FINISH_REASON]
        lea rsi, [.str_stop]
        mov rdx, 4
        call .memcmp4
        test rax, rax
        jnz .check_tool_calls

        ; finish_reason = "stop"
        ; append assistant message
        lea rdi, [.role_assistant]
        mov rsi, qword [json_resp_buf + RESP_CONTENT]
        ; need null-terminated content — copy and terminate
        mov rax, qword [json_resp_buf + RESP_CONTENT_LEN]
        test rax, rax
        jz .stop_null_content
        push rax
        lea rdi, [rax + 1]
        call arena_alloc
        mov rbx, rax                ; rbx = content copy
        mov rdi, rax
        mov rsi, qword [json_resp_buf + RESP_CONTENT]
        pop rdx
        push rdx
        call mem_copy
        pop rdx
        mov byte [rbx + rdx], 0
        lea rdi, [.role_assistant]
        mov rsi, rbx
        call msg_list_append_role

        ; print content to stdout
        mov rdi, rbx
        call str_len
        mov rdx, rax
        mov rsi, rbx
        mov eax, 1
        mov edi, 1
        syscall
        ; print newline
        lea rsi, [.newline]
        mov edx, 1
        mov eax, 1
        mov edi, 1
        syscall

        ; log
        cmp r14d, 0
        jle .stop_return
        mov edi, r14d
        lea rsi, [.role_assistant]
        mov rdx, rbx
        call log_write
.stop_return:
        xor eax, eax
        jmp .agent_done

.stop_null_content:
        lea rdi, [.role_assistant]
        xor esi, esi
        call msg_list_append_role
        xor eax, eax
        jmp .agent_done

.check_tool_calls:
        ; check if finish_reason is "tool_calls" (length 10)
        mov rax, qword [json_resp_buf + RESP_FINISH_LEN]
        cmp rax, 10
        jne .other_finish

        ; build redacted assistant message for the message list
        ; {"role":"assistant","content":null,"tool_calls":[...]}
        ; arguments are replaced with "{}" to avoid WAF triggers
        mov rdi, 65536
        call arena_alloc
        mov rbx, rax                ; rbx = buffer
        mov rdi, rbx
        lea rsi, [.tc_msg_prefix]
        call str_copy
        mov rdi, rbx
        call str_len
        push r15                    ; save turn counter
        mov r15, rax                ; r15 = write cursor

        ; iterate tool calls to build redacted array
        mov rax, qword [json_resp_buf + RESP_TOOL_CALLS]
        push rax                    ; [rsp] = tc cursor
        mov byte [rbx + r15], '['
        inc r15
        xor r8d, r8d               ; r8 = tool call index

.redact_loop:
        mov rdi, [rsp]              ; tc cursor
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

        ; parse this tool call for id and name
        mov rdi, rax
        call json_parse_tool_call
        test rax, rax
        jnz .redact_done

        ; comma before 2nd+ entries
        test r8d, r8d
        jz .redact_no_comma
        mov byte [rbx + r15], ','
        inc r15
.redact_no_comma:
        ; write: {"id":"
        lea rsi, [.redact_pre_id]
        lea rdi, [rbx + r15]
        call str_copy
        lea rdi, [rbx + r15]
        call str_len
        add r15, rax
        ; copy id value
        lea rdi, [rbx + r15]
        mov rsi, qword [json_tc_buf + TC_ID]
        mov rdx, qword [json_tc_buf + TC_ID + 8]
        call mem_copy
        add r15, qword [json_tc_buf + TC_ID + 8]
        ; write: ","type":"function","function":{"name":"
        lea rsi, [.redact_mid_name]
        lea rdi, [rbx + r15]
        call str_copy
        lea rdi, [rbx + r15]
        call str_len
        add r15, rax
        ; copy name value
        lea rdi, [rbx + r15]
        mov rsi, qword [json_tc_buf + TC_NAME]
        mov rdx, qword [json_tc_buf + TC_NAME + 8]
        call mem_copy
        add r15, qword [json_tc_buf + TC_NAME + 8]
        ; write: ","arguments":"{}"}}
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
        pop rax                     ; discard tc cursor
        ; close array and object
        mov byte [rbx + r15], ']'
        inc r15
        mov byte [rbx + r15], '}'
        inc r15
        mov byte [rbx + r15], 0

        ; append this assistant message to the list
        mov rdi, rbx
        mov rsi, r15
        call msg_list_append

        ; process tool calls
        mov rdi, qword [json_resp_buf + RESP_TOOL_CALLS]
        mov rsi, qword [json_resp_buf + RESP_TOOL_CALLS_END]
        mov edx, r14d
        call agent_process_tool_calls

        ; restore turn counter and increment
        pop r15
        inc r15d
        jmp .turn_loop

.other_finish:
        ; unknown finish reason — print content if any and return
        mov rax, qword [json_resp_buf + RESP_CONTENT]
        test rax, rax
        jz .stop_return
        mov rdi, qword [json_resp_buf + RESP_CONTENT_LEN]
        test rdi, rdi
        jz .stop_return
        ; print it
        mov eax, 1
        mov edi, 1
        mov rsi, qword [json_resp_buf + RESP_CONTENT]
        mov rdx, qword [json_resp_buf + RESP_CONTENT_LEN]
        syscall
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
        ; retry any API error with backoff
        cmp qword [retry_count], 5
        jge .api_error_final
        ; print "[retry] <error msg>, waiting Ns...\n" to stderr
        mov eax, 1
        mov edi, 2
        lea rsi, [.retry_prefix]
        mov edx, .retry_prefix_end - .retry_prefix
        syscall
        ; print the actual error message
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
        ; print backoff digit(s) — convert retry_backoff to ascii
        mov rax, [retry_backoff]
        lea rdi, [retry_digit_buf]
        xor rcx, rcx                ; digit count
        cmp rax, 10
        jge .two_digits
        add al, '0'
        mov byte [rdi], al
        mov ecx, 1
        jmp .print_backoff
.two_digits:
        ; handles up to 16
        xor edx, edx
        mov r8, 10
        div r8                      ; rax=tens, rdx=ones
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
        ; sleep for backoff seconds
        mov rax, [retry_backoff]
        mov qword [timespec_buf], rax
        mov qword [timespec_buf + 8], 0
        lea rdi, [timespec_buf]
        xor esi, esi
        call sys_nanosleep
        ; double backoff, increment retry count
        shl qword [retry_backoff], 1
        inc qword [retry_count]
        jmp .retry_request

.api_error_final:
        ; print error message and fail
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

.max_turns_exceeded:
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

; helper: compare 4 bytes at rdi with rsi, return 0 if equal
.memcmp4:
        mov eax, [rdi]
        mov ecx, [rsi]
        xor eax, ecx
        ret

.role_user      db 'user', 0
.role_assistant db 'assistant', 0
.str_stop       db 'stop'
.newline        db 10
.tc_msg_prefix   db '{"role":"assistant","content":null,"tool_calls":', 0
.redact_pre_id   db '{"id":"', 0
.redact_mid_name db '","type":"function","function":{"name":"', 0
.redact_suffix   db '","arguments":"{}"}}', 0
.err_http       db 'error: http request failed', 10
.err_http_end:
.err_parse      db 'error: failed to parse response', 10
.err_parse_end:
.err_api_prefix db 'error: API returned error: '
.err_api_prefix_end:
.retry_prefix   db '[retry] '
.retry_prefix_end:
.retry_wait     db ', waiting '
.retry_wait_end:
.retry_suffix   db 's...', 10
.retry_suffix_end:
.err_turns      db 'error: max turns exceeded', 10
.err_turns_end:
