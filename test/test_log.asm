format ELF64 executable 3

include '../src/test_macros.inc'
include '../src/syscalls.inc'
include '../src/strings.inc'
include '../src/log.inc'

segment readable executable

entry $

; --- byte_to_hex ---
        test_begin "byte_to_hex(0xFF) = 'ff'"
        mov dil, 0xFF
        lea rsi, [hex_buf]
        call byte_to_hex
        movzx rax, byte [hex_buf]
        assert_eq rax, 'f'
        test_begin "byte_to_hex(0xFF) second char"
        movzx rax, byte [hex_buf + 1]
        assert_eq rax, 'f'

        test_begin "byte_to_hex(0x00) = '00'"
        mov dil, 0x00
        lea rsi, [hex_buf]
        call byte_to_hex
        movzx rax, byte [hex_buf]
        assert_eq rax, '0'
        test_begin "byte_to_hex(0x00) second char"
        movzx rax, byte [hex_buf + 1]
        assert_eq rax, '0'

        test_begin "byte_to_hex(0x42) = '42'"
        mov dil, 0x42
        lea rsi, [hex_buf]
        call byte_to_hex
        movzx rax, byte [hex_buf]
        assert_eq rax, '4'
        test_begin "byte_to_hex(0x42) second char"
        movzx rax, byte [hex_buf + 1]
        assert_eq rax, '2'

; --- int_to_str ---
        test_begin "int_to_str(12345) = '12345'"
        mov rdi, 12345
        lea rsi, [int_buf]
        call int_to_str
        ; rax = pointer to start of number
        mov rdi, rax
        lea rsi, [s_12345]
        call str_eq
        assert_eq rax, 1

        test_begin "int_to_str(0) = '0'"
        xor edi, edi
        lea rsi, [int_buf]
        call int_to_str
        mov rdi, rax
        lea rsi, [s_zero]
        call str_eq
        assert_eq rax, 1

        test_begin "int_to_str(999) = '999'"
        mov rdi, 999
        lea rsi, [int_buf]
        call int_to_str
        mov rdi, rax
        lea rsi, [s_999]
        call str_eq
        assert_eq rax, 1

; --- log_gen_session_id ---
        test_begin "log_gen_session_id length = 16"
        lea rdi, [session_buf]
        call log_gen_session_id
        mov rdi, rax
        call str_len
        assert_eq rax, 16

        test_begin "log_gen_session_id all hex chars"
        ; check each of 16 chars is in 0-9 or a-f
        xor ecx, ecx
.check_hex:
        cmp ecx, 16
        je .hex_ok
        movzx eax, byte [session_buf + rcx]
        ; check 0-9
        cmp al, '0'
        jb .hex_fail
        cmp al, '9'
        jbe .hex_next
        ; check a-f
        cmp al, 'a'
        jb .hex_fail
        cmp al, 'f'
        ja .hex_fail
.hex_next:
        inc ecx
        jmp .check_hex
.hex_fail:
        test_fail
.hex_ok:
        test_pass

; --- log_open + log_write + log_close ---
        test_begin "log_open/write/close round-trip"
        ; open log
        lea rdi, [test_logdir]
        lea rsi, [test_sessid]
        call log_open
        cmp rax, 0
        jl .roundtrip_fail
        mov [test_fd], eax

        ; write an entry
        mov edi, [test_fd]
        lea rsi, [s_user]
        lea rdx, [s_hello]
        call log_write

        ; close
        mov edi, [test_fd]
        call log_close

        ; read back the file
        lea rdi, [test_logpath]
        xor esi, esi                    ; O_RDONLY
        xor edx, edx
        call sys_open
        cmp rax, 0
        jl .roundtrip_fail
        mov ebx, eax                    ; ebx = read fd

        mov edi, ebx
        lea rsi, [readback_buf]
        mov edx, 2048
        call sys_read
        mov [readback_len], rax

        mov edi, ebx
        call sys_close

        ; verify content contains "user"
        lea rdi, [readback_buf]
        mov sil, 'u'
        call str_chr
        test rax, rax
        jz .roundtrip_fail

        ; verify content contains "hello world"
        ; search for 'h' in readback, then check str_starts_with
        lea rdi, [readback_buf]
.find_hello:
        mov sil, 'h'
        call str_chr
        test rax, rax
        jz .roundtrip_fail
        mov rdi, rax
        lea rsi, [s_hello]
        push rdi
        call str_starts_with
        pop rdi
        test rax, rax
        jnz .found_hello
        inc rdi
        jmp .find_hello
.found_hello:
        ; verify content contains "=== "
        lea rdi, [readback_buf]
        mov sil, '='
        call str_chr
        test rax, rax
        jz .roundtrip_fail

        test_pass
        jmp .roundtrip_done
.roundtrip_fail:
        test_fail
.roundtrip_done:

        ; cleanup: unlink the test file and rmdir
        ; use sys_open trick — actually just leave it in /tmp

        tests_done

segment readable writeable

hex_buf       rb 4
int_buf       rb 24
session_buf   rb 20
s_12345       db '12345', 0
s_zero        db '0', 0
s_999         db '999', 0
s_user        db 'user', 0
s_hello       db 'hello world', 0
test_logdir   db '/tmp/szc_test_log', 0
test_sessid   db 'deadbeef01234567', 0
test_logpath  db '/tmp/szc_test_log/deadbeef01234567.txt', 0
test_fd       dd 0
readback_buf  rb 2048
readback_len  dq 0
timespec_buf  dq 0, 0
