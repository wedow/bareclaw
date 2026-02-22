; config.inc — env-vars-only config
; Requires: syscalls.inc, strings.inc, arena.inc

; env_init — rdi=envp pointer, stores globally
env_init:
        mov [envp_ptr], rdi
        ret

; env_get — rdi=name string, returns rax=pointer to value after '=' or 0
env_get:
        push rbx
        push r12
        push r13
        mov r12, rdi
        call str_len
        mov r13, rax
        mov rbx, [envp_ptr]
.scan:
        mov rdi, [rbx]
        test rdi, rdi
        jz .not_found
        mov rsi, r12
        call str_starts_with
        test rax, rax
        jz .next
        mov rdi, [rbx]
        cmp byte [rdi + r13], '='
        jne .next
        lea rax, [rdi + r13 + 1]
        pop r13
        pop r12
        pop rbx
        ret
.next:
        add rbx, 8
        jmp .scan
.not_found:
        xor eax, eax
        pop r13
        pop r12
        pop rbx
        ret

; _config_build_path — r12=HOME, rsi=suffix. Allocs in arena, returns rax=path.
_config_build_path:
        push rbx
        push r13
        mov r13, rsi
        mov rdi, 256
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, r12
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov rsi, r13
        call str_copy
        mov rax, rbx
        pop r13
        pop rbx
        ret

; config_init — read env vars, set globals with defaults
; Returns 0 on success, -1 if no api_key
config_init:
        push rbx
        push r12

        ; api_key (required)
        lea rdi, [.env_api_key]
        call env_get
        mov [config_api_key], rax

        ; model
        lea rdi, [.env_model]
        call env_get
        test rax, rax
        jnz .set_model
        lea rax, [.default_model]
.set_model:
        mov [config_model], rax

        ; endpoint
        lea rdi, [.env_endpoint]
        call env_get
        test rax, rax
        jnz .set_endpoint
        lea rax, [.default_endpoint]
.set_endpoint:
        mov [config_endpoint], rax

        ; get HOME for path defaults
        lea rdi, [.env_home]
        call env_get
        mov r12, rax

        ; skills_dir
        lea rdi, [.env_skills]
        call env_get
        test rax, rax
        jnz .set_skills
        test r12, r12
        jz .no_skills
        lea rsi, [.suffix_skills]
        call _config_build_path
.set_skills:
        mov [config_skills_dir], rax
.no_skills:

        ; sessions_dir
        lea rdi, [.env_sessions]
        call env_get
        test rax, rax
        jnz .set_sessions
        test r12, r12
        jz .no_sessions
        lea rsi, [.suffix_sessions]
        call _config_build_path
.set_sessions:
        mov [config_sessions_dir], rax
.no_sessions:

        ; check api_key
        cmp qword [config_api_key], 0
        je .no_key
        xor eax, eax
        pop r12
        pop rbx
        ret
.no_key:
        mov rax, -1
        pop r12
        pop rbx
        ret

.env_api_key  db 'BARECLAW_API_KEY', 0
.env_model    db 'BARECLAW_MODEL', 0
.env_endpoint db 'BARECLAW_ENDPOINT', 0
.env_skills   db 'BARECLAW_SKILLS_DIR', 0
.env_sessions db 'BARECLAW_SESSIONS_DIR', 0
.env_home     db 'HOME', 0

.default_model    db 'anthropic/claude-haiku-4.5', 0
.default_endpoint db 'https://openrouter.ai/api/v1/chat/completions', 0
.suffix_skills    db '/.bareclaw/skills', 0
.suffix_sessions  db '/.bareclaw/sessions', 0
