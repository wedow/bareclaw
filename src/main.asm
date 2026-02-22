format ELF64 executable 3

segment readable executable

include 'syscalls.inc'
include 'strings.inc'
include 'arena.inc'
include 'run_capture.inc'
include 'config.inc'
include 'json.inc'
include 'session.inc'
include 'agent.inc'

entry _start
_start:
        ; save argc, argv, envp
        mov rax, [rsp]
        mov [argc], rax
        lea rax, [rsp + 8]
        mov [argv], rax
        ; envp = argv + (argc+1)*8
        mov rcx, [argc]
        inc rcx
        lea rax, [rsp + 8 + rcx*8]
        mov [envp], rax

        ; quick scan for --help/-h before any init
        mov r12, [argv]
        mov r13, [argc]
        mov ecx, 1
.help_scan:
        cmp rcx, r13
        jge .no_help
        mov rdi, [r12 + rcx*8]
        push rcx
        lea rsi, [str_help_long]
        call str_eq
        pop rcx
        test rax, rax
        jnz .show_help
        mov rdi, [r12 + rcx*8]
        push rcx
        lea rsi, [str_help_short]
        call str_eq
        pop rcx
        test rax, rax
        jnz .show_help
        inc rcx
        jmp .help_scan
.no_help:

        ; env_init(envp)
        mov rdi, [envp]
        call env_init

        ; arena_init
        call arena_init

        ; config_init
        call config_init
        test rax, rax
        js .config_fail

        ; parse arguments
        mov qword [session_id], 0
        mov qword [prompt_ptr], 0
        mov r12, [argv]
        mov r13, [argc]
        mov ecx, 1                     ; i = 1

.parse_loop:
        cmp rcx, r13
        jge .parse_done
        mov rdi, [r12 + rcx*8]

        ; check --help
        push rcx
        lea rsi, [str_help_long]
        call str_eq
        pop rcx
        test rax, rax
        jnz .show_help

        ; check -h
        mov rdi, [r12 + rcx*8]
        push rcx
        lea rsi, [str_help_short]
        call str_eq
        pop rcx
        test rax, rax
        jnz .show_help

        ; check --session
        mov rdi, [r12 + rcx*8]
        push rcx
        lea rsi, [str_session_long]
        call str_eq
        pop rcx
        test rax, rax
        jnz .got_session_flag

        ; check -s
        mov rdi, [r12 + rcx*8]
        push rcx
        lea rsi, [str_session_short]
        call str_eq
        pop rcx
        test rax, rax
        jnz .got_session_flag

        ; otherwise it's prompt (or part of it)
        mov rax, [r12 + rcx*8]
        cmp qword [prompt_ptr], 0
        jne .concat_prompt
        mov [prompt_ptr], rax
        inc rcx
        jmp .parse_loop

.concat_prompt:
        ; concatenate: append space + this arg to prompt_ptr
        ; find end of current prompt
        push rcx
        mov rdi, [prompt_ptr]
        call str_len
        mov rbx, rax                    ; current prompt len
        mov rcx, [rsp]
        mov rdi, [r12 + rcx*8]
        call str_len
        mov r14, rax                    ; arg len
        pop rcx

        ; allocate new buffer: prompt_len + 1 + arg_len + 1
        push rcx
        lea rdi, [rbx + r14 + 2]
        call arena_alloc
        mov r15, rax
        mov rdi, rax
        mov rsi, [prompt_ptr]
        mov rdx, rbx
        call mem_copy
        mov byte [r15 + rbx], ' '
        lea rdi, [r15 + rbx + 1]
        mov rcx, [rsp]
        mov rsi, [r12 + rcx*8]
        mov rdx, r14
        call mem_copy
        lea rax, [rbx + r14 + 1]
        mov byte [r15 + rax], 0
        mov [prompt_ptr], r15
        pop rcx
        inc rcx
        jmp .parse_loop

.got_session_flag:
        inc rcx
        cmp rcx, r13
        jge .parse_done
        mov rax, [r12 + rcx*8]
        mov [session_id], rax
        inc rcx
        jmp .parse_loop

.parse_done:
        ; session setup
        cmp qword [session_id], 0
        jne .resume_session

        ; new session: generate id
        lea rdi, [session_id_buf]
        call session_gen_id
        mov [session_id], rax

        ; build system prompt
        call build_system_prompt
        mov [sys_prompt_ptr], rax

        ; create session file
        mov rdi, [session_id]
        call session_create
        test rax, rax
        js .session_create_fail
        mov [session_fd], eax

        ; build system message JSON, append to session
        mov rdi, 131072
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        lea rsi, [str_role_system]
        mov rdx, [sys_prompt_ptr]
        call json_build_message
        mov rdx, rax
        mov edi, [session_fd]
        mov rsi, rbx
        call session_append

        jmp .session_ready

.resume_session:
        mov rdi, [session_id]
        call session_open
        test rax, rax
        js .session_not_found
        mov [session_fd], eax

