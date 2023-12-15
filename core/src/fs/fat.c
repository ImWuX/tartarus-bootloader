#include "fat.h"
#include <libc.h>
#include <log.h>
#include <memory/heap.h>

#define FAT12_BAD 0xFF7
#define FAT16_BAD 0xFFF7
#define FAT32_BAD 0xFFFFFF7

#define ATTR_READ_ONLY (1 << 0)
#define ATTR_HIDDEN (1 << 1)
#define ATTR_SYSTEM (1 << 2)
#define ATTR_VOLUME_ID (1 << 3)
#define ATTR_DIRECTORY (1 << 4)
#define ATTR_ARCHIVE (1 << 5)

#define IS_DIR_FREE(c) ((c) == 0xE5 || (c) == 0)

#define IS_BAD(cluster, type) (((cluster) < 2) ||                             \
                               ((type) == FAT12 && (cluster) == FAT12_BAD) || \
                               ((type) == FAT16 && (cluster) == FAT16_BAD) || \
                               ((type) == FAT32 && (cluster) == FAT32_BAD))
#define IS_END(cluster, type) (((type) == FAT12 && (cluster) > FAT12_BAD) || \
                               ((type) == FAT16 && (cluster) > FAT16_BAD) || \
                               ((type) == FAT32 && (cluster) > FAT32_BAD))

#define FAT_OFFSET(info) ((info)->reserved_sectors * (info)->sector_size)
#define ROOT_OFFSET(info) (((info)->reserved_sectors + (info)->fat_count * (info)->fat_sectors) * (info)->sector_size)
#define DATA_OFFSET(info) (((info)->reserved_sectors + (info)->fat_count * (info)->fat_sectors + (((info)->type == FAT32 ? 0 : (info)->root_dir.entry_count * sizeof(directory_entry_t) + (info)->sector_size - 1) / (info)->sector_size)) * (info)->sector_size)

typedef struct {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sector_count16;
    uint8_t media;
    uint16_t fat_size16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sector_count;
    uint32_t total_sector_count32;
    union {
        struct {
            uint8_t drive_number;
            uint8_t rsv0;
            uint8_t boot_sig;
            uint32_t vol_id;
            uint8_t vol_label[11];
            uint8_t file_system_type[8];
        } ext16;
        struct {
            uint32_t fat_size32;
            uint16_t ext_flags;
            uint16_t fs_version;
            uint32_t root_cluster;
            uint16_t fs_info;
            uint16_t backup_boot_sector;
            uint8_t rsv1[12];
            uint8_t drive_number;
            uint8_t rsv0;
            uint8_t boot_sig;
            uint32_t vol_id;
            uint8_t vol_label[11];
            uint8_t file_system_type[8];
        } ext32;
    };
} __attribute((packed)) bpb_t;

typedef struct {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t rsv0;
    uint8_t creation_time_decisecond;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_accessed_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) directory_entry_t;

inline static uint32_t next_cluster(fat_info_t *info, uint32_t cluster) {
    uint64_t cluster_addr = 0;
    switch(info->type) {
        case FAT12: // FAT12 is cursed, wont optimize it cuz clusters can be across sector boundaries...
            uint32_t next_cluster = 0;
            disk_read(info->partition, FAT_OFFSET(info) + (cluster + cluster / 2), sizeof(uint16_t), &next_cluster);
            if(cluster % 2 != 0) next_cluster >>= 4;
            next_cluster &= 0xFFF;
            return next_cluster;
        case FAT16:
            cluster_addr = FAT_OFFSET(info) + cluster * sizeof(uint16_t);
            break;
        case FAT32:
            cluster_addr = FAT_OFFSET(info) + cluster * sizeof(uint32_t);
            break;
    }
    uint64_t lba = cluster_addr / info->partition->disk->sector_size;
    uint64_t offset = cluster_addr % info->partition->disk->sector_size;
    if(info->cache_lba != lba) {
        info->cache_lba = lba;
        if(disk_read_sector(info->partition->disk, info->partition->lba + lba, 1, info->cache)) log_panic("FAT", "Failed to read sector from disk");
    }
    switch(info->type) {
        case FAT12:
            __builtin_unreachable();
        case FAT16:
            return *(uint16_t *) (uintptr_t) ((uintptr_t) info->cache + offset);
        case FAT32:
            return (*(uint32_t *) (uintptr_t) ((uintptr_t) info->cache + offset)) & 0x0FFF'FFFF;
    }
    __builtin_unreachable();
}

