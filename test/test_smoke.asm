format ELF64 executable 3

include '../src/test_macros.inc'

segment readable executable

entry $
        test_begin "1+1=2"
        mov rax, 1
        add rax, 1
        assert_eq rax, 2

        tests_done
