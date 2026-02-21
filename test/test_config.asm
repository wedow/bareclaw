format ELF64 executable 3

include '../src/test_macros.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/strings.inc'
include '../src/arena.inc'
include '../src/config.inc'

entry $

; --- str_to_int tests ---
        test_begin "str_to_int('12345') = 12345"
        lea rdi, [s_12345]
        call str_to_int
        assert_eq rax, 12345

        test_begin "str_to_int('0') = 0"
        lea rdi, [s_zero]
        call str_to_int
        assert_eq rax, 0

        test_begin "str_to_int('200') = 200"
        lea rdi, [s_200]
        call str_to_int
        assert_eq rax, 200

; --- env_get tests with fake envp ---
        test_begin "env_get('HOME') = '/tmp'"
        lea rdi, [fake_envp]
        call env_init
        lea rdi, [s_HOME]
        call env_get
        ; rax should point to "/tmp"
        mov rbx, rax
        lea rdi, [rbx]
        lea rsi, [s_tmp]
        call str_eq
        assert_eq rax, 1

        test_begin "env_get('FOO') = 'bar'"
        lea rdi, [s_FOO]
        call env_get
        mov rbx, rax
        lea rdi, [rbx]
        lea rsi, [s_bar]
        call str_eq
        assert_eq rax, 1

        test_begin "env_get('NOPE') = 0"
        lea rdi, [s_NOPE]
        call env_get
        assert_eq rax, 0

; --- config_load test ---
        test_begin "config_load: sets api_key from env, default model, returns 0"
        ; set up envp with API key and HOME
        lea rdi, [test_envp]
        call env_init
        ; init arena
        call arena_init
        ; allocate config buffer
        mov rdi, CONFIG_SIZE
        call arena_alloc
        mov rbx, rax               ; rbx = config buffer
        ; call config_load
        mov rdi, rbx
        call config_load
        ; should return 0 (has api key)
        assert_eq rax, 0

        test_begin "config_load: api_key = 'test-key'"
        lea rdi, [rbx + CONFIG_API_KEY]
        lea rsi, [s_test_key]
        call str_eq
        assert_eq rax, 1

        test_begin "config_load: model = default"
        lea rdi, [rbx + CONFIG_MODEL]
        lea rsi, [s_default_model]
        call str_eq
        assert_eq rax, 1

        test_begin "config_load: max_turns = 200 (default)"
        mov rax, [rbx + CONFIG_MAX_TURNS]
        assert_eq rax, 200

        test_begin "config_load: max_messages = 40 (default)"
        mov rax, [rbx + CONFIG_MAX_MSGS]
        assert_eq rax, 40

        test_begin "config_load: skills dir = '/tmp/.bareclaw/skills'"
        lea rdi, [rbx + CONFIG_SKILLS]
        lea rsi, [s_skills_path]
        call str_eq
        assert_eq rax, 1

        test_begin "config_load: log dir = '/tmp/.bareclaw/logs'"
        lea rdi, [rbx + CONFIG_LOGDIR]
        lea rsi, [s_logs_path]
        call str_eq
        assert_eq rax, 1

        ; clean up
        call arena_destroy

; --- config_load: no api key returns -1 ---
        test_begin "config_load: no api key returns -1"
        lea rdi, [nokey_envp]
        call env_init
        call arena_init
        mov rdi, CONFIG_SIZE
        call arena_alloc
        mov rbx, rax
        mov rdi, rbx
        call config_load
        assert_eq rax, -1

        call arena_destroy

        tests_done

segment readable writeable

; str_to_int test data
s_12345 db '12345', 0
s_zero  db '0', 0
s_200   db '200', 0

; env_get test data
s_HOME db 'HOME', 0
s_FOO  db 'FOO', 0
s_NOPE db 'NOPE', 0
s_tmp  db '/tmp', 0
s_bar  db 'bar', 0

; fake envp for env_get tests: HOME=/tmp, FOO=bar, null
fake_envp:
        dq env0, env1, 0
env0 db 'HOME=/tmp', 0
env1 db 'FOO=bar', 0

; config_load test envp: has API key and HOME
test_envp:
        dq tenv0, tenv1, 0
tenv0 db 'BARECLAW_API_KEY=test-key', 0
tenv1 db 'HOME=/tmp', 0

; config_load no-key envp: just HOME
nokey_envp:
        dq nenv0, 0
nenv0 db 'HOME=/tmp', 0

; expected values
s_test_key      db 'test-key', 0
s_default_model db 'minimax/minimax-m2.5', 0
s_skills_path   db '/tmp/.bareclaw/skills', 0
s_logs_path     db '/tmp/.bareclaw/logs', 0

; arena and config data
arena_base dq 0
arena_pos  dq 0
arena_end  dq 0
envp_ptr   dq 0
