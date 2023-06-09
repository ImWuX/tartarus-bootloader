#include "disk.h"
#include <libc.h>
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

#define GPT_TYPE_PROTECTIVE 0xEE

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
            disk_part_t *partition = heap_alloc(sizeof(disk_part_t));
            partition->id = i;
            partition->disk = disk;
            partition->lba = entry->start_lba;
            partition->size = entry->end_lba - entry->start_lba;
            partition->next = disk->partitions;
            disk->partitions = partition;
        }
    } else {
        log_warning("PARTITION", "Ignoring drive %x. Read failed.", (uint64_t) disk->id);
    }
    // TODO: Free buf
}

static void initialize_partitions(disk_t *disk) {
    int buf_size = (disk->sector_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *buf = pmm_alloc_pages(buf_size, PMM_AREA_CONVENTIONAL);

    if(!disk_read_sector(disk, 0, 1, buf)) {
        mbr_t *mbr = (mbr_t *) ((uintptr_t) buf + 440);
        if(mbr->entries[0].type == GPT_TYPE_PROTECTIVE) {
            disk_read_sector(disk, mbr->entries[0].start_lba, 1, buf);
            initialize_gpt_partitions(disk, (gpt_header_t *) buf);
        } else {
            log_warning("PARTITION", "Ignoring drive %x because it is partitioned with a legacy MBR.", (uint64_t) disk->id);
        }
    } else {
        log_warning("PARTITION", "Ignoring drive %x. Read failed.", (uint64_t) disk->id);
    }
    // TODO: Free buf
}

#if defined __BIOS && defined __AMD64

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
        disk->id = i;
        disk->sector_size = params.sector_size; // TODO: The sector size provided by BIOS is unreliable. We can manually figure out the size by reading into a filled buffer and empty buffer and figuring out where the last modified byte is.
        disk->sector_count = params.abs_sectors;

        int buf_pages = (disk->sector_size + PAGE_SIZE - 1) / PAGE_SIZE;
        void *buf = pmm_alloc_pages(buf_pages, PMM_AREA_CONVENTIONAL);
        if(disk_read_sector(disk, 0, buf_pages, buf)) {
            heap_free(disk);
            continue;
        }
        disk->writable = !disk_write_sector(disk, 0, buf_pages, buf);
        disk->partitions = 0;
        initialize_partitions(disk);
        disk->next = g_disks;
        g_disks = disk;
        // TODO: Free buf
    }
}

bool disk_read_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *dest) {
    disk_address_packet_t dap = {
        .size = sizeof(disk_address_packet_t),
        .sector_count = 1,
    };
    int_regs_t regs = {
        .edx = disk->id,
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
        .edx = disk->id,
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
        disk->id = io->Media->MediaId;
        disk->io = io;
        disk->writable = !io->Media->ReadOnly;
        disk->sector_size = io->Media->BlockSize;
        disk->sector_count = io->Media->LastBlock + 1;
        disk->partitions = 0;
        initialize_partitions(disk);

        disk->next = g_disks;
        g_disks = disk;
    }
}

bool disk_read_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *dest) {
    return EFI_ERROR(disk->io->ReadBlocks(disk->io, disk->io->Media->MediaId, lba, sector_count * disk->sector_size, dest));
}

bool disk_write_sector(disk_t *disk, uint64_t lba, uint16_t sector_count, void *src) {
    return EFI_ERROR(disk->io->WriteBlocks(disk->io, disk->io->Media->MediaId, lba, sector_count * disk->sector_size, src));
}
#endif