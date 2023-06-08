OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 0xA000;
    __tartarus_start = .; 

    .text : {
        *(.entry)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    __tartarus_end = .;
}