inline static fat_file_t *create_file(fat_info_t *info, directory_entry_t *entry) {
    fat_file_t *file = heap_alloc(sizeof(fat_file_t));
    file->info = info;
    file->is_dir = entry->attributes & ATTR_DIRECTORY;
    file->size = entry->file_size;
    file->cluster_number = entry->cluster_low;
    if(info->type == FAT32) file->cluster_number |= entry->cluster_high << 16;
    return file;
}

inline static fat_file_t *internal_dir_lookup(fat_info_t *info, uint32_t cluster, const char *name) {
    directory_entry_t *entries = heap_alloc(info->cluster_size);
    while(!IS_END(cluster, info->type)) {
        if(IS_BAD(cluster, info->type)) log_panic("FAT", "Bad cluster");
        disk_read(info->partition, DATA_OFFSET(info) + (cluster - 2) * info->cluster_size, info->cluster_size, entries);
        for(uint16_t i = 0; i < info->cluster_size / sizeof(directory_entry_t); i++) {
            if(IS_DIR_FREE(entries[i].name[0])) continue;
            if(entries[i].attributes == 0xF) continue; // Ignore long names
            if(memcmp(entries[i].name, name, 11) != 0) continue;
            fat_file_t *file = create_file(info, &entries[i]);
            heap_free(entries);
            return file;
        }
        cluster = next_cluster(info, cluster);
    }
    heap_free(entries);
    return 0;
}

