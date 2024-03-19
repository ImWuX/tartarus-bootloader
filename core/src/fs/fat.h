#pragma once
#include <fs/vfs.h>
#include <dev/disk.h>

typedef enum {
    FAT_TYPE_12,
    FAT_TYPE_16,
    FAT_TYPE_32,
} fat_type_t;

vfs_t *fat_initialize(disk_part_t *partition);