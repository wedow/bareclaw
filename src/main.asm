format ELF64 executable 3

include 'strings.inc'

segment readable executable

include 'syscalls.inc'
include 'arena.inc'
include 'run_capture.inc'

entry _start
_start:
        xor edi, edi
        mov eax, 231
        syscall

segment readable writeable

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

pipe_fds    dd 0, 0
wait_status dd 0
