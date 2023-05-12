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
%endif