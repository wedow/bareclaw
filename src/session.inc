; session.inc — JSONL file-based persistent sessions
; Requires: syscalls.inc, strings.inc, arena.inc, config.inc

MSG_JSON_PTR   = 0
MSG_JSON_LEN   = 8
MSG_ENTRY_SIZE = 16

; byte_to_hex — dil=byte, rsi=output (2 bytes)
byte_to_hex:
        movzx eax, dil
        mov rcx, .hex_table
        mov edx, eax
        shr edx, 4
        mov dl, byte [rcx + rdx]
        mov byte [rsi], dl
        and eax, 0x0F
        mov al, byte [rcx + rax]
        mov byte [rsi + 1], al
        ret
.hex_table db '0123456789abcdef'

; session_gen_id — rdi=buffer (17 bytes min). Returns rax=buffer.
session_gen_id:
        push rbx
        push r12
        mov r12, rdi
        sub rsp, 8
        mov rdi, rsp
        mov esi, 8
        xor edx, edx
        call sys_getrandom
        xor ebx, ebx
.hex_loop:
        cmp ebx, 8
        je .hex_done
        movzx edi, byte [rsp + rbx]
        lea rsi, [r12 + rbx*2]
        call byte_to_hex
        inc ebx
        jmp .hex_loop
.hex_done:
        mov byte [r12 + 16], 0
        add rsp, 8
        mov rax, r12
        pop r12
        pop rbx
        ret

; _session_build_path — rdi=session_id. Builds path in arena. Returns rax=path.
_session_build_path:
        push rbx
        push r12
        mov r12, rdi
        mov rdi, 512
        call arena_alloc
        mov rbx, rax
        mov rdi, rax
        mov rsi, [config_sessions_dir]
        call str_copy
        mov rdi, rbx
        call str_len
        mov byte [rbx + rax], '/'
        lea rdi, [rbx + rax + 1]
        mov rsi, r12
        call str_copy
        mov rdi, rbx
        call str_len
        lea rdi, [rbx + rax]
        mov byte [rdi], '.'
        mov byte [rdi+1], 'j'
        mov byte [rdi+2], 's'
        mov byte [rdi+3], 'o'
        mov byte [rdi+4], 'n'
        mov byte [rdi+5], 'l'
        mov byte [rdi+6], 0
        mov rax, rbx
        pop r12
        pop rbx
        ret

; session_create — rdi=session_id. Returns rax=fd.
session_create:
        push rbx
        call _session_build_path
        mov rbx, rax

        ; mkdir sessions_dir (ignore EEXIST)
        mov rdi, [config_sessions_dir]
        mov esi, 493                ; 0755
        call sys_mkdir

        ; open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644)
        mov rdi, rbx
        mov esi, 0x241              ; 1|64|512
        mov edx, 420                ; 0644
        call sys_open
        pop rbx
        ret

; session_open — rdi=session_id. Returns rax=fd or negative.
session_open:
        call _session_build_path
        mov rdi, rax
        mov esi, 0x402              ; O_RDWR|O_APPEND
        xor edx, edx
        call sys_open
        ret

; session_append — rdi=fd, rsi=json_string, rdx=json_len. Returns 0.
session_append:
        push rbx
        push r12
        push r13
        mov ebx, edi
        mov r12, rsi
        mov r13, rdx
        mov edi, ebx
        mov rsi, r12
        mov rdx, r13
        call sys_write
        mov edi, ebx
        lea rsi, [.newline]
        mov edx, 1
        call sys_write
        xor eax, eax
        pop r13
        pop r12
        pop rbx
        ret
.newline db 10

; session_load_messages — rdi=fd. Populates msg_list_ptr/msg_list_count. Returns 0.
session_load_messages:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov ebx, edi

        ; seek to beginning
        mov edi, ebx
        xor esi, esi
        xor edx, edx               ; SEEK_SET
        call sys_lseek

        ; read entire file into arena
        mov rdi, 262144
        call arena_alloc
        mov r12, rax
        mov edi, ebx
        mov rsi, r12
        mov edx, 262143
        call sys_read
        cmp rax, 0
        jle .load_empty
        mov r13, rax                ; r13 = bytes read
        mov byte [r12 + r13], 0

        ; count lines (non-empty)
        xor r14d, r14d
        xor ecx, ecx
