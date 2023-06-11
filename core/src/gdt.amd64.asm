global gdt.descriptor

;
; GDT
;
gdt:
.null:
    dd 0
    dd 0
.code16:
    dw 0xFFFF           ; Low limit
    dw 0x0000           ; Low base
    db 0x00             ; Mid base
    db 10011010b        ; Access Byte
    db 00000000b        ; Flags & Upper limit
    db 0x00             ; Upper base
.data16:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00
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
.code64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b
    db 00100000b
    db 0x00
.data64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00
.descriptor:
    dw .descriptor - .null - 1
    dd .null
    dd 0
