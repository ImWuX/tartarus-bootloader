BOOT_ADDRESS equ 0x7C00
org BOOT_ADDRESS
bits 16

DAP equ BOOT_ADDRESS + 512
BUFFER equ BOOT_ADDRESS + 1024
CORE_ADDRESS equ 0xA000

;
;   Entry
;   BPB is required because a bad bios might assume its going to be there for whatever reason
;
bpb:
    jmp boot
    nop
.oem_id:                db "TARTARUS"
.bytes_per_sector:      dw 512
.sectors_per_cluster:   db 0
.reserved_sectors:      dw 0
.fat_count:             db 0
.root_entry_count:      dw 0
.total_sectors:         dw 0
.media_desc_type:       db 0
.sectors_per_fat:       dw 0
.sectors_per_track:     dw 18
.head_count:            dw 2
.hidden_sectors:        dd 0
.sector_count:          dd 0
.drive_number:          db 0
.reserved:              db 0
.signature:             db 0
.volume_id:             dd 0
.volume_label:          db "TARTARUS"
.sys_id:                db "BOOTPART"

;
;   Boot
;
boot:
    cli
    jmp 0:reset_seg                                         ; Reset segment for quicky bioses
reset_seg:
    xor ax, ax                                              ; Reset important segment registers
    mov es, ax
    mov ds, ax
    mov ss, ax
    cld                                                     ; Clear direction flag
    sti                                                     ; Enable interrupts

    mov byte [boot_drive], dl                               ; Set boot_drive
    mov bp, (0xE << 8 | '0')                                ; Prepare bp to be used for error code
    mov sp, BOOT_ADDRESS - 4                                ; Set initial stack below the boot sector

    cmp dl, 0x80
    jb error.invalid_disk                                   ; Check that boot drive is not a floppy

    cmp dl, 0x8F
    ja error.invalid_disk                                   ; Unexpected boot drive

    ; Check for int13 extensions
    ;
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13                                                ; Check for int13 extensions
    jc error.noext

    ; Read extended drive params for sector_size of boot_drive
    ;
    mov si, BUFFER                                          ; si = Param buffer
    mov [BUFFER], word 0x1E                                 ; Buffer size
    mov ah, 0x48                                            ; Extended read drive params
    int 0x13
    jc error.noext
    mov ax, word [BUFFER + 24]
    mov word [sector_size], ax                              ; Set sector_size

    ; Read in protective mbr and ensure its type is GPT
    ;
    mov eax, BUFFER                                         ; Read into buffer
    mov ebx, 512                                            ; Read 512 bytes
    xor ecx, ecx                                            ; LBA lower = 0
    xor edi, edi                                            ; LBA upper = 0
    call read
    cmp byte [BUFFER + 0x1BE + 4], 0xEE                     ; Check for GPT type
    jne error.legacy_mbr

    ; Read in the GPT header
    ;
    inc ecx
    call read                                               ; Read GPT

    ; Read in the GPT entry array
    ;
    mov ecx, dword [BUFFER + 0x48]                          ; Lower 32 bits of entry array LBA
    mov edi, dword [BUFFER + 0x4C]                          ; Upper 32 bits of entry array LBA
    mov ebx, dword [BUFFER + 0x54]                          ; Partition entry size
    mov eax, dword [BUFFER + 0x50]                          ; Number of partition entries
    push ebx
    push eax
    mul ebx
    cmp edx, 0
    jnz error.gpt_too_large
    mov ebx, eax                                            ; Entry table size
    mov eax, BUFFER
    call read
    pop ecx                                                 ; Number of partition entries
    pop eax                                                 ; Partition entry size

    ; Loop over the entries and look for the Tartarus boot entry
    ;
    xor ax, ax
    mov es, ax                                              ; Clear es
    mov ebx, BUFFER                                         ; Set eax to entry ptr
.loop:
    push ecx

    mov ecx, 16                                             ; GUID length
    mov esi, ebx
    mov edi, bpb.volume_label
    repe cmpsb                                              ; Compare the entry's GUID to the tartarus GUID
    pop ecx
    jz .found
    add ebx, eax                                            ; Add entry size to entry ptr

    loop .loop
    jmp error.part_not_found
.found:
    mov edi, dword [ebx + 0x24]                             ; Upper 32 bits of LBA
    cmp edi, dword [ebx + 0x2C]                             ; Upper 32 bits of end LBA
    jne error.core_too_large

    xor edx, edx
    movzx eax, word [sector_size]                           ; Sector size
    mov ecx, dword [ebx + 0x28]                             ; Lower 32 bits of end LBA
    sub ecx, dword [ebx + 0x20]                             ; Lower 32 bits of LBA
    mul ecx
    cmp edx, 0
    jnz error.core_too_large

    mov ecx, dword [ebx + 0x20]                             ; Lower 32 bits of LBA
    mov edi, dword [ebx + 0x24]                             ; Upper 32 bits of LBA

    mov ebx, eax                                            ; Size of core
    mov eax, CORE_ADDRESS
    call read

    jmp CORE_ADDRESS

boot_drive: db 0
sector_size: dw 0

error:
.core_too_large:
    inc bp
.part_not_found:
    inc bp
.gpt_too_large:
    inc bp
.legacy_mbr:
    inc bp
.read_failed:
    inc bp
.noext:
    inc bp
.invalid_disk:
    inc bp
.print:
    mov ax, (0xE << 8) | 'E'
    int 0x10
    mov ax, bp
    int 0x10
    cli
    hlt

;
;   Read from disk.
;   Assumptions: ds is zero. `sector_size` and `boot_drive` label/variable exists.
;     eax = dest
;     ebx = byte count
;     edi = upper 32bits of lba
;     ecx = lower 32bits of lba
;
read:
    pushad

    mov [DAP], dword (0x10 | (1 << 16))                     ; DAP Size + Sector Count
    mov [DAP + 4], dword eax                                ; Destination seg:off
    mov [DAP + 8], dword ecx                                ; Lower 32bits of LBA
    mov [DAP + 12], dword edi                               ; Upper 32bits of LBA

    mov eax, ebx                                            ; dividend (eax) = byte count
    xor edx, edx                                            ; Clear edx for division
    movzx ecx, word [sector_size]                           ; divisor (ecx) = sector size
    div ecx
    cmp edx, 0                                              ; remainder > 0
    je .skipinc
    inc eax

.skipinc:
    mov ecx, eax                                            ; loop counter (ecx) = sectors to read
    mov si, DAP                                             ; si = DAP
    mov dl, byte [boot_drive]
    mov bx, word [sector_size]

.loop:
    mov ah, 0x42                                            ; Extended read sectors from drive
    int 0x13
    jc error.read_failed
    add word [DAP + 4], bx                                 ; Add sector size to destination
    jnc .loop_nc
    add word [DAP + 6], 0x1000
    .loop_nc:
    add dword [DAP + 8], 1                                  ; Add to LBA lower
    adc dword [DAP + 12], 0                                 ; Add to LBA upper
    loop .loop

.done:
    popad
    ret

times 440-($-$$) db 0
; Deployer will fill in the protective mbr and boot signature