bits 32

section .data
align 16
linux_gdt:
    dq 0
    dq 0

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011011b
    db 11001111b
    db 0x00

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010011b
    db 11001111b
    db 0x00
.gdtr:
    dw (linux_gdt.gdtr - linux_gdt) - 1
    dd linux_gdt

section .text
global linux_handoff
linux_handoff:
    add esp, 4
    lgdt [linux_gdt.gdtr]
    jmp 0x10:.sws

.sws:
    mov eax, 0x18
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax

    xor ebp, ebp
    xor edi, edi
    xor ebx, ebx

    mov esi, [esp + 4]
    jmp [esp]