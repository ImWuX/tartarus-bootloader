# Documentation
- MBR Errors
  - `E:0` means a disk error occured
  - `E:1` means the bootloader was not found.

# Assumptions Made
Bootloader:
- Assumes that there is conventional memory at 0x8000 up for the bootloader memory map
- Assumes that there is conventional memory at 0x500 for the bios memory map

# Todo
- VESA 2.0 is assumed. Not checked...
- Right now the display mode is just picked if it is 1920x1080 and rgb. It should be more dynamic and shit
- Elf loading is super rudamentary
- Partitioning, probably using gpt
- At some point possibly implementing UEFI
- Fat32 doesnt actually check for invalid clusters, it probably should just incase
- CHS disk reading
- Pass HHDM address to kernel

# Bugs
- Bootloader only supports 512byte sectors, technically fat32 supports other sizes.