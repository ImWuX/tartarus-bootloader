#include "disk.h"
#include <core.h>
#include <log.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#if defined __BIOS && defined __AMD64
#include <int.h>
#endif
#ifdef __UEFI
#include <efi.h>
#endif

disk_t *g_disks;

#if defined __BIOS && defined __AMD64
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

#define EFLAGS_CF (1 << 0)

void disk_initialize() {
    for(int i = 0x80; i < 0xFF; i++) {
        ext_read_drive_params_t params = { .size = sizeof(ext_read_drive_params_t) };
        int_regs_t regs = {
            .eax = (0x48 << 8),
            .edx = i,
            .ds = int_16bit_segment(&params),
            .esi = int_16bit_offset(&params)
        };
        int_exec(0x13, &regs);
        if(regs.eflags & EFLAGS_CF) continue;

        disk_t *disk = heap_alloc(sizeof(disk_t));

        disk->drive_number = i;
        disk->sector_size = params.sector_size; // TODO: The sector size provided by BIOS is unreliable. We can manually figure out the size by reading into a filled buffer and empty buffer and figuring out where the last modified byte is.
        disk->sector_count = params.abs_sectors;

        int buf_pages = (disk->sector_size + PAGE_SIZE - 1) / PAGE_SIZE;
        void *buf = pmm_alloc_pages(buf_pages, PMM_AREA_CONVENTIONAL);
        if(disk_read_sector(disk, 0, buf_pages, buf)) {
            heap_free(disk);
            continue;
        }
        disk->writable = !disk_write_sector(disk, 0, buf_pages, buf);
        // TODO: Free buf
        disk->next = g_disks;
        g_disks = disk;
    }
}

bool disk_read_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *dest) {
    disk_address_packet_t dap = {
        .size = sizeof(disk_address_packet_t),
        .sector_count = 1,
    };
    int_regs_t regs = {
        .edx = disk->drive_number,
        .ds = int_16bit_segment(&dap),
        .esi = int_16bit_offset(&dap)
    };
    for(uint16_t i = 0; i < sector_count; i++) {
        dap.disk_lba = lba;
        dap.memory_segment = int_16bit_segment(dest);
        dap.memory_offset = int_16bit_offset(dest);
        regs.eax = (0x42 << 8);
        int_exec(0x13, &regs);
        if(regs.eflags & EFLAGS_CF) return true;
        lba++;
        dest += disk->sector_size;
    }
    return false;
}

bool disk_write_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *src) {
    // TODO: Rewrite like the read to write sectors one-by-one
    disk_address_packet_t dap = {
        .size = sizeof(disk_address_packet_t),
        .sector_count = sector_count,
        .disk_lba = lba,
        .memory_segment = int_16bit_segment(src),
        .memory_offset = int_16bit_offset(src)
    };
    int_regs_t regs = {
        .eax = (0x43 << 8) | 1,
        .edx = disk->drive_number,
        .ds = int_16bit_segment(&dap),
        .esi = int_16bit_offset(&dap)
    };
    int_exec(0x13, &regs);
    return regs.eflags & EFLAGS_CF;
}
#endif

#ifdef __UEFI
void disk_initialize() {
    UINTN buffer_size = 0;
    EFI_HANDLE *buffer = NULL;
    EFI_GUID guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_STATUS status = g_st->BootServices->LocateHandle(ByProtocol, &guid, NULL, &buffer_size, buffer);
    if(status == EFI_BUFFER_TOO_SMALL) {
        buffer = heap_alloc(buffer_size);
        status = g_st->BootServices->LocateHandle(ByProtocol, &guid, NULL, &buffer_size, buffer);
    }
    if(EFI_ERROR(status)) log_panic("DISK", "Failed to retrieve block I/O handles");

    for(UINTN i = 0; i < buffer_size; i += sizeof(EFI_HANDLE)) {
        EFI_BLOCK_IO *io;
        status = g_st->BootServices->OpenProtocol(*buffer++, &guid, (void **) &io, g_imagehandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if(EFI_ERROR(status) || !io || io->Media->LastBlock == 0 || io->Media->LogicalPartition) continue;

        io->Media->WriteCaching = false;

        disk_t *disk = heap_alloc(sizeof(disk_t));
        disk->next = g_disks;
        g_disks = disk;

        disk->io = io;
        disk->writable = !io->Media->ReadOnly;
        disk->sector_size = io->Media->BlockSize;
        disk->sector_count = io->Media->LastBlock + 1;
    }
}

bool disk_read_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *dest) {
    return EFI_ERROR(disk->io->ReadBlocks(disk->io, disk->io->Media->MediaId, lba, sector_count * disk->sector_size, dest));
}

bool disk_write_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *src) {
    return EFI_ERROR(disk->io->WriteBlocks(disk->io, disk->io->Media->MediaId, lba, sector_count * disk->sector_size, src));
}
#endif