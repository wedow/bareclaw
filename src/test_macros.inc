; test_macros.inc — minimal TDD harness for FASM x86-64 Linux
; Uses raw syscalls only: write(1), exit_group(231)

macro test_begin name {
local .msg, .msg_end
        jmp .msg_end
  .msg db '[ TEST ] ', `name, 10
  .msg_end:
        mov eax, 1
        mov edi, 1
        lea rsi, [.msg]
        mov edx, .msg_end - .msg
        syscall
}

macro test_pass {
local .msg, .msg_end
        jmp .msg_end
  .msg db '[  OK  ] PASS', 10
  .msg_end:
        mov eax, 1
        mov edi, 1
        lea rsi, [.msg]
        mov edx, .msg_end - .msg
        syscall
}

macro test_fail {
local .msg, .msg_end
        jmp .msg_end
  .msg db '[ FAIL ]', 10
  .msg_end:
        mov eax, 1
        mov edi, 1
        lea rsi, [.msg]
        mov edx, .msg_end - .msg
        syscall
        mov eax, 231
        mov edi, 1
        syscall
}

macro assert_eq reg, imm {
local .ok
        cmp reg, imm
        je .ok
        test_fail
  .ok:
        test_pass
}

macro tests_done {
local .msg, .msg_end
        jmp .msg_end
  .msg db 10, 'All tests passed.', 10
  .msg_end:
        mov eax, 1
        mov edi, 1
        lea rsi, [.msg]
        mov edx, .msg_end - .msg
        syscall
        mov eax, 231
        xor edi, edi
        syscall
}
