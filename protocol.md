# Tartarus Boot Protocol
The Tartarus Boot Protocol describes the state that the Tartarus bootloader will leave the kernel in.

### CPU State (AMD64)
- A20 line enabled
- CS is set to a flag 64bit code segment
- All other segments are set to flat 64bit data segments
- Registers:
    - `RFLAGS.DF` CLEAR
    - `CR0.PE` SET
    - `CR0.PG` SET
    - `CR0.WP` SET
    - `CR4.PAE` SET
    - `EFER.LMA` SET

### Memory Map
- Sorted by `base`
- `USABLE` / `BOOT_RECLAIMABLE` entries are strict entries. In addition to the normal properties, strict entries are also guaranteed to
    - Always be page aligned
    - Never overlap with other entries
- The first page is never `USABLE` (for example in AMD64 0 - 0x1000 is never usable)