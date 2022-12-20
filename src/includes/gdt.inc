bits 16

gdt:
.null:
    dd 0
    dd 0
.ring0_code:
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base medium
    db 10011010b    ; Access Byte
    db 11001111b    ; Flags + Limit high
    db 0x00         ; Base high
.ring0_data:
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base medium
    db 10010010b    ; Access Byte
    db 11001111b    ; Flags + Limit high
    db 0x00         ; Base high
.descriptor:
    dw .descriptor - .null - 1
    ; // TODO: This is being zero extended. use 32bit operations here
    dq .null

CODE_SEGMENT equ gdt.ring0_code - gdt.null
DATA_SEGMENT equ gdt.ring0_data - gdt.null

bits 32
gdt_elevate_long:
    mov [gdt.ring0_code + 6], byte 10101111b
    mov [gdt.ring0_data + 6], byte 10101111b
    ret