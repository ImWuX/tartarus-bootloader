extern core
extern g_gdtr

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
    jz error.no_cpuid

    mov eax, 0x80000000                     ; Test for CPUID extended info
    cpuid
    cmp eax, 0x80000001
    jb error.no_cpuid_ext

    mov eax, 0x80000001                     ; Test for long mode
    cpuid
    test edx, 1 << 29
    jz error.long_mode_unsupported

    call enable_a20                         ; Enable the a20 line
    jnc error.couldnt_enable_a20

    cli
    lgdt [g_gdtr]                           ; Load GDT

    mov eax, cr0
    or eax, 1                               ; Set protected mode bit
    mov cr0, eax

    jmp 0x18:entry_protected                ; Far jump into protected gdt segment

error:
.no_cpuid:
    mov bl, '1'
    jmp .print
.no_cpuid_ext:
    mov bl, '2'
    jmp .print
.long_mode_unsupported:
    mov bl, '3'
    jmp .print
.couldnt_enable_a20:
    mov bl, '4'
    jmp .print
.print:
    mov ax, (0xE << 8) | 'E'
    int 0x10
    mov al, '1'
    int 0x10
    mov al, bl
    int 0x10
    cli
    hlt

bits 32
entry_protected:
    mov eax, 0x20                           ; Reset all segments
    mov ds, eax
    mov ss, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    mov bp, sp

    jmp core

bits 16

KB_CMD equ 0x64
KB_DATA equ 0x60

;
; Attempts to enable the A20 line
; Returns:
;   cf = success
;
enable_a20:
    pusha

    call test_a20
    cmp ax, 1                                   ; Check if A20 is enabled
    je .success

    ; Try enabling A20 through BIOS
    mov ax, 0x2401
    int 0x15

    call test_a20
    cmp ax, 1                                   ; Check if A20 is enabled
    je .success

    ; Try enabling A20 through a keyboard controller
    cli
    call .wait_kb_write
    mov al, 0xAD                                ; 0xAD = Disable keyboard
    out KB_CMD, al

    call .wait_kb_write
    mov al, 0xD0                                ; 0xD0 = Read output port
    out KB_CMD, al

    call .wait_kb_data
    in al, KB_DATA                              ; Read byte from data port
    push ax

    call .wait_kb_write
    mov al, 0xD1                                ; 0xD1 = Write output port
    out KB_CMD, al

    call .wait_kb_write
    pop ax
    or al, 2                                    ; Set bit 2 of output port data (A20)
    out KB_DATA, al                             ; Write byte

    call .wait_kb_write
    mov al, 0xAE                                ; 0xAE = Enable keyboard
    out KB_CMD, al
    sti

    call test_a20
    cmp ax, 1                                   ; Check if A20 is enabled
    je .success

    ; Try enabling A20 using FastA20 (chipset)
    ; This could technically do anything, so we are doing this last
    in al, 0x92
    or al, 2
    out 0x92, al

    call test_a20
    cmp ax, 1                                   ; Check if A20 is enabled
    je .success

    popa
    clc
    ret

.success:
    popa
    stc
    ret                                         ; A20 line enabled

.wait_kb_write:
    in al, 0x64
    test al, 2
    jnz .wait_kb_write
    ret

.wait_kb_data:
    in al, 0x64
    test al, 1
    jz .wait_kb_data
    ret

;
; Tests if the A20 line is enabled
; Returns:
;   ax: 1 if the a20 line is disabled, 0 if it is enabled
;
test_a20:
    pusha

    mov ax, [0x7dfe]                            ; Get value at location of magic number

    push bx
    mov bx, 0xffff                              ; Set segment offset
    mov es, bx
    pop bx

    mov bx, 0x7e0e                              ; Location of magic number
    mov dx, [es:bx]                             ; Get value at location with offset 0xffff

    cmp ax, dx                                  ; Compare values, if they match we good!
    je .again                                   ; Test again

    popa
    mov ax, 1
    ret

.again:
    mov ax, [0x7dff]                            ; Get value at location of magic number

    push bx
    mov bx, 0xffff                              ; Set segment offset
    mov es, bx
    pop bx

    mov bx, 0x7e0f                              ; Location of magic number
    mov dx, [es:bx]                             ; Get value at location with offset 0xffff

    cmp ax, dx                                  ; Compare values, if they match we good!
    je .finish                                  ; Return

    popa
    mov ax, 1
    ret

.finish:
    popa
    xor ax, ax
    ret