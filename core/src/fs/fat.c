#include "fat.h"
#include <stdint.h>
#include <common/log.h>
#include <lib/str.h>
#include <lib/mem.h>
#include <lib/math.h>
#include <memory/heap.h>

#define FS_DATA(VFS) ((fs_data_t *) (VFS)->data)
#define NODE_DATA(NODE) ((node_data_t *) (NODE)->data)

#define FAT_OFFSET(FS_DATA) ((FS_DATA)->fat_meta.reserved_sectors * (FS_DATA)->fat_meta.sector_size)
#define ROOT_OFFSET(FS_DATA) (((FS_DATA)->fat_meta.reserved_sectors + (FS_DATA)->fat_meta.fat_count * (FS_DATA)->fat_meta.fat_sectors) * (FS_DATA)->fat_meta.sector_size)
#define DATA_OFFSET(FS_DATA) (ROOT_OFFSET(FS_DATA) + ((FS_DATA)->fat_meta.type == FAT_TYPE_32 ? 0 : MATH_DIV_CEIL((FS_DATA)->fat_meta.root_dir_entry_count * sizeof(directory_entry_t), (FS_DATA)->fat_meta.sector_size)))

#define CLUSTER_BAD_FAT12 0xFF7
#define CLUSTER_BAD_FAT16 0xFFF7
#define CLUSTER_BAD_FAT32 0xFFFFFF7
#define CLUSTER_IS_BAD(CLUSTER, TYPE) (((CLUSTER) < 2)              \
    || ((TYPE) == FAT_TYPE_12 && (CLUSTER) == CLUSTER_BAD_FAT12)    \
    || ((TYPE) == FAT_TYPE_16 && (CLUSTER) == CLUSTER_BAD_FAT16)    \
    || ((TYPE) == FAT_TYPE_32 && (CLUSTER) == CLUSTER_BAD_FAT32))
#define CLUSTER_IS_END(CLUSTER, TYPE)                               \
    (((TYPE) == FAT_TYPE_12 && (CLUSTER) > CLUSTER_BAD_FAT12)       \
    || ((TYPE) == FAT_TYPE_16 && (CLUSTER) > CLUSTER_BAD_FAT16)     \
    || ((TYPE) == FAT_TYPE_32 && (CLUSTER) > CLUSTER_BAD_FAT32))

#define DIR_ENTRY_ATTR_READ_ONLY (1 << 0)
#define DIR_ENTRY_ATTR_HIDDEN (1 << 1)
#define DIR_ENTRY_ATTR_SYSTEM (1 << 2)
#define DIR_ENTRY_ATTR_VOLUME_ID (1 << 3)
#define DIR_ENTRY_ATTR_DIRECTORY (1 << 4)
#define DIR_ENTRY_ATTR_ARCHIVE (1 << 5)
#define DIR_ENTRY_ATTR_LFN_MASK (DIR_ENTRY_ATTR_READ_ONLY | DIR_ENTRY_ATTR_HIDDEN | DIR_ENTRY_ATTR_SYSTEM | DIR_ENTRY_ATTR_VOLUME_ID)
#define DIR_ENTRY_IS_FREE(DIR_ENTRY) ((DIR_ENTRY)->name[0] == 0xE5 || (DIR_ENTRY)->name[0] == 0)
#define DIR_ENTRY_IS_LONG_NAME(DIR_ENTRY) ((DIR_ENTRY)->attributes == 0xF)
#define DIR_ENTRY_IS_DIRECTORY(DIR_ENTRY) ((DIR_ENTRY)->attributes & DIR_ENTRY_ATTR_DIRECTORY)

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

typedef struct {
    uint8_t order;
    uint8_t name1[10];
    uint8_t attributes;
    uint8_t type;
    uint8_t checksum;
    uint8_t name2[12];
    uint16_t cluster_low;
    uint8_t name3[4];
} __attribute__((packed)) lfn_directory_entry_t;

typedef struct {
    disk_part_t *partition;
    void *cache;
    uint64_t cache_lba;
    struct {
        fat_type_t type;
        uint16_t reserved_sectors;
        uint8_t fat_count;
        uint32_t fat_sectors;
        uint16_t sector_size;
        uint16_t cluster_size;
        uint16_t root_dir_entry_count; /* Only relevant for FAT12/16 */
    } fat_meta;
} fs_data_t;

