#include "disk.h"
#include <lib/mem.h>
#include <lib/math.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#include <sys/tsc.x86_64.h>
#include <sys/int.x86_64.bios.h>

#define EFLAGS_CF (1 << 0)

typedef struct {
    uint8_t size;
    uint8_t rsv0;
    uint16_t sector_count;
    uint16_t memory_offset;
    uint16_t memory_segment;
    uint64_t disk_lba;
} __attribute__((packed)) disk_address_packet_t;

typedef struct {
    uint16_t size;
    uint16_t info_flags;
    uint32_t phys_cylinders;
    uint32_t phys_heads;
    uint32_t phys_sectors_per_track;
    uint64_t abs_sectors;
    uint16_t sector_size;
    uint32_t edd_address;
} __attribute__((packed)) ext_read_drive_params_t;

static uint16_t estimate_sector_size(uint8_t disk_id, uint8_t test_val) {
    uint8_t *buf = pmm_alloc(PMM_AREA_CONVENTIONAL, 3);
    memset(buf, test_val, PMM_PAGE_SIZE * 3);

    disk_address_packet_t dap = {
        .size = sizeof(disk_address_packet_t),
        .sector_count = 1,
        .memory_segment = INT_16BIT_SEGMENT(buf),
        .memory_offset = INT_16BIT_OFFSET(buf),
        .disk_lba = 0
    };
    int_regs_t regs = {
        .eax = (0x42 << 8),
        .edx = disk_id,
        .ds = INT_16BIT_SEGMENT(&dap),
        .esi = INT_16BIT_OFFSET(&dap)
    };
    int_exec(0x13, &regs);
    if(regs.eflags & EFLAGS_CF) {
        pmm_free(buf, 3);
        return 0;
    }
    int size = 0;
    for(int i = 0; i < PMM_PAGE_SIZE * 3; i++) if(buf[i] != test_val) size = i;
    pmm_free(buf, 3);
    return size + 1;
}

static uint16_t estimate_optimal_transfer_size(disk_t *disk) {
    uint8_t *buf = pmm_alloc(PMM_AREA_CONVENTIONAL, 32);
    memset(buf, 0, PMM_PAGE_SIZE * 32);

    static const size_t transfer_sizes[] = { 1, 2, 4, 8, 16, 24, 32, 48, 64 };

    uint64_t fastest_transfer = UINT64_MAX;
    size_t fastest_size = transfer_sizes[0];
    for(size_t i = 0; i < sizeof(transfer_sizes) / sizeof(size_t); i++) {
        if(transfer_sizes[i] * disk->sector_size > PMM_PAGE_SIZE * 32) break;

        disk_address_packet_t dap = {
            .size = sizeof(disk_address_packet_t),
            .sector_count = transfer_sizes[i],
            .memory_segment = INT_16BIT_SEGMENT(buf),
            .memory_offset = INT_16BIT_OFFSET(buf),
            .disk_lba = 0
        };

        uint64_t start_time = tsc_read();
        for(size_t j = 0; j < (PMM_PAGE_SIZE * 32) / disk->sector_size; j += transfer_sizes[i]) {
            int_regs_t regs = {
                .eax = (0x42 << 8),
                .edx = disk->id,
                .ds = INT_16BIT_SEGMENT(&dap),
                .esi = INT_16BIT_OFFSET(&dap)
            };
            int_exec(0x13, &regs);
            if(regs.eflags & EFLAGS_CF) {
                pmm_free(buf, 32);
                return fastest_size;
            }
            dap.disk_lba += transfer_sizes[i];
        }
        uint64_t transfer_time = tsc_read() - start_time;

        if(transfer_time < fastest_transfer) {
            fastest_transfer = transfer_time;
            fastest_size = transfer_sizes[i];
        }
    }

    pmm_free(buf, 32);
    return fastest_size;
}

