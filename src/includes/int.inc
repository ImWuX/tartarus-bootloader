bits 32
global exec_int
exec_int:
    mov al, [esp + 4]
    mov [.int_no], al                           ; Save the interrupt number of the stack

    sgdt [.gdt]                                 ; Save GDT just in case bios overwrites it
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
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	lidt [.idt]                                 ; Load real mode idt
    sti                                         ; Enable interrupts

    mov ah, 0x0E
    mov al, 'Z'

    db 0xCD                                     ; Opcode for interrupt
.int_no:
    db 0                                        ; Interrupt number

    cli                                         ; Clear interrupts

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
.gdt:
    dd 0
    dd 0
.idt:
    dw 0x3FF
    dd 0
