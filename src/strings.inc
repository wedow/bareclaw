; strings.inc — string and memory utilities for x86-64 Linux
; Convention: args per System V ABI (rdi, rsi, rdx, rcx, r8, r9)
; Preserves rbx, rbp, r12-r15. Clobbers rax, rcx, rdx, rsi, rdi, r8-r11.

; str_len — rdi=string pointer, returns rax=length (excludes null)
str_len:
        xor rax, rax
  .loop:
        cmp byte [rdi + rax], 0
        je .done
        inc rax
        jmp .loop
  .done:
        ret

; str_eq — rdi=str1, rsi=str2, returns rax=1 if equal, 0 if not
str_eq:
        xor rcx, rcx
  .loop:
        movzx eax, byte [rdi + rcx]
        movzx edx, byte [rsi + rcx]
        cmp al, dl
        jne .neq
        test al, al
        jz .eq
        inc rcx
        jmp .loop
  .eq:
        mov eax, 1
        ret
  .neq:
        xor eax, eax
        ret

; mem_copy — rdi=dest, rsi=src, rdx=count, returns rax=dest
mem_copy:
        mov rax, rdi
        xor rcx, rcx
  .loop:
        cmp rcx, rdx
        je .done
        movzx r8d, byte [rsi + rcx]
        mov byte [rdi + rcx], r8b
        inc rcx
        jmp .loop
  .done:
        ret

; mem_set — rdi=dest, sil=byte, rdx=count, returns rax=dest
mem_set:
        mov rax, rdi
        xor rcx, rcx
  .loop:
        cmp rcx, rdx
        je .done
        mov byte [rdi + rcx], sil
        inc rcx
        jmp .loop
  .done:
        ret

; str_chr — rdi=string, sil=char, returns rax=pointer or 0 if not found
str_chr:
  .loop:
        movzx eax, byte [rdi]
        cmp al, sil
        je .found
        test al, al
        jz .nope
        inc rdi
        jmp .loop
  .found:
        mov rax, rdi
        ret
  .nope:
        xor eax, eax
        ret

; str_copy — rdi=dest, rsi=src, returns rax=dest
str_copy:
        mov rax, rdi
        xor rcx, rcx
  .loop:
        movzx edx, byte [rsi + rcx]
        mov byte [rdi + rcx], dl
        test dl, dl
        jz .done
        inc rcx
        jmp .loop
  .done:
        ret

; str_starts_with — rdi=string, rsi=prefix, returns rax=1 if starts with, 0 if not
str_starts_with:
        xor rcx, rcx
  .loop:
        movzx edx, byte [rsi + rcx]
        test dl, dl
        jz .yes
        movzx eax, byte [rdi + rcx]
        cmp al, dl
        jne .no
        inc rcx
        jmp .loop
  .yes:
        mov eax, 1
        ret
  .no:
        xor eax, eax
        ret
