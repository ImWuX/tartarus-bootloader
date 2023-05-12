bits 16

gdt:
.null:
    dd 0
    dd 0
.code32:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
.data32:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
.code16:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 00000000b
    db 0x00
.data16:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00
.descriptor:
    dw .descriptor - .null - 1
    dd .null
    dd 0

CODE_SEGMENT32 equ gdt.code32 - gdt.null
DATA_SEGMENT32 equ gdt.data32 - gdt.null