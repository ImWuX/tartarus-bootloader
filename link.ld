OUTPUT_FORMAT(binary)
ENTRY(ld_bootloader_start)

ld_bootloader_start = 0x8000;

SECTIONS
{
    . = ld_bootloader_start;

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

    ld_bootloader_end = .;

    /DISCARD/ : {
        *(.comment)
    }
}
