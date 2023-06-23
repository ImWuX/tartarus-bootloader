global protocol_tartarus_uefi_handoff

bits 64
protocol_tartarus_uefi_handoff:
    mov rax, cr0
    or rax, (1 << 16)                           ; Set write protect bit
    mov cr0, rax

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 20
    jz .noxd

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 11)                           ; Set NX
    wrmsr
.noxd:
    mov rax, qword rdi                          ; Set rax to entry address
    mov rdi, rsi                                ; Move boot_info into rdi
    push qword 0                                ; Push an invalid return address
    xor rbp, rbp                                ; Invalid stack frame
    jmp rax