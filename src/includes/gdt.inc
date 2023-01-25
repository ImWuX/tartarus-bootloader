bits 16

gdt:
.null:
    dd 0
    dd 0
.code:
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base medium
    db 10011010b    ; Access Byte
    db 11001111b    ; Flags + Limit high
    db 0x00         ; Base high
.data:
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base medium
    db 10010010b    ; Access Byte
    db 11001111b    ; Flags + Limit high
    db 0x00         ; Base high
.descriptor:
    dw .descriptor - .null - 1
    dd .null
    dd 0

CODE_SEGMENT equ gdt.code - gdt.null
DATA_SEGMENT equ gdt.data - gdt.null

global gdt_set_protected
gdt_set_protected:
    mov [ss:gdt.code + 6], byte 11001111b
    mov [ss:gdt.data + 6], byte 11001111b
    ret

bits 32
global gdt_set_real
gdt_set_real:
    mov [gdt.code + 6], byte 00000000b
    mov [gdt.data + 6], byte 00000000b
    ret

gdt_set_long:
    mov [gdt.code], word 0
    mov [gdt.data], word 0
    mov [gdt.code + 6], byte 00100000b
    mov [gdt.data + 6], byte 00000000b
    ret