fat_info_t *fat_initialize(disk_part_t *partition) {
    bpb_t *bpb = heap_alloc(sizeof(bpb_t));
    disk_read(partition, 0, sizeof(bpb_t), bpb);
    if(bpb->jmp_boot[0] != 0xE9 && bpb->jmp_boot[0] != 0xEB && bpb->jmp_boot[0] != 0x49) goto invalid;

    for(int i = 128; i <= 4096; i *= 2) if(bpb->bytes_per_sector == i) goto valid_sector_size;
    goto invalid;
    valid_sector_size:

    for(int i = 1; i <= 128; i *= 2) if(bpb->sectors_per_cluster == i) goto valid_cluster_size;
    goto invalid;
    valid_cluster_size:

    if(
        bpb->media != 0xF0 &&
        bpb->media != 0xF8 &&
        bpb->media != 0xF9 &&
        bpb->media != 0xFA &&
        bpb->media != 0xFB &&
        bpb->media != 0xFC &&
        bpb->media != 0xFD &&
        bpb->media != 0xFE &&
        bpb->media != 0xFF
    ) goto invalid;

    if(bpb->reserved_sector_count == 0) goto invalid;
    if(bpb->fat_count == 0) goto invalid;
    if(bpb->total_sector_count16 == 0 && bpb->total_sector_count32 == 0) goto invalid;

    uint32_t root_dir_sectors = ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;
    uint32_t fat_size = bpb->fat_size16;
    if(fat_size == 0) fat_size = bpb->ext32.fat_size32;
    uint32_t total_sectors = bpb->total_sector_count16;
    if(total_sectors == 0) total_sectors = bpb->total_sector_count32;
    uint32_t data_sectors = total_sectors - (bpb->reserved_sector_count + (bpb->fat_count * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / bpb->sectors_per_cluster;

    fat_type_t type = FAT32;
    if(cluster_count <= 4084) {
        type = FAT12;
    } else if(cluster_count <= 65525) {
        type = FAT16;
    }

    if((type == FAT12 || type == FAT16) && bpb->fat_size16 == 0) goto invalid;
    if(type == FAT32 && bpb->ext32.fat_size32 == 0) goto invalid;
    if(type == FAT32 && bpb->ext32.fs_version > 0) goto invalid;

    fat_info_t *info = heap_alloc(sizeof(fat_info_t));
    info->partition = partition;
    info->cache = heap_alloc(partition->disk->sector_size);
    info->cache_lba = 0;
    info->type = type;
    info->reserved_sectors = bpb->reserved_sector_count;
    info->fat_count = bpb->fat_count;
    info->sector_size = bpb->bytes_per_sector;
    info->cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    switch(type) {
        case FAT12:
        case FAT16:
            info->fat_sectors = bpb->fat_size16;
            info->root_dir.entry_count = bpb->root_entry_count;
            break;
        case FAT32:
            info->fat_sectors = bpb->ext32.fat_size32;
            info->root_dir.cluster_number = bpb->ext32.root_cluster;
            break;
    }
    heap_free(bpb);
    return info;

    invalid:
    heap_free(bpb);
    return 0;
}

fat_file_t *fat_root_lookup(fat_info_t *info, const char *name) {
    switch(info->type) {
        case FAT12:
        case FAT16:
            directory_entry_t *entry = heap_alloc(sizeof(directory_entry_t));
            for(uint16_t i = 0; i < info->root_dir.entry_count; i++) {
                disk_read(info->partition, ROOT_OFFSET(info) + (i * sizeof(directory_entry_t)), sizeof(directory_entry_t), entry);
                if(IS_DIR_FREE(entry->name[0])) continue;
                if(entry->attributes == 0xF) continue; // Ignore long names
                if(memcmp(entry->name, name, 11) != 0) continue;
                fat_file_t *file = create_file(info, entry);
                heap_free(entry);
                return file;
            }
            heap_free(entry);
            break;
        case FAT32: return internal_dir_lookup(info, info->root_dir.cluster_number, name);
    }
    return 0;
}

fat_file_t *fat_dir_lookup(fat_file_t *dir, const char *name) {
    if(!dir->is_dir) return 0;
    return internal_dir_lookup(dir->info, dir->cluster_number, name);
}

uint64_t fat_read(fat_file_t *file, uint64_t offset, uint64_t count, void *dest) {
    if((!file->is_dir && file->size < offset + count) || file->cluster_number == 0) return 0;
    uint64_t initial_count = count;

    uint32_t cluster = file->cluster_number;
    for(uint64_t i = 0; i < offset / file->info->cluster_size; i++) {
        if(IS_BAD(cluster, file->info->type)) log_panic("FAT", "Bad cluster");
        if(IS_END(cluster, file->info->type)) return 0;
        cluster = next_cluster(file->info, cluster);
    }
    offset %= file->info->cluster_size;

    while(!IS_END(cluster, file->info->type) && count > 0) {
        uint32_t streak_start = cluster;
        size_t streak_count = 0;
        while(cluster == streak_start + streak_count && !IS_END(cluster, file->info->type)) {
            // TODO: this can be optimized to not look for fot streaks longer than needed
            if(IS_BAD(cluster, file->info->type)) log_panic("FAT", "Bad cluster");
            streak_count++;
            cluster = next_cluster(file->info, cluster);
        }
        uint64_t actual_read_count = streak_count * file->info->cluster_size - offset;
        if(actual_read_count > count) actual_read_count = count;
        disk_read(file->info->partition, DATA_OFFSET(file->info) + (streak_start - 2) * file->info->cluster_size + offset, actual_read_count, dest);
        count -= actual_read_count;
        dest += actual_read_count;
        offset = 0;
    }

    return initial_count - count;
}