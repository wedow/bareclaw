; skills.inc — load .md skill files from a directory into a prompt string
; Requires: syscalls.inc, strings.inc, arena.inc

GETDENTS_BUFSZ equ 4096

; skills_build_prompt — rdi=skills directory path
; Copies base prompt into arena, opens dir, reads .md files, appends each.
; Returns rax=pointer to null-terminated prompt in arena.
; Uses arena_pos as a write cursor — appends bytes contiguously.
skills_build_prompt:
        push rbx
        push r12
        push r13
        push r14
        push r15
        push rbp
        sub rsp, GETDENTS_BUFSZ + 512  ; getdents buf + path buf on stack
        mov r12, rdi                ; r12 = skills dir path

        ; r13 = prompt start (current arena_pos, already aligned from prior alloc)
        mov r13, [arena_pos]

        ; copy base prompt into arena
        lea rdi, [.base_prompt]
        call str_len
        mov r14, rax                ; r14 = base prompt length
        mov rdi, r13
        lea rsi, [.base_prompt]
        mov rdx, r14
        call mem_copy
        lea rax, [r13 + r14]
        mov [arena_pos], rax        ; advance cursor past base prompt

        ; open directory: sys_open(path, O_RDONLY|O_DIRECTORY, 0)
        mov rdi, r12
        mov esi, 0x10000            ; O_DIRECTORY
        xor edx, edx
        call sys_open
        cmp rax, 0
        jl .no_dir
        mov ebx, eax                ; ebx = dir fd

        ; r15 = getdents buffer (on stack)
        lea r15, [rsp]

.getdents_loop:
        mov edi, ebx
        mov rsi, r15
        mov edx, GETDENTS_BUFSZ
        call sys_getdents64
        cmp rax, 0
        jle .close_dir
        mov rbp, rax                ; rbp = bytes returned

        xor r14d, r14d              ; r14 = offset into getdents buffer
.entry_loop:
        cmp r14, rbp
        jge .getdents_loop

        ; current entry at r15 + r14
        movzx ecx, word [r15 + r14 + 16]  ; d_reclen
        lea rdi, [r15 + r14 + 19]         ; d_name

        ; save iteration state
        push rcx                    ; reclen
        push r14                    ; offset

        ; check if filename ends with ".md"
        call str_len                ; rax = len, rdi preserved
        cmp rax, 3
        jl .skip_entry
        cmp byte [rdi + rax - 3], '.'
        jne .skip_entry
        cmp byte [rdi + rax - 2], 'm'
        jne .skip_entry
        cmp byte [rdi + rax - 1], 'd'
        jne .skip_entry

        ; rdi = filename pointer — save it
        mov r8, rdi

        ; build full path in stack buffer at rsp+16 (past saved rcx/r14)
        ; actually rsp changed due to pushes — use explicit offset
        ; path buffer is at original rsp + GETDENTS_BUFSZ, but we pushed 2 values
        ; let's compute: rsp now = original_rsp - 16
        ; getdents buf = rsp+16, path buf = rsp+16+GETDENTS_BUFSZ
        lea rdi, [rsp + 16 + GETDENTS_BUFSZ]
        mov rsi, r12
        call str_copy
        lea rdi, [rsp + 16 + GETDENTS_BUFSZ]
        call str_len
        lea rdi, [rsp + 16 + GETDENTS_BUFSZ + rax]
        mov byte [rdi], '/'
        inc rdi
        mov rsi, r8
        call str_copy

        ; append skill header to arena: "\n--- SKILL: filename ---\n"
        call .append_skill_prefix
        mov rdi, [arena_pos]
        mov rsi, r8
        call str_copy
        lea rdi, [rdi]
        call str_len
        add [arena_pos], rax
        call .append_skill_suffix

        ; open the .md file
        lea rdi, [rsp + 16 + GETDENTS_BUFSZ]
        xor esi, esi
        xor edx, edx
        call sys_open
        cmp rax, 0
        jl .next_entry
        mov r9d, eax

        ; read file into arena
.read_skill:
        mov edi, r9d
        mov rsi, [arena_pos]
        mov rdx, [arena_end]
        sub rdx, rsi
        test rdx, rdx
        jz .close_skill
        call sys_read
        cmp rax, 0
        jle .close_skill
        add [arena_pos], rax
        jmp .read_skill

.close_skill:
        mov edi, r9d
        call sys_close
        jmp .next_entry

.skip_entry:
.next_entry:
        pop r14
        pop rcx
        add r14, rcx
        jmp .entry_loop

.close_dir:
        mov edi, ebx
        call sys_close

.no_dir:
        ; null-terminate the prompt
        mov rax, [arena_pos]
        mov byte [rax], 0
        inc qword [arena_pos]

        mov rax, r13
        add rsp, GETDENTS_BUFSZ + 512
        pop rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; internal: append "\n--- SKILL: " to arena
.append_skill_prefix:
        mov rdi, [arena_pos]
        lea rsi, [.skill_prefix]
.ap_copy:
        lodsb
        test al, al
        jz .ap_done
        stosb
        jmp .ap_copy
.ap_done:
        mov [arena_pos], rdi
        ret

; internal: append " ---\n" to arena
.append_skill_suffix:
        mov rdi, [arena_pos]
        lea rsi, [.skill_suffix]
.as_copy:
        lodsb
        test al, al
        jz .as_done
        stosb
        jmp .as_copy
.as_done:
        mov [arena_pos], rdi
        ret

.base_prompt:
        db 'You are BareClaw, a minimal agentic assistant.', 10
        db 'You have one tool: shell. Use it to run any command.', 10
        db 'For files, use cat, tee, sed, etc. Be concise. Just do it.', 10, 10, 0

.skill_prefix db 10, '--- SKILL: ', 0
.skill_suffix db ' ---', 10, 0
