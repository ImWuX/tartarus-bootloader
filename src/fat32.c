#include "fat32.h"
#include <pmm.h>
#include <vmm.h>
#include <disk.h>
#include <log.h>

#define SECTOR_SIZE 512
#define ENTRY_CONST (SECTOR_SIZE / 4)

#define LAST_CLUSTER 0x0FFFFFF8
#define BAD_CLUSTER 0x0FFFFFF7
#define FREE_CLUSTER 0

static bios_param_block_ext_t *g_bpb;
static void *g_cluster_buffer;
static void *g_fat_buffer;

static uint32_t next_cluster(uint32_t cluster_num) {
    static uint64_t last_sector = 0;
    uint64_t sector = g_bpb->bios_param_block.reserved_sector_count + cluster_num / ENTRY_CONST;
    if(sector != last_sector) {
        disk_read(sector, 1, g_fat_buffer);
        last_sector = sector;
    }
    return (((uint32_t *) g_fat_buffer)[cluster_num % ENTRY_CONST] & 0x0FFFFFFF);
}

static void read_cluster_to_buffer(uint32_t cluster_num) {
    uint32_t first_data_sector = g_bpb->bios_param_block.reserved_sector_count + (g_bpb->bios_param_block.fat_count * g_bpb->sectors_per_fat);
    disk_read((cluster_num - 2) * g_bpb->bios_param_block.sectors_per_cluster + first_data_sector, g_bpb->bios_param_block.sectors_per_cluster, g_cluster_buffer);
}

bool fat32_read(uint32_t cluster_num, uint64_t seek, uint64_t count, void *dest) {
    if(cluster_num >= BAD_CLUSTER) return true;
    uint64_t cluster_size = g_bpb->bios_param_block.sectors_per_cluster * SECTOR_SIZE;
    for(uint64_t i = 0; i < seek / cluster_size; i++) {
        cluster_num = next_cluster(cluster_num);
        if(cluster_num >= BAD_CLUSTER) return true;
    }
    uint64_t seek_offset = seek % cluster_size;
    count += seek_offset;
    for(uint64_t i = 0; i < count / cluster_size; i++) {
        read_cluster_to_buffer(cluster_num);
        pmm_cpy(g_cluster_buffer + seek_offset, dest, cluster_size - seek_offset);
        dest += cluster_size - seek_offset;
        seek_offset = 0;
        cluster_num = next_cluster(cluster_num);
        if(cluster_num >= BAD_CLUSTER) return true;
    }
    read_cluster_to_buffer(cluster_num);
    pmm_cpy(g_cluster_buffer + seek_offset, dest, count % cluster_size);
    return false;
}

uint32_t fat32_root_find(uint8_t *name) {
    uint32_t cluster_num = g_bpb->root_cluster_num;
    while(cluster_num < BAD_CLUSTER) {
        read_cluster_to_buffer(cluster_num);
        fat_directory_entry_t *entries = (fat_directory_entry_t *) g_cluster_buffer;
        for(int i = 0; i < g_bpb->bios_param_block.sectors_per_cluster * ENTRY_CONST; i++) {
            fat_directory_entry_t entry = entries[i];
            if(entry.name[0] == 0x0) break;
            if(entry.name[0] == 0xE5) continue;
            if(entry.attributes == 0xF) continue; // TODO: Long name support

            bool match = true;
            for(int i = 0; i < 11; i++) {
                if(entry.name[i] != name[i]) {
                    match = false;
                    break;
                }
            }

            if(match) return (((uint32_t) entry.cluster_high) << 16) + entry.cluster_low;
        }
        cluster_num = next_cluster(cluster_num);
    }
    return 0;
}

void fat32_initialize() {
    void *bpb_page = pmm_request_page();

    if(disk_read(0, 1, bpb_page)) {
        log_panic("FS could not be initialized. BPB read failed.");
        return;
    }

    g_fat_buffer = pmm_request_page();
    g_bpb = (bios_param_block_ext_t *) bpb_page;
    uint32_t pages_per_cluster = g_bpb->bios_param_block.sectors_per_cluster / 8 + (g_bpb->bios_param_block.sectors_per_cluster % 8 > 0 ? 1 : 0);
    g_cluster_buffer = pmm_request_linear_pages(pages_per_cluster);
}