typedef enum {
    NODE_TYPE_ROOT,
    NODE_TYPE_DIR,
    NODE_TYPE_FILE
} node_type_t;

typedef struct {
    node_type_t type;
    uint32_t cluster;
    uint32_t file_size;
} node_data_t;

static vfs_node_ops_t g_node_ops;

static uint32_t next_cluster(fs_data_t *fs_data, uint32_t cluster) {
    uint64_t cluster_addr = 0;
    switch(fs_data->fat_meta.type) {
        case FAT_TYPE_12: // FAT12 is cursed, wont optimize it cuz clusters can exist across sector boundaries...
            uint32_t next_cluster = 0;
            disk_read(fs_data->partition, FAT_OFFSET(fs_data) + (cluster + cluster / 2), sizeof(uint16_t), &next_cluster);
            if(cluster % 2 != 0) next_cluster >>= 4;
            next_cluster &= 0xFFF;
            return next_cluster;
        case FAT_TYPE_16:
            cluster_addr = FAT_OFFSET(fs_data) + cluster * sizeof(uint16_t);
            break;
        case FAT_TYPE_32:
            cluster_addr = FAT_OFFSET(fs_data) + cluster * sizeof(uint32_t);
            break;
    }
    uint64_t lba = cluster_addr / fs_data->partition->disk->sector_size;
    uint64_t offset = cluster_addr % fs_data->partition->disk->sector_size;
    if(fs_data->cache_lba != lba) {
        fs_data->cache_lba = lba;
        if(disk_read_sector(fs_data->partition->disk, fs_data->partition->lba + lba, 1, fs_data->cache)) log_panic("FAT", "Failed to read sector from disk");
    }
    switch(fs_data->fat_meta.type) {
        case FAT_TYPE_12:
            __builtin_unreachable();
        case FAT_TYPE_16:
            return *(uint16_t *) (uintptr_t) ((uintptr_t) fs_data->cache + offset);
        case FAT_TYPE_32:
            return (*(uint32_t *) (uintptr_t) ((uintptr_t) fs_data->cache + offset)) & 0x0FFF'FFFF;
    }
    __builtin_unreachable();
}

static vfs_node_t *create_node(vfs_t *vfs, node_type_t type, uint32_t cluster, uint32_t file_size) {
    node_data_t *node_data = heap_alloc(sizeof(node_data_t));
    node_data->type = type;
    node_data->cluster = cluster;
    node_data->file_size = file_size;

    vfs_node_t *node = heap_alloc(sizeof(vfs_node_t));
    node->vfs = vfs;
    node->ops = &g_node_ops;
    node->data = node_data;
    return node;
}

static bool name_to_8_3(char *src, char dest[11]) {
    bool ext = false;
    int j = 0;
    for(int i = 0; src[i] != 0; i++) {
        if(src[i] == '.') {
            if(ext) return false;
            ext = true;
            for(; j < 8; j++) dest[j] = ' ';
            continue;
        }
        if(j >= 11 || (!ext && j >= 8)) return false;
        dest[j++] = (src[i] >= 'a' && src[i] <= 'z') ? src[i] - 0x20 : src[i];
    }
    for(; j < 11; j++) dest[j] = ' ';
    return true;
}

static bool cmp_dir(directory_entry_t *entry, char *name) {
    if(DIR_ENTRY_IS_FREE(entry)) return false;
    if(DIR_ENTRY_IS_LONG_NAME(entry)) return false; // TODO: Dont ignore long names

    char sfn[11];
    if(!name_to_8_3(name, sfn)) return false;
    return memcmp(entry->name, sfn, 11) == 0;
}

