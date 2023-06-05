global handoff

bits 32
handoff:
    mov eax, cr4
    or eax, 1 << 5                              ; Enable PAE bit
    mov cr4, eax                                ; Move the modified register back

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8                              ; Set long mode bit
    wrmsr                                       ; Enable long mode

    mov eax, cr0
    or eax, (1 << 31) | (1 << 16)               ; Set paging bit & write protect bits
    mov cr0, eax                                ; Enable paging
    cli
    hlt

    lgdt [gdt.descriptor]
    jmp CODE_SEGMENT:entry_long                 ; Long jump into long mode gdt segment

bits 64
entry_long:
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 20
    jz .noxd

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 11)                           ; Set NX
    wrmsr
.noxd:
    mov rax, DATA_SEGMENT                       ; Reset segments just in-case
    mov ds, rax
    mov ss, rax
    mov es, rax
    mov fs, rax
    mov gs, rax

    cli
    hlt

gdt:
.null:
    dd 0
    dd 0
.code:
    dw 0            ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base medium
    db 10011010b    ; Access Byte
    db 00100000b    ; Flags + Limit high
    db 0x00         ; Base high
.data:
    dw 0            ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base medium
    db 10010010b    ; Access Byte
    db 00000000b    ; Flags + Limit high
    db 0x00         ; Base high
.descriptor:
    dw .descriptor - .null - 1
    dd .null
    dd 0

CODE_SEGMENT equ gdt.code - gdt.null
DATA_SEGMENT equ gdt.data - gdt.null