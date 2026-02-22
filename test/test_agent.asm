format ELF64 executable 3

include '../src/strings.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/test_macros.inc'
include '../src/arena.inc'
include '../src/run_capture.inc'
include '../src/config.inc'
include '../src/session.inc'
include '../src/json.inc'
include '../src/agent.inc'

entry _start
_start:
        call arena_init

        ; set up fake env
        lea rdi, [fake_envp]
        call env_init
        call config_init

        ; === Test 1: shell_exec ===
        test_begin 'shell_exec: echo hello'
        lea rdi, [.cmd_echo]
        call shell_exec
        mov rdi, rax
        lea rsi, [.expect_hello]
        call str_starts_with
        assert_eq rax, 1

        ; === Test 2: build_system_prompt with test skills dir ===
        test_begin 'build_system_prompt: contains skill name'
        ; create test skills dir and a skill file
        lea rdi, [.bin_sh]
        lea rsi, [.argv_mkdir]
        call run_capture
        lea rdi, [.bin_sh]
        lea rsi, [.argv_mkskill]
        call run_capture

        ; set skills dir
        lea rax, [test_skills_dir]
        mov [config_skills_dir], rax
        call build_system_prompt
        ; verify prompt contains "testskill"
        mov rdi, rax
        lea rsi, [.expect_skill]
        call .str_contains
        assert_eq rax, 1

        ; === Test 3: http_post returns non-null (even with bad endpoint) ===
        test_begin 'http_post: returns non-null'
        lea rax, [.bad_endpoint]
        mov [config_endpoint], rax
        lea rax, [.fake_key]
        mov [config_api_key], rax
        lea rdi, [.fake_body]
        call http_post
        test rax, rax
        jnz .http_ok
        test_fail
.http_ok:
        test_pass

        ; cleanup
        lea rdi, [.bin_sh]
        lea rsi, [.argv_cleanup]
        call run_capture

        call arena_destroy
        tests_done

; str_contains — rdi=haystack, rsi=needle. Returns rax=1 if found, 0 if not.
.str_contains:
        push rbx
        push r12
        push r13
        mov r12, rdi
        mov r13, rsi
        mov rdi, r13
        call str_len
        mov rbx, rax               ; needle len
        test rbx, rbx
        jz .sc_yes
.sc_loop:
        movzx eax, byte [r12]
        test al, al
        jz .sc_no
        mov rdi, r12
        mov rsi, r13
        call str_starts_with
        test rax, rax
        jnz .sc_yes
        inc r12
        jmp .sc_loop
.sc_yes:
        mov rax, 1
        pop r13
        pop r12
        pop rbx
        ret
.sc_no:
        xor eax, eax
        pop r13
        pop r12
        pop rbx
        ret

; --- data ---
.cmd_echo     db 'echo hello', 0
.expect_hello db 'hello', 0
.expect_skill db 'testskill', 0
.bad_endpoint db 'http://127.0.0.1:1', 0
.fake_key     db 'fake-key', 0
.fake_body    db '{}', 0

.bin_sh db '/bin/sh', 0
.arg_c  db '-c', 0
.mkdir_cmd   db 'mkdir -p /tmp/szc_test_skills/testskill', 0
.mkskill_cmd db 'printf "%s\n" "---" "name: testskill" "description: a test skill" "---" "Body" > /tmp/szc_test_skills/testskill/SKILL.md', 0
.cleanup_cmd db 'rm -rf /tmp/szc_test_skills', 0

align 8
.argv_mkdir    dq .bin_sh, .arg_c, .mkdir_cmd, 0
.argv_mkskill  dq .bin_sh, .arg_c, .mkskill_cmd, 0
.argv_cleanup  dq .bin_sh, .arg_c, .cleanup_cmd, 0

segment readable writeable

test_skills_dir db '/tmp/szc_test_skills', 0

align 8
fake_envp:
        dq .env1, .env2, 0
.env1 db 'BARECLAW_API_KEY=test-key', 0
.env2 db 'HOME=/tmp', 0

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
