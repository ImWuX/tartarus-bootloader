global memcpy
global memset
global memmove

%ifdef __UEFI64
memcpy:
    mov rcx, rdx
    mov rax, rdi
    rep movsb
    ret

memset:
    push rdi
    mov rax, rsi
    mov rcx, rdx
    rep stosb
    pop rax
    ret

memmove:
    mov rcx, rdx
    mov rax, rdi
    cmp rdi, rsi
    jb .move
    add rdi, rcx
    add rsi, rcx
    dec rdi
    dec rsi
    std
.move:
    rep movsb
    cld
    ret
%else
memcpy:
    push esi
    push edi
    mov eax, dword [esp + 12]
    mov edi, eax
    mov esi, dword [esp + 16]
    mov ecx, dword [esp + 20]
    rep movsb
    pop edi
    pop esi
    ret

memset:
    push edi
    mov edx, dword [esp + 8]
    mov edi, edx
    mov eax, dword [esp + 12]
    mov ecx, dword [esp + 16]
    rep stosb
    mov eax, edx
    pop edi
    ret

memmove:
    push esi
    push edi
    mov edi, dword [esp + 12]
    mov esi, dword [esp + 16]
    mov ecx, dword [esp + 20]
    mov eax, edi
    cmp edi, esi
    jb .move
    add edi, ecx
    add esi, ecx
    dec edi
    dec esi
    std
.move:
    rep movsb
    cld
    pop edi
    pop esi
    ret
%endif