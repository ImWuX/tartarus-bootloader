global protocol_tartarus_bios_handoff

bits 32
protocol_tartarus_bios_handoff:
    mov eax, dword [esp + 4]
    mov dword [kernel_entry], eax
    mov eax, dword [esp + 8]
    mov dword [kernel_entry + 4], eax
    mov eax, dword [esp + 12]
    mov dword [stack], eax
    mov eax, dword [esp + 16]
    mov dword [boot_info], eax
    mov eax, dword [esp + 20]
    mov dword [boot_info + 4], eax

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

    jmp 0x28:entry_long                         ; Long jump into long mode gdt segment

bits 64
entry_long:
    mov rax, 0x30                               ; Reset segments
    mov ds, rax
    mov ss, rax
    mov es, rax
    mov fs, rax
    mov gs, rax

    xor rsp, rsp
    mov esp, dword [stack]

    cld

    mov rdi, qword [boot_info]                  ; Boot info
    push qword 0                                ; Push an invalid return address
    xor rbp, rbp                                ; Invalid stack frame
    jmp qword [kernel_entry]

boot_info: dq 0
stack: dd 0
kernel_entry: dq 0