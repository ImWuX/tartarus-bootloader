#ifndef FS_FAT_H
#define FS_FAT_H

#include <stdint.h>
#include <drivers/disk.h>

typedef enum {
    FAT12 = 12,
    FAT16 = 16,
    FAT32 = 32,
} fat_type_t;

typedef struct {
    disk_part_t *partition;
    fat_type_t type;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint32_t fat_sectors;
    uint16_t sector_size;
    uint16_t cluster_size;
    union {
        uint32_t cluster_number;
        uint16_t entry_count;
    } root_dir;
} fat_info_t;

typedef struct {
    fat_info_t *info;
    bool is_dir;
    uint32_t cluster_number;
    uint32_t size;
} fat_file_t;

fat_info_t *fat_initialize(disk_part_t *partition);
fat_file_t *fat_root_lookup(fat_info_t *info, const char *name);
fat_file_t *fat_dir_lookup(fat_file_t *dir, const char *name);
uint64_t fat_read(fat_file_t *file, uint64_t offset, uint64_t count, void *dest);

#endif