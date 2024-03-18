bits 32
section .early

global int_exec
int_exec:
    mov al, byte [esp + 4]
    mov byte [.int_no], al                      ; Set the interrupt number

    mov eax, dword [esp + 8]
    mov dword [.regs], eax                      ; Save the pointer to regs

    o32 sgdt [.gdt]                             ; Save GDT just in case bios overwrites it

	lidt [.idt]                                 ; Load BIOS idt

    push ebx                                    ; Push non scratch registers
    push esi
    push edi
    push ebp

    jmp 0x8:.realseg                            ; Jump to real mode segment

.realseg:
bits 16
    mov ax, 0x10                                ; Load 16bit data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, cr0
    and al, ~1                                  ; Disable protected mode
    mov cr0, eax

    jmp 0:.zeroseg                              ; Jump to null segment

.zeroseg:
	xor ax, ax                                  ; Reset data segment registers
	mov ss, ax

    mov dword [ss:.esp], esp
    mov esp, dword [ss:.regs]                   ; Set esp to point to registers and pop them in
    pop gs
    pop fs
    pop es
    pop ds
    popfd
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    mov esp, dword [ss:.esp]                    ; Reset esp

    sti                                         ; Enable interrupts

    db 0xCD                                     ; Opcode for interrupt
.int_no:
    db 0                                        ; Interrupt number

    cli                                         ; Clear interrupts

    mov dword [ss:.esp], esp
    mov esp, dword [ss:.regs]
    lea esp, [esp + 40]
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    pushfd
    push ds
    push es
    push fs
    push gs
    mov esp, dword [ss:.esp]

    o32 lgdt [ss:.gdt]                          ; Load GDT in case it was overwritten

    mov eax, cr0
    or eax, 1                                   ; Enable protected mode
    mov cr0, eax

    jmp 0x18:.protectedseg                      ; Jump to protected segment

.protectedseg:
bits 32
    mov eax, 0x20                               ; Load 32bit data segment
	mov ds, eax
	mov es, eax
	mov fs, eax
	mov gs, eax
	mov ss, eax

    pop ebp                                     ; Load back non scratch registers
    pop edi
    pop esi
    pop ebx

    ret

bits 32
align 16
.esp:   dd 0
.regs:  dd 0
.gdt:   dd 0
        dd 0
.idt:   dw 0x3FF
        dd 0