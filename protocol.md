# Tartarus Boot Protocol
The Tartarus Boot Protocol describes the state that the Tartarus bootloader will leave the kernel in.


### CPU State
- A20 line enabled


### Memory Map
- Sorted by `base`
- `USABLE` / `BOOT_RECLAIMABLE` entries are strict entries. In addition to the normal properties, strict entries are also guaranteed to
    - Always be page aligned
    - Never overlap with other entries
- The first page is never `USABLE` (for example in amd64 0 - 0x1000 is never usable)


# This state is described by UEFI
```
Uniprocessor, as described in chapter 8.4 of:

— Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 3, System Programming Guide, Part 1, Order Number: 253668-033US, December 2009

— See “Links to UEFI-Related Documents” ( http://uefi.org/uefi) under the heading “Intel Processor Manuals”.

Long mode, in 64-bit mode

Paging mode is enabled and any memory space defined by the UEFI memory map is identity mapped (virtual address equals physical address), although the attributes of certain regions may not have all read, write, and execute attributes or be unmarked for purposes of platform protection. The mappings to other regions, such as those for unaccepted memory, are undefined and may vary from implementation to implementation.

Selectors are set to be flat and are otherwise not used.

Interrupts are enabled-though no interrupt services are supported other than the UEFI boot services timer functions (All loaded device drivers are serviced synchronously by “polling.”)

Direction flag in EFLAGs is clear

Other general purpose flag registers are undefined

128 KiB, or more, of available stack space

The stack must be 16-byte aligned. Stack may be marked as non-executable in identity mapped page tables.

Floating-point control word must be initialized to 0x037F (all exceptions masked, double-extended-precision, round-to-nearest)

Multimedia-extensions control word (if supported) must be initialized to 0x1F80 (all exceptions masked, round-to-nearest, flush to zero for masked underflow).

CR0.EM must be zero

CR0.TS must be zero
```