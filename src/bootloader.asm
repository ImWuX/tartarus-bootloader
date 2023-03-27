extern load

PAGE_SIZE equ 0x1000

bits 16
section .entry
    jmp entry_real                              ; Jump to text section

section .text
entry_real:
    xor ax, ax                                  ; Reset all segment registers
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call test_long_mode                         ; Make sure we can eventually boot into long mode
    jc .long_mode

    mov bx, TXT_ERROR_LONG_MODE
    call print
    cli
    hlt

.long_mode:
    call enable_a20                             ; Enable the a20 line
    jc .a20

    mov bx, TXT_ERROR_A20
    call print
    cli
    hlt

.a20:
    cli
    lgdt [gdt.descriptor]                       ; Load gdt

    mov eax, cr0
    or eax, 1                                   ; Set protected mode bit
    mov cr0, eax                                ; Enable protected mode

    jmp CODE_SEGMENT:entry_protected            ; Long jump into protected gdt segment

bits 32
entry_protected:
    mov eax, DATA_SEGMENT
    mov ds, eax
    mov ss, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    call load                                   ; Initialize memory and load kernel into memory
    mov dword [load_ret], eax                   ; Save entry + params

    mov edi, [load_ret]
    mov ebp, [edi + 8]
    mov esp, ebp

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

    call gdt_set_long                           ; Modify GDT to be long mode compatible
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
    mov rax, DATA_SEGMENT                       ; Reset segments just incase
    mov ds, rax
    mov ss, rax
    mov es, rax
    mov fs, rax
    mov gs, rax

    xor rdi, rdi
    mov edi, dword [load_ret]                   ; Gets the parameter struct
    mov rax, qword [edi]                        ; Gets the entry address
    add edi, 16                                 ; Increment to skip the entry address
    jmp rax                                     ; Load kernel

load_ret: dd 0

%include "includes/gdt.inc"
%include "includes/a20.inc"
%include "includes/print.inc"
%include "includes/long_mode.inc"

section .data
TXT_ERROR_A20:          db "Tartarus Panic | Failed to enable the A20 line", 0
TXT_ERROR_LONG_MODE:    db "Tartarus Panic | Your machine does not support long mode (x86_64)", 0