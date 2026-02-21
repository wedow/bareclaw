; log.inc — session logging (open, write, close)
; Requires: syscalls.inc, strings.inc

; byte_to_hex — dil=byte value, rsi=output buffer (2 bytes)
; Writes 2 lowercase hex chars
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

; int_to_str — rdi=64-bit unsigned int, rsi=output buffer (at least 20 bytes)
; Writes decimal string, null-terminated
; Returns rax=pointer to start of number in buffer
int_to_str:
        lea rcx, [rsi + 19]            ; point to end of buffer
        mov byte [rcx], 0              ; null-terminate
        mov rax, rdi
        test rax, rax
        jnz .divide
        dec rcx
        mov byte [rcx], '0'
        mov rax, rcx
        ret
.divide:
        test rax, rax
        jz .done
        xor edx, edx
        mov r8, 10
        div r8
        add dl, '0'
        dec rcx
        mov byte [rcx], dl
        jmp .divide
.done:
        mov rax, rcx
        ret

; log_gen_session_id — rdi=buffer (at least 17 bytes)
; Uses getrandom for 8 bytes, converts to 16 hex chars + null
; Returns rax=buffer pointer
log_gen_session_id:
        push rbx
        push r12
        mov r12, rdi                    ; save buffer pointer
        ; get 8 random bytes into stack temp
        sub rsp, 8
        mov rdi, rsp
        mov esi, 8
        xor edx, edx
        call sys_getrandom
        ; convert each byte
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
        mov byte [r12 + 16], 0         ; null-terminate
        add rsp, 8
        mov rax, r12
        pop r12
        pop rbx
        ret

; log_open — rdi=log directory path, rsi=session ID string
; Creates dir, opens logdir/sessionid.txt, writes header
; Returns rax=fd (or negative on error)
log_open:
        push rbx
        push r12
        push r13
        push r14
        sub rsp, 320                    ; stack buffer for path + header
        mov r12, rdi                    ; r12 = logdir
        mov r13, rsi                    ; r13 = session id

        ; mkdir(logdir, 0755) — ignore EEXIST (-17)
        mov esi, 493                    ; 0755 octal
        call sys_mkdir
        cmp rax, -17
        je .mkdir_ok
        test rax, rax
        js .open_fail
.mkdir_ok:
        ; build path: logdir/sessionid.txt
        lea rdi, [rsp]
        mov rsi, r12
        call str_copy
        lea rdi, [rsp]
        call str_len
        lea rdi, [rsp + rax]
        mov byte [rdi], '/'
        inc rdi
        mov rsi, r13
        call str_copy
        ; append ".txt"
        mov rdi, rsp
        lea rdi, [rsp]
        call str_len
        lea rdi, [rsp + rax]
        mov dword [rdi], '.txt'
        mov byte [rdi + 4], 0

        ; open(path, O_WRONLY|O_CREAT|O_APPEND, 0644)
        lea rdi, [rsp]
        mov esi, 1089                   ; 1|64|1024
        mov edx, 420                    ; 0644 octal
        call sys_open
        cmp rax, 0
        jl .open_fail
        mov r14d, eax                   ; r14 = fd

        ; write header: "=== sessionid timestamp\n"
        ; build in stack buffer at rsp+256
        lea rdi, [rsp + 256]
        mov dword [rdi], '=== '
        add rdi, 4
        mov rsi, r13
        call str_copy
        lea rdi, [rsp + 256]
        call str_len
        lea rbx, [rsp + 256 + rax]     ; rbx = write cursor
        mov byte [rbx], ' '
        inc rbx

        ; get timestamp
        lea rdi, [timespec_buf]
        xor esi, esi                    ; CLOCK_REALTIME = 0
        xchg rdi, rsi                   ; rdi=clock_id, rsi=timespec
        call sys_clock_gettime
        mov rdi, [timespec_buf]         ; seconds
        lea rsi, [rsp + 200]            ; temp buffer for int_to_str
        call int_to_str
        ; rax = pointer to start of number
        mov rsi, rax
        mov rdi, rbx
        call str_copy
        mov rdi, rbx
        call str_len
        add rbx, rax
        mov byte [rbx], 10             ; newline
        inc rbx

        ; write header
        mov edi, r14d
        lea rsi, [rsp + 256]
        mov rdx, rbx
        lea rax, [rsp + 256]
        sub rdx, rax
        call sys_write

        mov eax, r14d
        add rsp, 320
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.open_fail:
        add rsp, 320
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; log_write — rdi=fd, rsi=role string, rdx=content string
; Writes "[timestamp] role: content\n"
log_write:
        push rbx
        push r12
        push r13
        push r14
        sub rsp, 512                    ; buffer for formatted line
        mov r12d, edi                   ; fd
        mov r13, rsi                    ; role
        mov r14, rdx                    ; content

        ; get timestamp
        lea rsi, [timespec_buf]
        xor edi, edi                    ; CLOCK_REALTIME
        call sys_clock_gettime
        mov rdi, [timespec_buf]
        lea rsi, [rsp + 480]            ; temp for int_to_str
        call int_to_str
        mov r8, rax                     ; r8 = timestamp string

        ; build line: "[timestamp] role: content\n"
        lea rbx, [rsp]
        mov byte [rbx], '['
        inc rbx
        mov rsi, r8
        mov rdi, rbx
        call str_copy
        mov rdi, rbx
        call str_len
        add rbx, rax
        mov word [rbx], '] '
        add rbx, 2
        mov rsi, r13
        mov rdi, rbx
        call str_copy
        mov rdi, rbx
        call str_len
        add rbx, rax
        mov word [rbx], ': '
        add rbx, 2
        mov rsi, r14
        mov rdi, rbx
        call str_copy
        mov rdi, rbx
        call str_len
        add rbx, rax
        mov byte [rbx], 10
        inc rbx

        ; write
        mov edi, r12d
        lea rsi, [rsp]
        mov rdx, rbx
        sub rdx, rsp
        call sys_write

        add rsp, 512
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; log_close — rdi=fd
log_close:
        call sys_close
        ret
