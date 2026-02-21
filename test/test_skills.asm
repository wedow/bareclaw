format ELF64 executable 3

include '../src/test_macros.inc'

segment readable executable

include '../src/syscalls.inc'
include '../src/strings.inc'
include '../src/arena.inc'
include '../src/skills.inc'

SYS_UNLINK equ 87
SYS_RMDIR  equ 84

entry $

        call arena_init

        ; --- setup: create test directory and files ---
        ; mkdir /tmp/szc_test_skills (ignore EEXIST = -17)
        lea rdi, [test_dir]
        mov esi, 493                ; 0755
        call sys_mkdir

        ; write test1.md
        lea rdi, [path_test1]
        mov esi, 0x241              ; O_WRONLY|O_CREAT|O_TRUNC
        mov edx, 420                ; 0644
        call sys_open
        mov ebx, eax
        mov edi, ebx
        lea rsi, [content_test1]
        mov edx, content_test1_len
        call sys_write
        mov edi, ebx
        call sys_close

        ; write test2.md
        lea rdi, [path_test2]
        mov esi, 0x241
        mov edx, 420
        call sys_open
        mov ebx, eax
        mov edi, ebx
        lea rsi, [content_test2]
        mov edx, content_test2_len
        call sys_write
        mov edi, ebx
        call sys_close

        ; write readme.txt (should be ignored)
        lea rdi, [path_readme]
        mov esi, 0x241
        mov edx, 420
        call sys_open
        mov ebx, eax
        mov edi, ebx
        lea rsi, [content_readme]
        mov edx, content_readme_len
        call sys_write
        mov edi, ebx
        call sys_close

        ; --- Test 1: call skills_build_prompt ---
        test_begin "skills_build_prompt returns non-null"
        lea rdi, [test_dir]
        call skills_build_prompt
        mov r12, rax                ; r12 = prompt pointer
        test rax, rax
        jz .fail_1
        test_pass
        jmp .test_2
.fail_1:
        test_fail
.test_2:

        ; --- Test 2: starts with "You are BareClaw" ---
        test_begin "prompt starts with base prompt"
        mov rdi, r12
        lea rsi, [s_starts_with]
        call str_starts_with
        assert_eq rax, 1

        ; --- Test 3: contains "--- SKILL:" ---
        test_begin "prompt contains '--- SKILL:'"
        mov rdi, r12
        lea rsi, [s_skill_hdr]
        call str_contains
        assert_eq rax, 1

        ; --- Test 4: contains skill content ---
        test_begin "prompt contains skill content"
        mov rdi, r12
        lea rsi, [s_skill_one]
        call str_contains
        mov rbx, rax
        mov rdi, r12
        lea rsi, [s_skill_two]
        call str_contains
        or rax, rbx                ; at least one must be found
        assert_eq rax, 1

        ; --- Test 5: does NOT contain .txt content ---
        test_begin "prompt does not contain txt content"
        mov rdi, r12
        lea rsi, [s_not_skill]
        call str_contains
        assert_eq rax, 0

        ; --- cleanup ---
        call arena_destroy

        ; unlink files
        lea rdi, [path_test1]
        mov eax, SYS_UNLINK
        syscall
        lea rdi, [path_test2]
        mov eax, SYS_UNLINK
        syscall
        lea rdi, [path_readme]
        mov eax, SYS_UNLINK
        syscall
        ; rmdir
        lea rdi, [test_dir]
        mov eax, SYS_RMDIR
        syscall

        tests_done

; str_contains — rdi=haystack, rsi=needle, returns rax=1 if found, 0 if not
; Simple O(n*m) substring search
str_contains:
        push rbx
        push r12
        push r13
        mov r12, rdi                ; r12 = haystack
        mov r13, rsi                ; r13 = needle
        ; get needle length
        mov rdi, r13
        call str_len
        test rax, rax
        jz .sc_found                ; empty needle always matches
        mov rbx, rax                ; rbx = needle length

        mov rdi, r12                ; rdi = current haystack pos
.sc_outer:
        cmp byte [rdi], 0
        je .sc_not_found
        ; compare needle at current position
        xor rcx, rcx
.sc_inner:
        cmp rcx, rbx
        je .sc_found                ; matched all needle chars
        movzx eax, byte [rdi + rcx]
        cmp al, byte [r13 + rcx]
        jne .sc_advance
        inc rcx
        jmp .sc_inner
.sc_advance:
        inc rdi
        jmp .sc_outer
.sc_found:
        mov eax, 1
        pop r13
        pop r12
        pop rbx
        ret
.sc_not_found:
        xor eax, eax
        pop r13
        pop r12
        pop rbx
        ret

segment readable writeable

arena_base dq 0
arena_pos  dq 0
arena_end  dq 0

test_dir      db '/tmp/szc_test_skills', 0
path_test1    db '/tmp/szc_test_skills/test1.md', 0
path_test2    db '/tmp/szc_test_skills/test2.md', 0
path_readme   db '/tmp/szc_test_skills/readme.txt', 0

content_test1     db '# Skill One', 10, 'Do something.', 0
content_test1_len = $ - content_test1 - 1
content_test2     db '# Skill Two', 10, 'Do another thing.', 0
content_test2_len = $ - content_test2 - 1
content_readme     db 'not a skill', 0
content_readme_len = $ - content_readme - 1

s_starts_with db 'You are BareClaw', 0
s_skill_hdr   db '--- SKILL:', 0
s_skill_one   db 'Skill One', 0
s_skill_two   db 'Skill Two', 0
s_not_skill   db 'not a skill', 0
