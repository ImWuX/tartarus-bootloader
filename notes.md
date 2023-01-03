# Todo
- Elf loading is super rudamentary
- Partitioning, probably using gpt
- At some point possibly implementing UEFI
- CHS disk reading

# Documentation
- MBR Errors
  - `E:0` means a disk error occured
  - `E:1` means the bootloader was not found.

# Assumptions Made
- Assumes that there is conventional memory at `0x7C00^` for the bootloader and its memory map
- Assumes that `0xC00` is enough memory for the initial stack before a proper stack is reserved
- Assumes that there is conventional memory at `0x500` for the bios memory map

# Optimizations
- The PMM memory map sanitization is unnecessarily expensive

# Bugs
- Bootloader only supports 512byte sectors, technically fat32 supports other sizes.