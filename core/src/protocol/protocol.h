#pragma once
#include <common/fb.h>
#include <fs/vfs.h>
#include <dev/acpi.h>
#include <sys/e820.x86_64.bios.h> // TODO: this is invalid since protocol.h & linux.c are not restricted to any target...

[[noreturn]] void protocol_linux(vfs_node_t *kernel_node, vfs_node_t *ramdisk_node, char *cmd, acpi_rsdp_t *rsdp, e820_entry_t e820[], size_t e820_size, fb_t fb);