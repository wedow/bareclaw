; http.inc — HTTP POST via fork+execve of curl, read response into arena
; Requires: syscalls.inc, strings.inc, arena.inc

HTTP_READ_CHUNK equ 4096

; http_post — rdi=URL, rsi=API key, rdx=JSON body
; Forks curl, captures stdout into arena.
; Returns rax=pointer to null-terminated response, or 0 on failure.
http_post:
        push rbx
        push r12
        push r13
        push r14
        push r15
        push rbp
        mov r12, rdi                ; r12 = URL
        mov r13, rsi                ; r13 = API key
        mov r14, rdx                ; r14 = body

        ; build auth header: "Authorization: Bearer " + api_key
        ; measure api_key length
        mov rdi, r13
        call str_len
        lea rdi, [rax + 22 + 1]    ; "Authorization: Bearer " = 22 chars + key + null
        call arena_alloc
        mov rbp, rax                ; rbp = auth header buffer
        mov rdi, rax
        lea rsi, [.auth_prefix]
        call str_copy
        lea rdi, [rbp + 22]
        mov rsi, r13
        call str_copy

        ; create pipe
        lea rdi, [pipe_fds]
        call sys_pipe
        test eax, eax
        js .fail

        ; fork
        call sys_fork
        test rax, rax
        js .fail
        jz .child

        ; --- parent ---
        mov r13, rax                ; r13 = child pid

        ; close write end
        mov edi, [pipe_fds + 4]
        call sys_close

        ; record arena pos as output start
        mov r15, [arena_pos]        ; r15 = output buffer start

        ; read loop
        mov ebx, [pipe_fds]         ; ebx = read fd
.read_loop:
        mov edi, ebx
        mov rsi, [arena_pos]
        mov rdx, [arena_end]
        sub rdx, rsi
        cmp rdx, HTTP_READ_CHUNK
        jbe .use_remaining
        mov rdx, HTTP_READ_CHUNK
.use_remaining:
        test rdx, rdx
        jz .read_done
        call sys_read
        cmp rax, 0
        jle .read_done
        add [arena_pos], rax
        jmp .read_loop

.read_done:
        ; null-terminate
        mov rax, [arena_pos]
        mov byte [rax], 0
        inc qword [arena_pos]

        ; close read end
        mov edi, ebx
        call sys_close

        ; wait for child
        mov rdi, r13
        lea rsi, [wait_status]
        xor edx, edx
        xor r10d, r10d
        call sys_wait4

        mov rax, r15                ; return output pointer
        pop rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.fail:
        xor eax, eax
        pop rbp
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

        ; --- child process ---
.child:
        ; close read end
        mov edi, [pipe_fds]
        call sys_close

        ; dup2(write_fd, stdout)
        mov edi, [pipe_fds + 4]
        mov esi, 1
        call sys_dup2

        ; dup2(write_fd, stderr)
        mov edi, [pipe_fds + 4]
        mov esi, 2
        call sys_dup2

        ; close write end
        mov edi, [pipe_fds + 4]
        call sys_close

        ; build argv on stack (push in reverse, 12 entries)
        push 0                      ; argv[11] = NULL
        push r12                    ; argv[10] = URL
        push r14                    ; argv[9]  = body
        lea rax, [.arg_d]
        push rax                    ; argv[8]  = "-d"
        push rbp                    ; argv[7]  = auth header
        lea rax, [.arg_H]
        push rax                    ; argv[6]  = "-H"
        lea rax, [.content_type]
        push rax                    ; argv[5]  = "Content-Type: application/json"
        lea rax, [.arg_H]
        push rax                    ; argv[4]  = "-H"
        lea rax, [.arg_timeout]
        push rax                    ; argv[3]  = "120"
        lea rax, [.arg_m]
        push rax                    ; argv[2]  = "-m"
        lea rax, [.arg_s]
        push rax                    ; argv[1]  = "-s"
        lea rax, [.curl_path]
        push rax                    ; argv[0]  = "curl"

        ; execve("/usr/bin/curl", argv, NULL)
        lea rdi, [.curl_path]
        mov rsi, rsp
        xor edx, edx               ; envp = NULL
        call sys_execve

        ; execve failed — exit 127
        mov edi, 127
        call sys_exit_group

.curl_path    db '/usr/bin/curl', 0
.arg_s        db '-sS', 0
.arg_m        db '-m', 0
.arg_timeout  db '120', 0
.arg_H        db '-H', 0
.content_type db 'Content-Type: application/json', 0
.arg_d        db '-d', 0
.auth_prefix  db 'Authorization: Bearer ', 0