static vfs_node_t *node_lookup(vfs_node_t *node, char *name) {
    if(NODE_DATA(node)->type != NODE_TYPE_DIR && NODE_DATA(node)->type != NODE_TYPE_ROOT) return NULL;

    if(NODE_DATA(node)->type == NODE_TYPE_ROOT && (FS_DATA(node->vfs)->fat_meta.type == FAT_TYPE_12 || FS_DATA(node->vfs)->fat_meta.type == FAT_TYPE_16)) {
        directory_entry_t entry;
        for(uint16_t i = 0; i < FS_DATA(node->vfs)->fat_meta.root_dir_entry_count; i++) {
            disk_read(FS_DATA(node->vfs)->partition, ROOT_OFFSET(FS_DATA(node->vfs)) + (i * sizeof(directory_entry_t)), sizeof(directory_entry_t), &entry);
            if(entry.name[0] == 0) break;
            if(!cmp_dir(&entry, name)) continue;
            return create_node(node->vfs, DIR_ENTRY_IS_DIRECTORY(&entry) ? NODE_TYPE_DIR : NODE_TYPE_FILE, entry.cluster_low, entry.file_size);
        }
        return NULL;
    }

    directory_entry_t *entries = heap_alloc(FS_DATA(node->vfs)->fat_meta.cluster_size);
    uint32_t cluster = NODE_DATA(node)->cluster;
    while(!CLUSTER_IS_END(cluster, FS_DATA(node->vfs)->fat_meta.type)) {
        if(CLUSTER_IS_BAD(cluster, FS_DATA(node->vfs)->fat_meta.type)) log_panic("FAT", "Bad cluster");
        disk_read(FS_DATA(node->vfs)->partition, DATA_OFFSET(FS_DATA(node->vfs)) + (cluster - 2) * FS_DATA(node->vfs)->fat_meta.cluster_size, FS_DATA(node->vfs)->fat_meta.cluster_size, entries);
        for(unsigned int i = 0; i < FS_DATA(node->vfs)->fat_meta.cluster_size / sizeof(directory_entry_t); i++) {
            if(entries[i].name[0] == 0) goto exit;
            if(!cmp_dir(&entries[i], name)) continue;
            uint32_t entry_cluster = entries[i].cluster_low;
            if(FS_DATA(node->vfs)->fat_meta.type == FAT_TYPE_32) entry_cluster |= (entries[i].cluster_high << 16);
            return create_node(node->vfs, DIR_ENTRY_IS_DIRECTORY(&entries[i]) ? NODE_TYPE_DIR : NODE_TYPE_FILE, entry_cluster, entries[i].file_size);
        }
        cluster = next_cluster(FS_DATA(node->vfs), cluster);
    }
    exit:
    heap_free(entries);
    return NULL;
}

static size_t node_read(vfs_node_t *node, void *dest, size_t offset, size_t count) {
    if(NODE_DATA(node)->type != NODE_TYPE_FILE) return 0;

    size_t initial_count = count;
    size_t initial_offset = offset;

    uint32_t cluster = NODE_DATA(node)->cluster;
    for(unsigned int i = 0; i < offset / FS_DATA(node->vfs)->fat_meta.cluster_size; i++) {
        if(CLUSTER_IS_BAD(cluster, FS_DATA(node->vfs)->fat_meta.type)) log_panic("FAT", "Bad cluster");
        if(CLUSTER_IS_END(cluster, FS_DATA(node->vfs)->fat_meta.type)) return 0;
        cluster = next_cluster(FS_DATA(node->vfs), cluster);
    }
    offset %= FS_DATA(node->vfs)->fat_meta.cluster_size;

    while(!CLUSTER_IS_END(cluster, FS_DATA(node->vfs)->fat_meta.type) && count > 0) {
        uint32_t streak_start = cluster;
        size_t streak_size = 0;
        while(count > streak_size * FS_DATA(node->vfs)->fat_meta.cluster_size && cluster == streak_start + streak_size && !CLUSTER_IS_END(cluster, FS_DATA(node->vfs)->fat_meta.type)) {
            if(CLUSTER_IS_BAD(cluster, FS_DATA(node->vfs)->fat_meta.type)) log_panic("FAT", "Bad cluster");
            streak_size++;
            cluster = next_cluster(FS_DATA(node->vfs), cluster);
        }

        size_t read_count = streak_size * FS_DATA(node->vfs)->fat_meta.cluster_size - offset;
        if(read_count > count) read_count = count;
        if(initial_offset + read_count > NODE_DATA(node)->file_size) read_count = NODE_DATA(node)->file_size - initial_offset;
        disk_read(FS_DATA(node->vfs)->partition, DATA_OFFSET(FS_DATA(node->vfs)) + (streak_start - 2) * FS_DATA(node->vfs)->fat_meta.cluster_size + offset, read_count, dest);
        count -= read_count;
        dest += read_count;
        offset = 0;
    }

    return initial_count - count;
}

