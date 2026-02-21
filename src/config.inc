; config.inc — config file parser and env var reader
; Requires: syscalls.inc, strings.inc, arena.inc

; Config struct offsets
CONFIG_API_KEY   = 0       ; 256 bytes
CONFIG_MODEL     = 256     ; 256 bytes
CONFIG_ENDPOINT  = 512     ; 256 bytes
CONFIG_SKILLS    = 768     ; 256 bytes
CONFIG_LOGDIR    = 1024    ; 256 bytes
CONFIG_MAX_TURNS = 1280    ; 8 bytes (qword)
CONFIG_MAX_MSGS  = 1288    ; 8 bytes (qword)
CONFIG_SIZE      = 1296

; env_init — rdi=envp pointer, stores globally
env_init:
        mov [envp_ptr], rdi
        ret

; env_get — rdi=name string, returns rax=pointer to value after '=' or 0
env_get:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; r12 = name to find
        ; get name length
        call str_len
        mov r13, rax                ; r13 = name length
        mov rbx, [envp_ptr]         ; rbx = current envp entry pointer
.scan:
        mov rdi, [rbx]
        test rdi, rdi
        jz .not_found
        ; check if entry starts with name
        mov rsi, r12
        call str_starts_with
        test rax, rax
        jz .next
        ; check that the char after name is '='
        mov rdi, [rbx]
        cmp byte [rdi + r13], '='
        jne .next
        ; found — return pointer past '='
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

; str_to_int — rdi=string, returns rax=unsigned integer
str_to_int:
        xor rax, rax
.loop:
        movzx ecx, byte [rdi]
        sub cl, '0'
        cmp cl, 9
        ja .done
        imul rax, 10
        add rax, rcx
        inc rdi
        jmp .loop
.done:
        ret

; config_load — rdi=config buffer (CONFIG_SIZE bytes), returns rax=0 ok, -1 no api key
config_load:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; r12 = config buffer

        ; zero the buffer
        xor sil, sil
        mov rdx, CONFIG_SIZE
        call mem_set

        ; copy default endpoint
        lea rdi, [r12 + CONFIG_ENDPOINT]
        lea rsi, [.default_endpoint]
        call str_copy

        ; copy default model
        lea rdi, [r12 + CONFIG_MODEL]
        lea rsi, [.default_model]
        call str_copy

        ; set default max_turns=200, max_messages=40
        mov qword [r12 + CONFIG_MAX_TURNS], 200
        mov qword [r12 + CONFIG_MAX_MSGS], 40

        ; build paths from $HOME
        lea rdi, [.env_home]
        call env_get
        test rax, rax
        jz .skip_paths

        ; skills dir: $HOME/.bareclaw/skills
        mov r13, rax                ; r13 = home dir
        lea rdi, [r12 + CONFIG_SKILLS]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + CONFIG_SKILLS]
        call str_len
        lea rdi, [r12 + CONFIG_SKILLS + rax]
        lea rsi, [.suffix_skills]
        call str_copy

        ; log dir: $HOME/.bareclaw/logs
        lea rdi, [r12 + CONFIG_LOGDIR]
        mov rsi, r13
        call str_copy
        lea rdi, [r12 + CONFIG_LOGDIR]
        call str_len
        lea rdi, [r12 + CONFIG_LOGDIR + rax]
        lea rsi, [.suffix_logs]
        call str_copy

        ; try to open config file: $HOME/.bareclaw/config
        ; build path in arena temp space
        mov rdi, 512
        call arena_alloc
        mov r14, rax                ; r14 = path buffer
        mov rdi, r14
        mov rsi, r13
        call str_copy
        mov rdi, r14
        call str_len
        lea rdi, [r14 + rax]
        lea rsi, [.suffix_config]
        call str_copy

        ; open(path, O_RDONLY)
        mov rdi, r14
        xor esi, esi                ; O_RDONLY
        xor edx, edx
        call sys_open
        cmp rax, 0
        jl .skip_paths

        ; read file into arena buffer
        mov r14d, eax               ; r14 = fd
        mov rdi, 8192
        call arena_alloc
        mov r15, rax                ; r15 = file buffer
        mov edi, r14d
        mov rsi, r15
        mov edx, 8191
        call sys_read
        cmp rax, 0
        jle .close_file
        mov byte [r15 + rax], 0     ; null-terminate

        ; parse lines
        mov rbx, r15                ; rbx = current position
