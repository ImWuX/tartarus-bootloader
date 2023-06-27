# Tartarus Boot Protocol
The Tartarus Boot Protocol describes the state a bootloader conforming to the protocol shall leave a kernel in.

## CPU State
The general idea is to leave the CPU in the simplest state possible for the kernel to take over and continue without having to do a lot of arch specific setup.
### AMD64
- A20 line enabled
- `CS` is set to a flag 64bit code segment
- All other segments are set to flat 64bit data segments
- Registers:
    - `RFLAGS.DF` CLEAR
    - `CR0.PE` SET
    - `CR0.PG` SET
    - `CR0.WP` SET
    - `CR4.PAE` SET
    - `EFER.LMA` SET

AP's are guaranteed stacks of at least 16Kb
SMP is guaranteed a stack of 64Kb

## Boot Info
The bootloader prepares a structure that will be passed to the kernel as the one and only argument to the entrypoint. This structure is defined in the `tartarus.h` header file.
Note that by default the boot_info structure along with all the pointers within are in the HHDM.

### AMD64
On amd64 the System V ABI is assumed. The only implication relevant to the kernel is that tartarus will pass `boot_info` through the `rdi` register as defined by the SysV calling convention. If the target kernel uses another ABI, an assembly stub is needed to retrieve boot info.

## Physical Memory Map
- Sorted by base
- `USABLE` / `BOOT_RECLAIMABLE` entries are strict entries. In addition to the normal properties, strict entries are also guaranteed to
    - Always be page aligned
    - Never overlap with other entries
- The first page is never `USABLE` (for example in AMD64 0 - 0x1000 is never usable)