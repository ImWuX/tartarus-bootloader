OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

PHDRS
{
    text    PT_LOAD FLAGS((1 << 0) | (1 << 2));
    data    PT_LOAD FLAGS((1 << 1) | (1 << 2));
    rodata  PT_LOAD FLAGS((1 << 2));
}

SECTIONS
{
    . = 0xA000;
    __tartarus_start = .;

    .early : {
        *(.entry)
        *(.early)
    } :text

    .text : {
        *(.text)
    } :text

    .rodata : {
        *(.rodata)
    } :rodata

    .data : {
        *(.data)
    } :data

    .bss : {
        *(.bss)
        *(COMMON)
    } :data

    . = ALIGN(0x1000);
    __tartarus_end = .;
}