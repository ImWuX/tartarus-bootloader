#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#ifdef __UEFI
#include <efi.h>
#endif

typedef struct disk_part {
    struct disk *disk;
    uint64_t lba;
    uint64_t size;
    struct disk_part *next;
} disk_part_t;

typedef struct disk {
#if defined __BIOS && defined __AMD64
    uint8_t drive_number;
#endif
#ifdef __UEFI
    EFI_BLOCK_IO *io;
#endif
    uint64_t sector_count;
    uint16_t sector_size;
    bool writable;
    struct disk_part *partitions;
    struct disk *next;
} disk_t;

extern disk_t *g_disks;

void disk_initialize();
bool disk_read_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *dest);
bool disk_write_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *src);

#endif