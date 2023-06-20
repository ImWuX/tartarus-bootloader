global protocol_tartarus_uefi_handoff

bits 64
protocol_tartarus_uefi_handoff:
    mov eax, cr0
    or eax, (1 << 16)                           ; Set write protect bit
    mov cr0, eax

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 20
    jz .noxd

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 11)                           ; Set NX
    wrmsr
.noxd:
    mov rax, qword [rdi + 24]                   ; Boot info + 24 = entry address
    push qword 0                                ; Push an invalid return address
    xor rbp, rbp                                ; Invalid stack frame
    jmp rax