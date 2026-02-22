format ELF64 executable 3

include 'strings.inc'

segment readable executable

include 'syscalls.inc'
include 'arena.inc'
include 'run_capture.inc'
include 'config.inc'
include 'session.inc'
include 'json.inc'
include 'agent.inc'

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

envp_ptr   dq 0

config_api_key      dq 0
config_model        dq 0
config_endpoint     dq 0
config_skills_dir   dq 0
config_sessions_dir dq 0

msg_list_ptr   dq 0
msg_list_count dq 0

json_resp_buf rb RESP_SIZE
json_tc_buf   rb TC_SIZE

tc_cursor       dq 0
retry_count     dq 0
retry_backoff   dq 0
retry_digit_buf rb 4
timespec_buf    dq 0, 0
