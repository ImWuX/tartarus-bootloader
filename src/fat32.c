#include "fat32.h"
#include <pmm.h>
#include <vmm.h>
#include <disk.h>
#include <log.h>

#define SECTOR_SIZE 512

#define LAST_CLUSTER 0x0FFFFFF8
#define BAD_CLUSTER 0x0FFFFFF7
#define FREE_CLUSTER 0

static bios_param_block_ext_t *g_bpb;
static void *g_cluster_buffer;

static uint32_t next_cluster(uint32_t cluster_num) {
    disk_read(g_bpb->bios_param_block.reserved_sector_count + cluster_num / (SECTOR_SIZE / 4), 1, g_cluster_buffer);
    return (((uint32_t *) g_cluster_buffer)[cluster_num % (SECTOR_SIZE / 4)] & 0x0FFFFFFF);
}

static void read_cluster_to_buffer(uint32_t cluster_num) {
    uint32_t first_data_sector = g_bpb->bios_param_block.reserved_sector_count + (g_bpb->bios_param_block.fat_count * g_bpb->sectors_per_fat);
    disk_read((cluster_num - 2) * g_bpb->bios_param_block.sectors_per_cluster + first_data_sector, g_bpb->bios_param_block.sectors_per_cluster, g_cluster_buffer);
}

bool fread(file_descriptor_t *file_descriptor, uint64_t count, void *dest) {
    if(file_descriptor->seek_offset + count > file_descriptor->file_size) return true;
    uint64_t cluster_size = g_bpb->bios_param_block.sectors_per_cluster * SECTOR_SIZE;
    uint32_t cluster_num = file_descriptor->cluster_num;
    for(uint64_t i = 0; i < file_descriptor->seek_offset / cluster_size; i++) cluster_num = next_cluster(cluster_num);
    uint64_t seek_offset = file_descriptor->seek_offset % cluster_size;
    count += seek_offset;
    for(uint64_t i = 0; i < count / cluster_size; i++) {
        read_cluster_to_buffer(cluster_num);
        pmm_cpy(g_cluster_buffer + seek_offset, dest, cluster_size - seek_offset);
        dest += cluster_size - seek_offset;
        seek_offset = 0;
        cluster_num = next_cluster(cluster_num);
    }
    read_cluster_to_buffer(cluster_num);
    pmm_cpy(g_cluster_buffer + seek_offset, dest, count % cluster_size);
    file_descriptor->seek_offset += count;
    return false;
}

bool fseekto(file_descriptor_t *file_descriptor, uint64_t offset) {
    if(offset > file_descriptor->file_size) return true;
    file_descriptor->seek_offset = offset;
    return false;
}

bool fseek(file_descriptor_t *file_descriptor, int64_t count) {
    if((int64_t) file_descriptor->seek_offset - count < 0) return true;
    return fseekto(file_descriptor, file_descriptor->seek_offset + count);
}

dir_entry_t *read_directory(uint32_t cluster_num) {
    int current_page_entry_count = 0;
    dir_entry_t *dir_entry;
    dir_entry_t *last_dir_entry = 0;
    while(cluster_num < LAST_CLUSTER) {
        read_cluster_to_buffer(cluster_num);
        fat_directory_entry_t *entries = (fat_directory_entry_t *) g_cluster_buffer;
        for(int i = 0; i < g_bpb->bios_param_block.sectors_per_cluster * 16; i++) {
            fat_directory_entry_t entry = entries[i];
            if(entry.name[0] == 0x0) break;
            if(entry.name[0] == 0xE5) continue;
            if(entry.attributes == 0xF) continue; // Long name support

            if(current_page_entry_count == 0 || (current_page_entry_count + 1) * sizeof(dir_entry_t) >= 0x1000) {
                void* page = pmm_request_page();
                pmm_set(0, page, 0x1000);
                dir_entry = (dir_entry_t *) (uint32_t) page;
                current_page_entry_count = 0;
            }

            dir_entry->fd.seek_offset = 0;
            dir_entry->fd.cluster_num = ((uint32_t) entry.cluster_high) << 16;
            dir_entry->fd.cluster_num += entry.cluster_low;
            dir_entry->fd.file_size = entry.size;
            pmm_cpy(entry.name, dir_entry->fd.name, 11);
            dir_entry->last_entry = last_dir_entry;
            last_dir_entry = dir_entry;

            dir_entry++;
            current_page_entry_count++;
        }
        cluster_num = next_cluster(cluster_num);
    }
    return --dir_entry;
}

dir_entry_t *read_root_directory() {
    return read_directory(g_bpb->root_cluster_num);
}

void fat32_initialize() {
    void *bpb_page = pmm_request_page();

    if(disk_read(0, 1, bpb_page)) {
        log_panic("FS could not be initialized. BPB read failed.");
        return;
    }

    g_bpb = (bios_param_block_ext_t *) bpb_page;
    uint32_t pages_per_cluster = g_bpb->bios_param_block.sectors_per_cluster / 8 + (g_bpb->bios_param_block.sectors_per_cluster % 8 > 0 ? 1 : 0);
    g_cluster_buffer = pmm_request_linear_pages(pages_per_cluster);
}

