OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 0xA000;
    __tartarus_start = .;

    .early : {
        *(.entry)
        *(.early)
    }

    .text : {
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

    . = ALIGN(0x1000);
    __tartarus_end = .;
}