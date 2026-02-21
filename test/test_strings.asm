format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/strings.inc'

segment readable executable

entry $

; --- str_len ---
        test_begin "str_len('hello') = 5"
        lea rdi, [s_hello]
        call str_len
        assert_eq rax, 5

        test_begin "str_len('') = 0"
        lea rdi, [s_empty]
        call str_len
        assert_eq rax, 0

; --- str_eq ---
        test_begin "str_eq('abc','abc') = 1"
        lea rdi, [s_abc]
        lea rsi, [s_abc2]
        call str_eq
        assert_eq rax, 1

        test_begin "str_eq('abc','abd') = 0"
        lea rdi, [s_abc]
        lea rsi, [s_abd]
        call str_eq
        assert_eq rax, 0

        test_begin "str_eq('ab','abc') = 0"
        lea rdi, [s_ab]
        lea rsi, [s_abc]
        call str_eq
        assert_eq rax, 0

; --- mem_copy ---
        test_begin "mem_copy copies 5 bytes"
        lea rdi, [buf]
        lea rsi, [s_world]
        mov rdx, 5
        call mem_copy
        ; verify first byte
        movzx rbx, byte [buf]
        assert_eq rbx, 'w'

; --- str_chr ---
        test_begin "str_chr('hello','l') finds at offset 2"
        lea rdi, [s_hello]
        mov sil, 'l'
        call str_chr
        lea rbx, [s_hello]
        sub rax, rbx
        assert_eq rax, 2

        test_begin "str_chr('hello','z') = 0"
        lea rdi, [s_hello]
        mov sil, 'z'
        call str_chr
        assert_eq rax, 0

; --- str_starts_with ---
        test_begin "str_starts_with('hello world','hello') = 1"
        lea rdi, [s_hello_world]
        lea rsi, [s_hello]
        call str_starts_with
        assert_eq rax, 1

        test_begin "str_starts_with('hello world','world') = 0"
        lea rdi, [s_hello_world]
        lea rsi, [s_world]
        call str_starts_with
        assert_eq rax, 0

        tests_done

segment readable writeable

s_hello       db 'hello', 0
s_empty       db 0
s_abc         db 'abc', 0
s_abc2        db 'abc', 0
s_abd         db 'abd', 0
s_ab          db 'ab', 0
s_world       db 'world', 0
s_hello_world db 'hello world', 0
buf           rb 64
