global protocol_tartarus_uefi_handoff

bits 64
protocol_tartarus_uefi_handoff:
    mov rax, cr0
    or rax, (1 << 16)                           ; Set write protect bit
    mov cr0, rax

    mov rax, qword rdi                          ; Set rax to entry address
    mov rdi, rsi                                ; Move boot_info into rdi
    push qword 0                                ; Push an invalid return address
    xor rbp, rbp                                ; Invalid stack frame
    jmp rax