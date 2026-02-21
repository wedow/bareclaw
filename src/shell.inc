; shell.inc — execute shell commands via fork+pipe+execve, read output into arena
; Requires: syscalls.inc, arena.inc

SHELL_READ_CHUNK equ 4096

; shell_exec — rdi=command string (null-terminated)
; Forks, executes /bin/sh -c <command>, captures stdout+stderr into arena.
; Returns rax=pointer to null-terminated output in arena.
shell_exec:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; r12 = command string

        ; create pipe — int[2] at pipe_fds (4 bytes each)
        lea rdi, [pipe_fds]
        call sys_pipe
        test eax, eax
        js .exec_fail

        ; fork
        call sys_fork
        test rax, rax
        js .exec_fail
        jz .child

        ; --- parent ---
        mov r13, rax                ; r13 = child pid

        ; close write end
        mov edi, [pipe_fds + 4]
        call sys_close

        ; record arena pos as output start
        mov r14, [arena_pos]        ; r14 = output buffer start

        ; read loop
        mov r15d, [pipe_fds]        ; r15d = read fd
.read_loop:
        mov edi, r15d
        mov rsi, [arena_pos]
        ; compute remaining space
        mov rdx, [arena_end]
        sub rdx, rsi
        cmp rdx, SHELL_READ_CHUNK
        jbe .use_remaining
        mov rdx, SHELL_READ_CHUNK
.use_remaining:
        test rdx, rdx
        jz .read_done               ; arena full
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
        mov edi, r15d
        call sys_close

        ; wait for child
        mov rdi, r13                ; pid
        lea rsi, [wait_status]      ; &status
        xor edx, edx               ; options = 0
        xor r10d, r10d             ; rusage = NULL
        call sys_wait4

        mov rax, r14                ; return output pointer
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.exec_fail:
        xor eax, eax
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

        ; dup2(write_fd, 1) — stdout
        mov edi, [pipe_fds + 4]
        mov esi, 1
        call sys_dup2

        ; dup2(write_fd, 2) — stderr
        mov edi, [pipe_fds + 4]
        mov esi, 2
        call sys_dup2

        ; close write end
        mov edi, [pipe_fds + 4]
        call sys_close

        ; set up argv: ["/bin/sh", "-c", command, 0]
        ; use stack for argv array
        push 0                      ; argv[3] = NULL
        push r12                    ; argv[2] = command
        lea rax, [.arg_c]
        push rax                    ; argv[1] = "-c"
        lea rax, [.bin_sh]
        push rax                    ; argv[0] = "/bin/sh"

        ; execve("/bin/sh", argv, NULL)
        lea rdi, [.bin_sh]
        mov rsi, rsp                ; argv
        xor edx, edx               ; envp = NULL
        call sys_execve

        ; execve failed — exit 127
        mov edi, 127
        call sys_exit_group

.bin_sh db '/bin/sh', 0
.arg_c  db '-c', 0