.session_ready:
        ; print banner to stderr: "bareclaw · MODEL · SESSION_ID\n"
        mov edi, 2
        lea rsi, [str_banner_pre]
        mov edx, str_banner_pre_len
        mov eax, 1
        syscall
        mov rdi, [config_model]
        call str_len
        mov rdx, rax
        mov edi, 2
        mov rsi, [config_model]
        mov eax, 1
        syscall
        mov edi, 2
        lea rsi, [str_banner_mid]
        mov edx, str_banner_mid_len
        mov eax, 1
        syscall
        mov rdi, [session_id]
        call str_len
        mov rdx, rax
        mov edi, 2
        mov rsi, [session_id]
        mov eax, 1
        syscall
        mov edi, 2
        lea rsi, [str_newline]
        mov edx, 1
        mov eax, 1
        syscall

        ; one-shot or REPL?
        cmp qword [prompt_ptr], 0
        je .repl_mode

        ; one-shot: agent_run(prompt, session_fd)
        mov rdi, [prompt_ptr]
        mov esi, [session_fd]
        call agent_run
        mov r12, rax

        ; close session fd
        mov edi, [session_fd]
        call sys_close

        ; exit with agent_run return code (0 or 1)
        mov edi, r12d
        test r12, r12
        jz .exit
        mov edi, 1
.exit:
        mov eax, 231
        syscall

.repl_mode:
        ; load existing messages for resumed sessions
        mov edi, [session_fd]
        call session_load_messages

.repl_loop:
        ; write "> " to stdout
        mov edi, 1
        lea rsi, [str_repl_prompt]
        mov edx, 2
        mov eax, 1
        syscall

        ; read line from stdin
        mov edi, 0
        lea rsi, [repl_buf]
        mov edx, 4095
        mov eax, 0
        syscall
        cmp rax, 0
        jle .repl_exit

        ; strip trailing newline
        mov rcx, rax
        dec rcx
        cmp byte [repl_buf + rcx], 10
        jne .no_strip
        mov byte [repl_buf + rcx], 0
        jmp .check_empty
.no_strip:
        mov byte [repl_buf + rax], 0
.check_empty:
        cmp byte [repl_buf], 0
        je .repl_loop

        ; check /quit
        lea rdi, [repl_buf]
        lea rsi, [str_quit]
        call str_eq
        test rax, rax
        jnz .repl_exit

        ; check /exit
        lea rdi, [repl_buf]
        lea rsi, [str_exit]
        call str_eq
        test rax, rax
        jnz .repl_exit

        ; agent_run(input, session_fd)
        lea rdi, [repl_buf]
        mov esi, [session_fd]
        call agent_run

        ; write newline to stdout
        mov edi, 1
        lea rsi, [str_newline]
        mov edx, 1
        mov eax, 1
        syscall

        jmp .repl_loop

.repl_exit:
        mov edi, [session_fd]
        call sys_close
        xor edi, edi
        mov eax, 231
        syscall

.show_help:
        mov edi, 1
        lea rsi, [str_usage]
        mov edx, str_usage_len
        mov eax, 1
        syscall
        xor edi, edi
        mov eax, 231
        syscall

.config_fail:
        mov edi, 2
        lea rsi, [str_config_err]
        mov edx, str_config_err_len
        mov eax, 1
        syscall
        mov edi, 1
        mov eax, 231
        syscall

.session_create_fail:
        mov edi, 2
        lea rsi, [str_session_err]
        mov edx, str_session_err_len
        mov eax, 1
        syscall
        mov edi, 1
        mov eax, 231
        syscall

.session_not_found:
        mov edi, 2
        lea rsi, [str_session_nf]
        mov edx, str_session_nf_len
        mov eax, 1
        syscall
        mov edi, 1
        mov eax, 231
        syscall

segment readable writeable

; --- stack save ---
argc dq 0
argv dq 0
envp dq 0

; --- parsed args ---
session_id     dq 0
prompt_ptr     dq 0
session_fd     dd 0
sys_prompt_ptr dq 0
session_id_buf rb 17

; --- arena ---
arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

; --- run_capture ---
pipe_fds    dd 0, 0
wait_status dd 0

; --- config ---
envp_ptr   dq 0

config_api_key      dq 0
config_model        dq 0
config_endpoint     dq 0
config_skills_dir   dq 0
config_sessions_dir dq 0

; --- agent/session ---
msg_list_ptr   dq 0
msg_list_count dq 0

json_resp_buf rb RESP_SIZE
json_tc_buf   rb TC_SIZE

tc_cursor       dq 0
retry_count     dq 0
retry_backoff   dq 0
retry_digit_buf rb 4
timespec_buf    dq 0, 0

; --- REPL ---
repl_buf rb 4096

; --- strings ---
str_help_long     db '--help', 0
str_help_short    db '-h', 0
str_session_long  db '--session', 0
str_session_short db '-s', 0
str_role_system   db 'system', 0
str_quit          db '/quit', 0
str_exit          db '/exit', 0
str_repl_prompt   db '> '
str_newline       db 10

str_banner_pre db 'bareclaw ', 0xC2, 0xB7, ' '
str_banner_pre_len = $ - str_banner_pre
str_banner_mid db ' ', 0xC2, 0xB7, ' '
str_banner_mid_len = $ - str_banner_mid

str_usage db 'usage: bareclaw [options] [prompt]', 10, \
             10, \
             'options:', 10, \
             '  -s, --session ID   resume a session', 10, \
             '  -h, --help         show this help', 10, \
             10, \
             'modes:', 10, \
             '  bareclaw "prompt"         new session, one-shot', 10, \
             '  bareclaw -s ID "prompt"   resume session, one-shot', 10, \
             '  bareclaw -s ID            resume session, REPL', 10, \
             '  bareclaw                  new session, REPL', 10
str_usage_len = $ - str_usage

str_config_err db 'error: BARECLAW_API_KEY not set', 10
str_config_err_len = $ - str_config_err

str_session_err db 'error: failed to create session', 10
str_session_err_len = $ - str_session_err

str_session_nf db 'error: session not found', 10
str_session_nf_len = $ - str_session_nf