.parse_line:
        cmp byte [rbx], 0
        je .close_file

        ; skip leading whitespace
.skip_ws:
        cmp byte [rbx], ' '
        je .skip_ws_next
        cmp byte [rbx], 9          ; tab
        je .skip_ws_next
        jmp .check_line
.skip_ws_next:
        inc rbx
        jmp .skip_ws

.check_line:
        ; skip comments and empty lines
        cmp byte [rbx], '#'
        je .skip_to_eol
        cmp byte [rbx], 10
        je .next_line
        cmp byte [rbx], 0
        je .close_file

        ; find '='
        mov r13, rbx                ; r13 = line start (key start)
.find_eq:
        cmp byte [rbx], '='
        je .found_eq
        cmp byte [rbx], 10
        je .next_line
        cmp byte [rbx], 0
        je .close_file
        inc rbx
        jmp .find_eq

.found_eq:
        ; null-terminate key at '=' position, then trim trailing whitespace
        mov byte [rbx], 0          ; overwrite '=' with null
        mov rdi, rbx
        dec rdi
.trim_key:
        cmp rdi, r13
        jl .key_done
        cmp byte [rdi], ' '
        je .trim_key_ws
        cmp byte [rdi], 9
        je .trim_key_ws
        jmp .key_done
.trim_key_ws:
        mov byte [rdi], 0
        dec rdi
        jmp .trim_key
.key_done:
        inc rbx                     ; advance past null (was '=')

        ; skip whitespace after =
.skip_val_ws:
        cmp byte [rbx], ' '
        je .skip_val_ws_next
        cmp byte [rbx], 9
        je .skip_val_ws_next
        jmp .val_start
.skip_val_ws_next:
        inc rbx
        jmp .skip_val_ws

.val_start:
        ; strip leading quote
        cmp byte [rbx], '"'
        je .strip_lquote
        cmp byte [rbx], 0x27       ; single quote
        je .strip_lquote
        jmp .find_val_end
.strip_lquote:
        inc rbx

.find_val_end:
        mov r15, rbx                ; r15 = value start (reuse, file buf no longer needed for scanning)
        ; actually we still need file buf pointer — let's just find end
        mov rdi, rbx
.find_eol:
        cmp byte [rdi], 10
        je .eol_found
        cmp byte [rdi], 0
        je .eol_found
        inc rdi
        jmp .find_eol
.eol_found:
        ; rdi points at newline or null
        mov cl, byte [rdi]
        mov byte [rdi], 0          ; null-terminate value
        push rcx
        push rdi

        ; trim trailing whitespace and quotes from value
        dec rdi
.trim_val:
        cmp rdi, r15
        jl .val_trimmed
        cmp byte [rdi], ' '
        je .trim_val_char
        cmp byte [rdi], 9
        je .trim_val_char
        cmp byte [rdi], '"'
        je .trim_val_char
        cmp byte [rdi], 0x27
        je .trim_val_char
        jmp .val_trimmed
.trim_val_char:
        mov byte [rdi], 0
        dec rdi
        jmp .trim_val
.val_trimmed:

        ; match key against known keys, copy value
        ; r13 = key, r15 = value
        lea rdi, [r13]
        lea rsi, [.key_api_key]
        call str_eq
        test rax, rax
        jnz .set_api_key

        lea rdi, [r13]
        lea rsi, [.key_model]
        call str_eq
        test rax, rax
        jnz .set_model

        lea rdi, [r13]
        lea rsi, [.key_endpoint]
        call str_eq
        test rax, rax
        jnz .set_endpoint

        lea rdi, [r13]
        lea rsi, [.key_max_turns]
        call str_eq
        test rax, rax
        jnz .set_max_turns

        lea rdi, [r13]
        lea rsi, [.key_max_messages]
        call str_eq
        test rax, rax
        jnz .set_max_messages

        jmp .key_matched

