OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

PHDRS
{
    ro PT_LOAD FLAGS((1 << 2));
    rx PT_LOAD FLAGS((1 << 0) | (1 << 2));
    rw PT_LOAD FLAGS((1 << 1) | (1 << 2));
}

SECTIONS
{
    . = 0xA000;

    .text : {
        *(.entry)
        *(.text)
    } :rx

    .rodata : {
        *(.rodata)
    } :ro

    .data : {
        *(.data)
    } :rw

    .bss : {
        *(.bss)
        *(COMMON)
    } :rw
}