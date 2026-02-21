format ELF64 executable 3

include 'strings.inc'

segment readable executable

include 'syscalls.inc'
include 'arena.inc'
include 'json_build.inc'
include 'json_scan.inc'
include 'config.inc'
include 'log.inc'
include 'http.inc'
include 'shell.inc'
include 'skills.inc'
include 'agent.inc'

entry _start
_start:
        ; save argc, argv, envp
        mov rax, [rsp]
        mov [main_argc], rax
        lea rax, [rsp + 8]
        mov [main_argv], rax
        ; envp = argv + (argc+1)*8
        mov rcx, [rsp]
        inc rcx
        lea rax, [rsp + 8 + rcx*8]
        mov [main_envp], rax

        ; init env and arena
        mov rdi, [main_envp]
        call env_init
        call arena_init

        ; --help / -h check (before config, so it works without api key)
        mov rax, [main_argc]
        cmp rax, 2
        jl .no_help
        mov rcx, [main_argv]
        mov rdi, [rcx + 8]             ; argv[1]
        lea rsi, [str_help_long]
        call str_eq
        test rax, rax
        jnz .print_help
        mov rcx, [main_argv]
        mov rdi, [rcx + 8]
        lea rsi, [str_help_short]
        call str_eq
        test rax, rax
        jnz .print_help
        jmp .no_help

.print_help:
        mov eax, 1
        mov edi, 2
        lea rsi, [str_usage]
        mov edx, str_usage_len
        syscall
        xor edi, edi
        jmp .exit

.no_help:
        ; allocate config and load
        mov rdi, CONFIG_SIZE
        call arena_alloc
        mov [main_config], rax
        mov rdi, rax
        call config_load
        test rax, rax
        jns .config_ok

        ; no api key
        mov eax, 1
        mov edi, 2
        lea rsi, [str_no_key]
        mov edx, str_no_key_len
        syscall
        mov edi, 1
        jmp .exit

.config_ok:
        ; build system prompt from skills dir
        mov rax, [main_config]
        lea rdi, [rax + CONFIG_SKILLS]
        call skills_build_prompt
        mov [main_prompt], rax

        ; generate session id
        lea rdi, [session_id]
        call log_gen_session_id

        ; open log file
        mov rax, [main_config]
        lea rdi, [rax + CONFIG_LOGDIR]
        lea rsi, [session_id]
        call log_open
        mov [main_log_fd], eax

        ; init message list and append system prompt
        call msg_list_init
        lea rdi, [str_system]
        mov rsi, [main_prompt]
        call msg_list_append_role

        ; print banner: "bareclaw · MODEL · SESSION_ID\n"
        mov eax, 1
        mov edi, 2
        lea rsi, [str_banner_pre]
        mov edx, str_banner_pre_len
        syscall
        ; model name
        mov rax, [main_config]
        lea rdi, [rax + CONFIG_MODEL]
        call str_len
        mov rdx, rax
        mov rax, [main_config]
        lea rsi, [rax + CONFIG_MODEL]
        mov eax, 1
        mov edi, 2
        syscall
        ; separator
        mov eax, 1
        mov edi, 2
        lea rsi, [str_banner_mid]
        mov edx, str_banner_mid_len
        syscall
        ; session id
        mov eax, 1
        mov edi, 2
        lea rsi, [session_id]
        mov edx, 16
        syscall
        ; newline
        mov eax, 1
        mov edi, 2
        lea rsi, [str_newline]
        mov edx, 1
        syscall

        ; check for one-shot mode (argc >= 2, not --help)
        mov rax, [main_argc]
        cmp rax, 2
        jl .repl

        ; --- one-shot: concatenate argv[1..n] with spaces ---
        mov rdi, 8192
        call arena_alloc
        mov r12, rax                    ; r12 = concat buffer
        xor r13d, r13d                  ; r13 = write offset
        mov r14, 1                      ; r14 = argv index
.concat_loop:
        cmp r14, [main_argc]
        jge .concat_done
        ; space separator after first arg
        cmp r14, 1
        je .no_space
        mov byte [r12 + r13], ' '
        inc r13
