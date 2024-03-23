#include "disk.h"
#include <common/log.h>
#include <lib/math.h>
#include <lib/mem.h>
#include <memory/pmm.h>
#include <memory/heap.h>

#define GPT_TYPE_PROTECTIVE 0xEE

disk_t *g_disks = NULL;

typedef struct {
    uint8_t boot_indicator;
    uint8_t start_chs[3];
    uint8_t type;
    uint8_t end_chs[3];
    uint32_t start_lba;
    uint32_t end_lba;
} __attribute__((packed)) mbr_entry_t;

typedef struct {
    uint32_t disk_signature;
    uint16_t rsv0;
    mbr_entry_t entries[4];
} mbr_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t rsv0;
    uint64_t this_lba;
    uint64_t alt_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_array_lba;
    uint32_t partition_array_count;
    uint32_t partition_entry_size;
    uint32_t partition_array_crc32;
} gpt_header_t;

typedef struct {
    uint8_t type_guid[16];
    uint8_t id_guid[16];
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attributes;
    uint8_t name[72];
} gpt_entry_t;

static void initialize_gpt_partitions(disk_t *disk, gpt_header_t *header) {
    uint32_t array_sectors = MATH_DIV_CEIL(header->partition_array_count * header->partition_entry_size, disk->sector_size);
    uint32_t buf_size = MATH_DIV_CEIL(array_sectors * disk->sector_size, PMM_PAGE_SIZE);
    void *buf = pmm_alloc(PMM_AREA_CONVENTIONAL, buf_size);
    if(!disk_read_sector(disk, header->partition_array_lba, array_sectors, buf)) {
        for(uint32_t i = 0; i < header->partition_array_count; i++) {
            gpt_entry_t *entry = (gpt_entry_t *) ((uintptr_t) buf + i * header->partition_entry_size);
            bool is_empty = true;
            for(int j = 0; j < 16; j++) {
                if(entry->type_guid[j] == 0) continue;
                is_empty = false;
                break;
            }
            if(is_empty) continue;
            disk_part_t *partition = heap_alloc(sizeof(disk_part_t));
            partition->id = i;
            partition->disk = disk;
            partition->lba = entry->start_lba;
            partition->size = entry->end_lba - entry->start_lba;
            partition->next = disk->partitions;
            disk->partitions = partition;
        }
    } else {
        log_warning("PARTITION", "Ignoring drive %#llx. Read failed", (uint64_t) disk->id);
    }
    pmm_free(buf, buf_size);
}

void disk_initialize_partitions(disk_t *disk) {
    int buf_size = MATH_DIV_CEIL(disk->sector_size, PMM_PAGE_SIZE);
    void *buf = pmm_alloc(PMM_AREA_CONVENTIONAL, buf_size);

    if(!disk_read_sector(disk, 0, 1, buf)) {
        mbr_t *mbr = (mbr_t *) ((uintptr_t) buf + 440);
        if(mbr->entries[0].type == GPT_TYPE_PROTECTIVE) {
            disk_read_sector(disk, mbr->entries[0].start_lba, 1, buf);
            initialize_gpt_partitions(disk, (gpt_header_t *) buf);
        } else {
            log_warning("PARTITION", "Ignoring drive %#llx because it is partitioned with a legacy MBR", (uint64_t) disk->id);
        }
    } else {
        log_warning("PARTITION", "Ignoring drive %#llx. Read failed", (uint64_t) disk->id);
    }
    pmm_free(buf, buf_size);
}

void disk_read(disk_part_t *part, uint64_t offset, uint64_t count, void *dest) {
    uint64_t lba_offset = offset / part->disk->sector_size;
    uint64_t sect_offset = offset % part->disk->sector_size;
    uint64_t sect_count = MATH_DIV_CEIL(sect_offset + count, part->disk->sector_size);

    uint64_t buf_size = MATH_DIV_CEIL(part->disk->sector_size * sect_count, PMM_PAGE_SIZE);
    void *buf = pmm_alloc(PMM_AREA_MAX, buf_size);

    if(disk_read_sector(part->disk, part->lba + lba_offset, sect_count, buf)) log_panic("DISK", "Read failed");
    memcpy(dest, (void *) (buf + sect_offset), count);

    pmm_free(buf, buf_size);
}