static void node_attr(vfs_node_t *node, vfs_attr_t *attr) {
    attr->size = NODE_DATA(node)->file_size;
}

static vfs_node_ops_t g_node_ops = {
    .lookup = node_lookup,
    .read = node_read,
    .attr = node_attr
};

vfs_t *fat_initialize(disk_part_t *partition) {
    bpb_t *bpb = heap_alloc(sizeof(bpb_t));
    disk_read(partition, 0, sizeof(bpb_t), bpb);

    // Ensure BPB values are sane
    if(bpb->jmp_boot[0] != 0xE9 && bpb->jmp_boot[0] != 0xEB && bpb->jmp_boot[0] != 0x49) goto invalid;
    if(bpb->media != 0xF0 && bpb->media < 0xF8) goto invalid;
    if(bpb->reserved_sector_count == 0) goto invalid;
    if(bpb->fat_count == 0) goto invalid;
    if(bpb->total_sector_count16 == 0 && bpb->total_sector_count32 == 0) goto invalid;

    for(int i = 128; i <= 4096; i *= 2) if(bpb->bytes_per_sector == i) goto valid_sector_size;
    goto invalid;
    valid_sector_size:

    for(int i = 1; i <= 128; i *= 2) if(bpb->sectors_per_cluster == i) goto valid_cluster_size;
    goto invalid;
    valid_cluster_size:

    // Calculate some values
    uint32_t root_dir_sectors = MATH_DIV_CEIL(bpb->root_entry_count * 32, bpb->bytes_per_sector);
    uint32_t fat_size = bpb->fat_size16 == 0 ? bpb->ext32.fat_size32 : bpb->fat_size16;
    uint32_t total_sectors = bpb->total_sector_count16 == 0 ? bpb->total_sector_count32 : bpb->total_sector_count16;
    uint32_t data_sectors = total_sectors - (bpb->reserved_sector_count + (bpb->fat_count * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / bpb->sectors_per_cluster;

    fat_type_t type = FAT_TYPE_32;
    if(cluster_count <= 65525) type = FAT_TYPE_16;
    if(cluster_count <= 4084) type = FAT_TYPE_12;

    if((type == FAT_TYPE_12 || type == FAT_TYPE_16) && bpb->fat_size16 == 0) goto invalid;
    if(type == FAT_TYPE_32 && bpb->ext32.fat_size32 == 0) goto invalid;
    if(type == FAT_TYPE_32 && bpb->ext32.fs_version > 0) goto invalid;

    // Create data structures
    fs_data_t *fs_data = heap_alloc(sizeof(fs_data_t));
    fs_data->partition = partition;
    fs_data->cache = heap_alloc(partition->disk->sector_size);
    fs_data->cache_lba = 0;
    fs_data->fat_meta.type = type;
    fs_data->fat_meta.reserved_sectors = bpb->reserved_sector_count;
    fs_data->fat_meta.fat_count = bpb->fat_count;
    fs_data->fat_meta.sector_size = bpb->bytes_per_sector;
    fs_data->fat_meta.cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    switch(type) {
        case FAT_TYPE_12:
        case FAT_TYPE_16:
            fs_data->fat_meta.fat_sectors = bpb->fat_size16;
            fs_data->fat_meta.root_dir_entry_count = bpb->root_entry_count;
            break;
        case FAT_TYPE_32:
            fs_data->fat_meta.fat_sectors = bpb->ext32.fat_size32;
            break;
    }
    uint32_t root_cluster = type == FAT_TYPE_32 ? bpb->ext32.root_cluster : 0;
    heap_free(bpb);

    vfs_t *vfs = heap_alloc(sizeof(vfs_t));
    vfs->data = (void *) fs_data;
    vfs->root = create_node(vfs, NODE_TYPE_ROOT, root_cluster, 0);
    return vfs;

    invalid:
    heap_free(bpb);
    return NULL;
}