void disk_initialize() {
    for(int i = 0x80; i < 0xFF; i++) {
        ext_read_drive_params_t params = { .size = sizeof(ext_read_drive_params_t) };
        int_regs_t regs = {
            .eax = (0x48 << 8),
            .edx = i,
            .ds = INT_16BIT_SEGMENT(&params),
            .esi = INT_16BIT_OFFSET(&params)
        };
        int_exec(0x13, &regs);
        if(regs.eflags & EFLAGS_CF) continue;

        uint16_t first_estimation = estimate_sector_size(i, 0);
        uint16_t second_estimation = estimate_sector_size(i, 123);
        uint16_t calculated_sector_size = first_estimation > second_estimation ? first_estimation : second_estimation;

        disk_t *disk = heap_alloc(sizeof(disk_t));
        disk->id = i;
        disk->sector_size = calculated_sector_size != 0 ? calculated_sector_size : params.sector_size;
        disk->sector_count = params.abs_sectors;
        disk->optimal_transfer_size = 1;

        int buf_size = MATH_DIV_CEIL(disk->sector_size, PMM_PAGE_SIZE);
        void *buf = pmm_alloc(PMM_AREA_CONVENTIONAL, buf_size);
        if(buf_size == 0 || disk_read_sector(disk, 0, buf_size, buf)) {
            heap_free(disk);
            continue;
        }
        disk->writable = !disk_write_sector(disk, 0, buf_size, buf);
        disk->partitions = 0;
        disk_initialize_partitions(disk);
        disk->optimal_transfer_size = estimate_optimal_transfer_size(disk);
        disk->next = g_disks;
        g_disks = disk;

        pmm_free(buf, buf_size);
    }
}

bool disk_read_sector(disk_t *disk, uint64_t lba, uint64_t sector_count, void *dest) {
    disk_address_packet_t dap = {
        .size = sizeof(disk_address_packet_t)
    };
    int_regs_t regs = {
        .edx = disk->id,
        .ds = INT_16BIT_SEGMENT(&dap),
        .esi = INT_16BIT_OFFSET(&dap)
    };
    size_t buf_size = MATH_DIV_CEIL(disk->optimal_transfer_size * disk->sector_size, PMM_PAGE_SIZE);
    void *buf = pmm_alloc(PMM_AREA_CONVENTIONAL, buf_size);
    for(uint64_t i = 0; i < sector_count; i += disk->optimal_transfer_size) {
        dap.disk_lba = lba;
        uint64_t tmp_sec_count = sector_count - i;
        if(tmp_sec_count > UINT16_MAX) tmp_sec_count = UINT16_MAX;
        if(tmp_sec_count > disk->optimal_transfer_size) tmp_sec_count = disk->optimal_transfer_size;
        dap.sector_count = tmp_sec_count;
        dap.memory_segment = INT_16BIT_SEGMENT(buf);
        dap.memory_offset = INT_16BIT_OFFSET(buf);
        regs.eax = (0x42 << 8);
        int_exec(0x13, &regs);
        if(regs.eflags & EFLAGS_CF) return true;
        lba += disk->optimal_transfer_size;

        memcpy(dest, buf, dap.sector_count * disk->sector_size);
        dest += disk->sector_size * disk->optimal_transfer_size;
    }
    pmm_free(buf, buf_size);
    return false;
}

bool disk_write_sector(disk_t *disk, uint64_t lba, uint64_t sector_count, void *src) {
    disk_address_packet_t dap = {
        .size = sizeof(disk_address_packet_t),
        .sector_count = 1,
    };
    int_regs_t regs = {
        .edx = disk->id,
        .ds = INT_16BIT_SEGMENT(&dap),
        .esi = INT_16BIT_OFFSET(&dap)
    };
    for(uint64_t i = 0; i < sector_count; i++) {
        dap.disk_lba = lba;
        dap.memory_segment = INT_16BIT_SEGMENT(src);
        dap.memory_offset = INT_16BIT_OFFSET(src);
        regs.eax = (0x43 << 8) | 1;
        int_exec(0x13, &regs);
        if(regs.eflags & EFLAGS_CF) return true;
        lba++;
        src += disk->sector_size;
    }
    return false;
}