.set_api_key:
        lea rdi, [r12 + CONFIG_API_KEY]
        mov rsi, r15
        call str_copy
        jmp .key_matched
.set_model:
        lea rdi, [r12 + CONFIG_MODEL]
        mov rsi, r15
        call str_copy
        jmp .key_matched
.set_endpoint:
        lea rdi, [r12 + CONFIG_ENDPOINT]
        mov rsi, r15
        call str_copy
        jmp .key_matched
.set_max_turns:
        mov rdi, r15
        call str_to_int
        mov [r12 + CONFIG_MAX_TURNS], rax
        jmp .key_matched
.set_max_messages:
        mov rdi, r15
        call str_to_int
        mov [r12 + CONFIG_MAX_MSGS], rax
        jmp .key_matched

.key_matched:
        pop rdi
        pop rcx
        ; restore newline if it was one, advance past it
        mov byte [rdi], cl
        mov rbx, rdi
        cmp cl, 10
        jne .close_file
        inc rbx
        jmp .parse_line

.skip_to_eol:
        cmp byte [rbx], 10
        je .next_line
        cmp byte [rbx], 0
        je .close_file
        inc rbx
        jmp .skip_to_eol
.next_line:
        inc rbx
        jmp .parse_line

.close_file:
        mov edi, r14d
        call sys_close

.skip_paths:
        ; env overrides
        lea rdi, [.env_api_key]
        call env_get
        test rax, rax
        jz .no_env_api_key
        lea rdi, [r12 + CONFIG_API_KEY]
        mov rsi, rax
        call str_copy
.no_env_api_key:

        lea rdi, [.env_model]
        call env_get
        test rax, rax
        jz .no_env_model
        lea rdi, [r12 + CONFIG_MODEL]
        mov rsi, rax
        call str_copy
.no_env_model:

        lea rdi, [.env_endpoint]
        call env_get
        test rax, rax
        jz .no_env_endpoint
        lea rdi, [r12 + CONFIG_ENDPOINT]
        mov rsi, rax
        call str_copy
.no_env_endpoint:

        lea rdi, [.env_max_turns]
        call env_get
        test rax, rax
        jz .no_env_max_turns
        mov rdi, rax
        call str_to_int
        mov [r12 + CONFIG_MAX_TURNS], rax
.no_env_max_turns:

        lea rdi, [.env_max_messages]
        call env_get
        test rax, rax
        jz .no_env_max_messages
        mov rdi, rax
        call str_to_int
        mov [r12 + CONFIG_MAX_MSGS], rax
.no_env_max_messages:

        ; check if api_key is set
        cmp byte [r12 + CONFIG_API_KEY], 0
        je .no_key
        xor eax, eax
        jmp .config_done
.no_key:
        mov rax, -1
.config_done:
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; string constants (in executable segment — read-only)
.default_endpoint db 'https://openrouter.ai/api/v1/chat/completions', 0
.default_model    db 'minimax/minimax-m2.5', 0
.suffix_skills    db '/.bareclaw/skills', 0
.suffix_logs      db '/.bareclaw/logs', 0
.suffix_config    db '/.bareclaw/config', 0
.env_home         db 'HOME', 0
.env_api_key      db 'BARECLAW_API_KEY', 0
.env_model        db 'BARECLAW_MODEL', 0
.env_endpoint     db 'BARECLAW_ENDPOINT', 0
.env_max_turns    db 'BARECLAW_MAX_TURNS', 0
.env_max_messages db 'BARECLAW_MAX_MESSAGES', 0
.key_api_key      db 'api_key', 0
.key_model        db 'model', 0
.key_endpoint     db 'endpoint', 0
.key_max_turns    db 'max_turns', 0
.key_max_messages db 'max_messages', 0
