; run_capture.inc — fork+pipe+exec, read output into arena
; Requires: syscalls.inc, arena.inc

RUN_READ_CHUNK equ 4096

; run_capture — fork+pipe+execve, capture stdout+stderr into arena
; Input: rdi = path to executable (e.g., "/bin/sh")
;        rsi = pointer to null-terminated argv array
; Output: rax = pointer to captured output (null-terminated, in arena)
;         returns 0 on fork/pipe failure
run_capture:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov r12, rdi                ; r12 = executable path
        mov r13, rsi                ; r13 = argv

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
        mov r14, rax                ; r14 = child pid

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
        cmp rdx, RUN_READ_CHUNK
        jbe .use_remaining
        mov rdx, RUN_READ_CHUNK
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
        mov rdi, r14
        lea rsi, [wait_status]
        xor edx, edx
        xor r10d, r10d
        call sys_wait4

        mov rax, r15                ; return output pointer
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

.fail:
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

        ; execve(path, argv, envp)
        mov rdi, r12
        mov rsi, r13
        mov rdx, [envp_ptr]
        call sys_execve

        ; execve failed — exit 127
        mov edi, 127
        call sys_exit_group
