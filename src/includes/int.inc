bits 32
global int_exec
int_exec:
    mov al, [esp + 4]
    mov [.int_no], al                           ; Save the interrupt number of the stack

    mov eax, dword [esp + 8]
    mov dword [.regs], eax                      ; Save the pointer to regs

    sgdt [.gdt]                                 ; Save GDT just in case bios overwrites it

	lidt [.idt]                                 ; Load BIOS idt

    call gdt_set_real                           ; Modify the GDT to be real mode compatible
    jmp CODE_SEGMENT:.realseg                   ; Jump to real mode segment

.realseg:
bits 16
    mov eax, cr0
    mov [.cr0], eax                             ; Save cr0
    and eax, ~1                                 ; Disable protected mode
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

    call gdt_set_protected                      ; Modify GDT to be protected mode compatible

    jmp CODE_SEGMENT:.protectedseg              ; Jump to protected segment

.protectedseg:
bits 32
    mov eax, DATA_SEGMENT                       ; Load data segment
	mov ds, eax
	mov es, eax
	mov fs, eax
	mov gs, eax
	mov ss, eax

    ret

bits 32
align 16
.cr0:
    dd 0
.esp:
    dd 0
.regs:
    dd 0
.gdt:
    dd 0
    dd 0
.idt:
    dw 0x3FF
    dd 0