.count_loop:
        cmp rcx, r13
        jge .count_done
        cmp byte [r12 + rcx], 10
        jne .count_next
        inc r14d
.count_next:
        inc rcx
        jmp .count_loop
.count_done:
        ; last line without trailing newline
        cmp r13, 0
        je .load_empty
        cmp byte [r12 + r13 - 1], 10
        je .check_count
        inc r14d
.check_count:
        test r14d, r14d
        jz .load_empty

        ; allocate msg_list
        mov rdi, r14
        shl rdi, 4
        call arena_alloc
        mov [msg_list_ptr], rax
        mov r15, rax

        ; fill entries
        xor ecx, ecx               ; byte offset
        xor ebx, ebx               ; entry index
.fill_loop:
        cmp ebx, r14d
        jge .fill_done
        ; skip empty lines
        cmp rcx, r13
        jge .fill_done
        cmp byte [r12 + rcx], 10
        je .skip_empty
        ; record line start
        lea rax, [r12 + rcx]
        mov rdx, rbx
        shl rdx, 4
        mov [r15 + rdx + MSG_JSON_PTR], rax
        ; find end of line
        mov r8, rcx
.find_eol:
        cmp r8, r13
        jge .eol_found
        cmp byte [r12 + r8], 10
        je .eol_found
        inc r8
        jmp .find_eol
.eol_found:
        mov rax, r8
        sub rax, rcx
        mov rdx, rbx
        shl rdx, 4
        mov [r15 + rdx + MSG_JSON_LEN], rax
        lea rcx, [r8 + 1]
        inc ebx
        jmp .fill_loop
.skip_empty:
        inc rcx
        jmp .fill_loop

.fill_done:
        mov qword [msg_list_count], rbx
        xor eax, eax
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.load_empty:
        mov qword [msg_list_ptr], 0
        mov qword [msg_list_count], 0
        xor eax, eax
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; session_compact — rdi=fd, rsi=max_messages
; Keeps first line (system) + last (max-1) lines. Rewrites file in place.
; Returns 0.
session_compact:
        push rbx
        push r12
        push r13
        push r14
        push r15
        push rbp
        mov ebx, edi
        mov r12, rsi                ; max_messages

        mov rax, [msg_list_count]
        cmp rax, r12
        jle .compact_noop

        mov r13, [msg_list_ptr]     ; list base
        mov r14, [msg_list_count]   ; total count
        mov r15, r12
        dec r15                     ; tail count = max - 1
        mov rbp, r14
        sub rbp, r15                ; tail start index

        ; ftruncate fd to 0
        mov edi, ebx
        xor esi, esi
        call sys_ftruncate

        ; lseek to 0 (reset O_APPEND effect via lseek)
        mov edi, ebx
        xor esi, esi
        xor edx, edx
        call sys_lseek

        ; write first entry (system message)
        mov rcx, [r13 + MSG_JSON_PTR]
        mov rdx, [r13 + MSG_JSON_LEN]
        mov edi, ebx
        mov rsi, rcx
        call sys_write
        mov edi, ebx
        lea rsi, [session_append.newline]
        mov edx, 1
        call sys_write

        ; write tail entries
        xor r14d, r14d              ; counter
.compact_loop:
        cmp r14, r15
        jge .compact_reload
        mov rax, rbp
        add rax, r14
        shl rax, 4
        mov rcx, [r13 + rax + MSG_JSON_PTR]
        mov rdx, [r13 + rax + MSG_JSON_LEN]
        mov edi, ebx
        mov rsi, rcx
        call sys_write
        mov edi, ebx
        lea rsi, [session_append.newline]
        mov edx, 1
        call sys_write
        inc r14
        jmp .compact_loop

.compact_reload:
        ; reload messages from rewritten file
        mov edi, ebx
        call session_load_messages

.compact_noop:
        xor eax, eax
        pop rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret
