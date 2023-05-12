extern core

bits 16
section .entry
    jmp entry_real

section .text
entry_real:
    xor ax, ax                              ; Reset all segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pushfd                                  ; Test for CPUID
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    jz .error

    mov eax, 0x80000000                     ; Test for CPUID extended info
    cpuid
    cmp eax, 0x80000001
    jb .error

    mov eax, 0x80000001                     ; Test for long mode
    cpuid
    test edx, 1 << 29
    jz .error

    call enable_a20                         ; Enable the a20 line
    jnc .error

    cli
    lgdt [gdt.descriptor]                   ; Load GDT

    mov eax, cr0
    or eax, 1                               ; Set protected mode bit
    mov cr0, eax

    jmp CODE_SEGMENT32:entry_protected      ; Long jump into protected gdt segment
.error:
    cli
    hlt

bits 32
entry_protected:
    mov eax, DATA_SEGMENT32                 ; Reset all segments
    mov ds, eax
    mov ss, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    jmp core

%include "a20.inc"
%include "gdt.inc"