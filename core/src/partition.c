#include "partition.h"
#include <log.h>
#include <libc.h>
#include <memory/pmm.h>

#define GPT_PROTECTIVE 0xEE

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

static void gpt_partitions(disk_t *disk, gpt_header_t *header) {
    uint32_t array_sectors = (header->partition_array_count * header->partition_entry_size + disk->sector_size - 1) / disk->sector_size;
    uint32_t buf_size = (array_sectors * disk->sector_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *buf = pmm_alloc_pages(buf_size, PMM_AREA_CONVENTIONAL);
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
            log(">> Partition %i.%i { Start: %i, End: %i }\n", (uint64_t) disk->drive_number, (uint64_t) i, entry->start_lba, entry->end_lba);
        }
    } else {
        log_warning("PARTITION", "Ignoring drive %x. Read failed.", (uint64_t) disk->drive_number);
    }
    // TODO: Free buf
}

void partition_get(disk_t *disk) {
    int buf_size = (disk->sector_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *buf = pmm_alloc_pages(buf_size, PMM_AREA_CONVENTIONAL);

    if(!disk_read_sector(disk, 0, 1, buf)) {
        mbr_t *mbr = (mbr_t *) ((uintptr_t) buf + 440);
        if(mbr->entries[0].type == GPT_PROTECTIVE) {
            disk_read_sector(disk, mbr->entries[0].start_lba, 1, buf);
            gpt_partitions(disk, (gpt_header_t *) buf);
        } else {
            log_warning("PARTITION", "Ignoring drive %x because it is partitioned with a legacy MBR.", (uint64_t) disk->drive_number);
        }
    } else {
        log_warning("PARTITION", "Ignoring drive %x. Read failed.", (uint64_t) disk->drive_number);
    }
    // TODO: Free buf
}