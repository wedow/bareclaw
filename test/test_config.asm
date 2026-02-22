format ELF64 executable 3

include '../src/strings.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/test_macros.inc'
include '../src/arena.inc'
include '../src/config.inc'

entry _start
_start:
        call arena_init

        ; === Test 1: config_init with api key ===
        test_begin 'config_init: api_key from env'
        lea rdi, [fake_envp_with_key]
        call env_init
        call config_init
        assert_eq rax, 0

        ; verify api_key points to "test-key"
        test_begin 'config_init: api_key value'
        mov rax, [config_api_key]
        mov rdi, rax
        lea rsi, [.expect_key]
        call str_eq
        assert_eq rax, 1

        ; === Test 2: default model ===
        test_begin 'config_init: default model'
        mov rax, [config_model]
        mov rdi, rax
        lea rsi, [.expect_model]
        call str_eq
        assert_eq rax, 1

        ; === Test 3: sessions_dir built from HOME ===
        test_begin 'config_init: sessions_dir from HOME'
        mov rax, [config_sessions_dir]
        mov rdi, rax
        lea rsi, [.expect_sessions]
        call str_eq
        assert_eq rax, 1

        ; === Test 4: no api_key returns -1 ===
        test_begin 'config_init: no api_key returns -1'
        ; reset globals
        mov qword [config_api_key], 0
        mov qword [config_model], 0
        mov qword [config_endpoint], 0
        mov qword [config_skills_dir], 0
        mov qword [config_sessions_dir], 0
        lea rdi, [fake_envp_no_key]
        call env_init
        call config_init
        assert_eq rax, -1

        call arena_destroy
        tests_done

.expect_key      db 'test-key', 0
.expect_model    db 'anthropic/claude-haiku-4.5', 0
.expect_sessions db '/tmp/.bareclaw/sessions', 0

segment readable writeable

; fake envp with api key and HOME
align 8
fake_envp_with_key:
        dq .env1, .env2, 0
.env1 db 'BARECLAW_API_KEY=test-key', 0
.env2 db 'HOME=/tmp', 0

; fake envp without api key
align 8
fake_envp_no_key:
        dq .env3, 0
.env3 db 'HOME=/tmp', 0

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

envp_ptr   dq 0

config_api_key      dq 0
config_model        dq 0
config_endpoint     dq 0
config_skills_dir   dq 0
config_sessions_dir dq 0