.no_space:
        mov rcx, [main_argv]
        mov rdi, [rcx + r14*8]
        call str_len
        mov rdx, rax                    ; len
        lea rdi, [r12 + r13]
        mov rcx, [main_argv]
        mov rsi, [rcx + r14*8]
        call mem_copy
        add r13, rdx
        inc r14
        jmp .concat_loop
.concat_done:
        mov byte [r12 + r13], 0

        ; run agent
        mov rdi, [main_config]
        mov rsi, r12
        mov edx, [main_log_fd]
        call agent_run
        mov r12d, eax                   ; save return value

        ; cleanup
        mov edi, [main_log_fd]
        call log_close
        call arena_destroy
        mov edi, r12d
        test edi, edi
        jz .exit
        mov edi, 1
        jmp .exit

        ; --- REPL mode ---
.repl:
        ; allocate read buffer
        mov rdi, 4096
        call arena_alloc
        mov [main_readbuf], rax

.repl_loop:
        ; write "> " to stdout
        mov eax, 1
        mov edi, 1
        lea rsi, [str_prompt]
        mov edx, 2
        syscall

        ; read line from stdin
        xor edi, edi
        mov rsi, [main_readbuf]
        mov edx, 4095
        call sys_read
        cmp rax, 0
        jle .repl_exit                  ; EOF or error

        ; null-terminate, strip trailing newline
        mov rdi, [main_readbuf]
        mov byte [rdi + rax], 0
        dec rax
        js .repl_loop                   ; empty read
        cmp byte [rdi + rax], 10
        jne .no_strip
        mov byte [rdi + rax], 0
        dec rax
.no_strip:
        ; skip if empty
        cmp byte [rdi], 0
        je .repl_loop

        ; check /quit or /exit
        mov rdi, [main_readbuf]
        lea rsi, [str_quit]
        call str_eq
        test rax, rax
        jnz .repl_exit
        mov rdi, [main_readbuf]
        lea rsi, [str_exit]
        call str_eq
        test rax, rax
        jnz .repl_exit

        ; run agent
        mov rdi, [main_config]
        mov rsi, [main_readbuf]
        mov edx, [main_log_fd]
        call agent_run

        ; print newline
        mov eax, 1
        mov edi, 1
        lea rsi, [str_newline]
        mov edx, 1
        syscall

        jmp .repl_loop

.repl_exit:
        mov edi, [main_log_fd]
        call log_close
        call arena_destroy
        xor edi, edi

.exit:
        mov eax, 231
        syscall

; =====================================================================
segment readable writeable

; arena state
arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

; message list
msg_list_ptr   dq 0
msg_list_count dq 0
msg_list_cap   dq 0

; shared module state
pipe_fds    dd 0, 0
wait_status dd 0
envp_ptr    dq 0
timespec_buf dq 0, 0

; json scanner buffers
json_resp_buf rb RESP_SIZE
json_tc_buf   rb TC_SIZE

; main local state
main_argc    dq 0
main_argv    dq 0
main_envp    dq 0
main_config  dq 0
main_prompt  dq 0
main_log_fd  dd 0
main_readbuf dq 0
session_id   rb 20

; string constants
str_system     db 'system', 0
str_help_long  db '--help', 0
str_help_short db '-h', 0
str_quit       db '/quit', 0
str_exit       db '/exit', 0
str_prompt     db '> '
str_newline    db 10

str_banner_pre db 'bareclaw ', 0xC2, 0xB7, ' '
str_banner_pre_len = $ - str_banner_pre
str_banner_mid db ' ', 0xC2, 0xB7, ' '
str_banner_mid_len = $ - str_banner_mid

str_usage db 'BareClaw — skill-driven agentic runtime', 10
          db 'Usage: bareclaw ["prompt"]', 10
          db 'Config: ~/.bareclaw/config', 10
str_usage_len = $ - str_usage

str_no_key db 'error: no API key configured', 10
           db 'Set BARECLAW_API_KEY or add api_key to ~/.bareclaw/config', 10
str_no_key_len = $ - str_no_key
