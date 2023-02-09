# Tartarus Boot Protocol
The Tartarus Boot Protocol describes the state that the Tartarus bootloader will leave the kernel in. Note that the Tartarus bootloader is purely a x86_64 bootloader.

# Kernel Requirements
- Supports x86_64
- Kernel elf should start at or above `0xFFFF FFFF 8000 0000`

# State Immediately After Bootloader Exit
**Anything not listed here should not be assumed to be true or false.**
- A20 line is opened
- CPU is set to long mode
- Stack is set to some area of 64KiB of contigous bootloader reclaimable memory
- GDT is assured to be loaded in bootloader reclaimable memory with at least the following entries:
    - 0x0:  NULL
    - 0x8:  64bit CODE ring0 readable
    - 0x10: 64ibt DATA ring0 writable

## The Virtual Memory Map
- 4GB and all non `BAD` memory entries will be mapped starting at
    - `0x0000 0000 0000 0000` *Identity mapped*
    - `0xFFFF 8000 0000 0000` *Higher Half Direct Memory (HHDM)*
- Kernel is mapped according to the ELF file

## The Physical Memory Map
- Anything under `0x1000` will never be included in an entry
- All entries are ensured to not be overlapping
- All entries are ensured to be ordered
- Contigous memory of the same type is ensured to be one entry (No back to back entries of the same type)
- Entries of the following types will be paged alined (`0x1000`):
    - `USABLE`
    - `BOOT_RECLAIMABLE`
    - `FRAMEBUFFER`
- `BOOT_RECLAIMABLE` entries should only be reclaimed once the kernel is either done with data provided by the bootloader or has moved it
- Conflicting entries provided by the bootloader will be resolved by prioritizing ACPI entries over usable, reserved over ACPI, and bad over reserved

## VESA Framebuffer
- The VESA mode will be set to:
    - Linear
    - RGB
    - 32BPP

  if the GPU does not support this framebuffer, it will not load the kernel