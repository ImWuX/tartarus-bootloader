#include "arch/acpi.h"
#include "common/log.h"
#include "common/panic.h"
#include "dev/acpi.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/pmm.h"
#include "protocol.h"

#include <stdint.h>

#ifdef __PLATFORM_X86_64_UEFI
#include "arch/uefi/uefi.h"
#endif

#define LINUX_IMAGE_SIGNATURE 0x53726448

#define LOAD_FLAGS_LOADED_HIGH (1 << 0)
#define LOAD_FLAGS_QUIET (1 << 5)
#define LOAD_FLAGS_KEEP_SEGMENTS (1 << 6)
#define LOAD_FLAGS_CAN_USE_HEAP (1 << 7)

#define VIDEO_FLAG_NOCURSOR (1 << 0)

#define VIDEO_CAP_NOQUIRKS (1 << 0)
#define VIDEO_CAP_64BIT (1 << 1)

#define VIDEO_TYPE_VLFB 0x23
#define VIDEO_TYPE_EFI 0x70

typedef struct [[gnu::packed]] {
    uint8_t orig_x, orig_y;
    uint16_t ext_mem_k;
    uint16_t orig_video_page;
    uint8_t orig_video_mode;
    uint8_t orig_video_cols;
    uint8_t flags;
    uint8_t unused0;
    uint16_t orig_video_ega_bx;
    uint16_t unused1;
    uint8_t orig_video_lines;
    uint8_t orig_video_is_vga;
    uint16_t orig_video_points;
    uint16_t lfb_width, lfb_height, lfb_depth;
    uint32_t lfb_base, lfb_size;
    uint16_t cl_magic, cl_offset;
    uint16_t lfb_line_length;
    uint8_t red_size, red_pos;
    uint8_t green_size, green_pos;
    uint8_t blue_size, blue_pos;
    uint8_t rsv_size, rsv_pos;
    uint16_t vesapm_seg, vesapm_off;
    uint16_t pages;
    uint16_t vesa_attributes;
    uint32_t capabilities;
    uint32_t ext_lfb_base;
    uint8_t rsv0[2];
} screen_info_t;

typedef struct [[gnu::packed]] {
    uint16_t version;
    uint16_t cseg;
    uint32_t offset;
    uint16_t cseg_16;
    uint16_t dseg;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
} apm_bios_info_t;

typedef struct [[gnu::packed]] {
    uint32_t signature;
    uint32_t command;
    uint32_t event;
    uint32_t perf_level;
} ist_info_t;

typedef struct [[gnu::packed]] {
    uint16_t length;
    uint8_t table[14];
} sys_desc_table_t;

typedef struct [[gnu::packed]] {
    uint32_t ofw_magic, ofw_version;
    uint32_t cif_handler;
    uint32_t irq_desc_table;
} olpc_ofw_header_t;

typedef struct [[gnu::packed]] {
    unsigned char dummy[128];
} edid_info_t;

typedef struct [[gnu::packed]] {
    uint32_t efi_loader_signature;
    uint32_t efi_systab;
    uint32_t efi_memdesc_size, efi_memdesc_version;
    uint32_t efi_memmap, efi_memmap_size;
    uint32_t efi_systab_hi;
    uint32_t efi_memmap_hi;
} efi_info_t;

typedef struct [[gnu::packed]] {
    uint8_t setup_sects;
    uint16_t root_flags;
    uint32_t sys_size;
    uint16_t ram_size;
    uint16_t vid_mode;
    uint16_t root_dev;
    uint16_t boot_flag;
    uint16_t jump;
    uint32_t header;
    uint16_t version;
    uint32_t realmode_switch;
    uint16_t start_sys;
    uint16_t kernel_version;
    uint8_t type_of_loader;
    uint8_t load_flags;
    uint16_t setup_move_size;
    uint32_t code32_start;
    uint32_t ramdisk_image, ramdisk_size;
    uint32_t bootsect_kludge;
    uint16_t heap_end_ptr;
    uint8_t ext_loader_ver, ext_loader_type;
    uint32_t cmd_line_ptr;
    uint32_t initrd_addr_max;
    uint32_t kernel_alignment;
    uint8_t relocatable_kernel;
    uint8_t min_alignment;
    uint16_t xload_flags;
    uint32_t cmdline_size;
    uint32_t hardware_subarch;
    uint64_t hardware_subarch_data;
    uint32_t payload_offset, payload_length;
    uint64_t setup_data;
    uint64_t pref_address;
    uint32_t init_size;
    uint32_t handover_offset;
    uint32_t kernel_info_offset;
} setup_header_t;

typedef struct [[gnu::packed]] {
    uint64_t address;
    uint64_t size;
    uint32_t type;
} linux_e820_entry_t;

typedef enum {
    LINUX_E820_TYPE_USABLE = 1,
    LINUX_E820_TYPE_RESERVED,
    LINUX_E820_TYPE_ACPI_RECLAIMABLE,
    LINUX_E820_TYPE_ACPI_NVS,
    LINUX_E820_TYPE_BAD,
} linux_e820_type_t;


typedef struct [[gnu::packed]] {
    uint16_t length;
    uint16_t info_flags;
    uint32_t num_default_cylinders, num_default_heads;
    uint32_t sectors_per_track;
    uint64_t number_of_sectors;
    uint16_t bytes_per_sector;
    uint32_t dpte_ptr;
    uint16_t key;
    uint8_t device_path_info_length;
    uint8_t rsv0;
    uint16_t rsv1;
    uint8_t host_bus_type[4];
    uint8_t interface_type[8];
    union {
        struct [[gnu::packed]] {
            uint16_t base_address;
            uint16_t rsv0;
            uint32_t rsv1;
        } isa;
        struct [[gnu::packed]] {
            uint8_t bus, slot, function, channel;
            uint32_t rsv0;
        } pci;
        struct [[gnu::packed]] {
            uint64_t rsv0;
        } ibnd;
        struct [[gnu::packed]] {
            uint64_t rsv0;
        } xprs;
        struct [[gnu::packed]] {
            uint64_t rsv0;
        } htpt;
        struct [[gnu::packed]] {
            uint64_t rsv0;
        } unknown;
    } interface_path;
    union {
        struct [[gnu::packed]] {
            uint8_t device;
            uint8_t rsv0;
            uint16_t rsv1;
            uint32_t rsv2;
            uint64_t rsv3;
        } ata;
        struct [[gnu::packed]] {
            uint8_t device, lun;
            uint8_t rsv0, rsv1;
            uint32_t rsv2;
            uint64_t rsv3;
        } atapi;
        struct [[gnu::packed]] {
            uint16_t id;
            uint64_t lun;
            uint16_t rsv0;
            uint32_t rsv1;
        } scsi;
        struct [[gnu::packed]] {
            uint64_t serial_number;
            uint64_t rsv0;
        } usb;
        struct [[gnu::packed]] {
            uint64_t eui;
            uint64_t rsv0;
        } i1394;
        struct [[gnu::packed]] {
            uint64_t wwid, lun;
        } fibre;
        struct [[gnu::packed]] {
            uint64_t identity_tag;
            uint64_t rsv0;
        } i2o;
        struct [[gnu::packed]] {
            uint32_t array_number;
            uint32_t rsv0;
            uint64_t rsv1;
        } raid;
        struct [[gnu::packed]] {
            uint8_t device;
            uint8_t rsv0;
            uint16_t rsv1;
            uint32_t rsv2;
            uint64_t rsv3;
        } sata;
        struct [[gnu::packed]] {
            uint64_t rsv0, rsv1;
        } unknown;
    } device_path;
    uint8_t rsv2;
    uint8_t checksum;
} edd_device_params_t;

typedef struct [[gnu::packed]] {
    uint8_t device, version;
    uint16_t interface_support;
    uint16_t legacy_max_cylinder;
    uint8_t legacy_max_head;
    uint8_t legacy_sectors_per_track;
    edd_device_params_t params;
} edd_info_t;

typedef struct [[gnu::packed]] {
    screen_info_t screen_info;
    apm_bios_info_t apm_bios_info;
    uint8_t padding0[4];
    uint64_t tboot_address;
    ist_info_t ist_info;
    uint64_t acpi_rsdp_address;
    uint8_t padding1[8];
    uint8_t hd0_info[16];
    uint8_t hd1_info[16];
    sys_desc_table_t sys_desc_table;
    olpc_ofw_header_t olpc_ofw_header;
    uint32_t ext_ramdisk_image, ext_ramdisk_size;
    uint32_t ext_cmd_line_ptr;
    uint8_t padding2[112];
    uint32_t cc_blob_address;
    edid_info_t edid_info;
    efi_info_t efi_info;
    uint32_t alt_mem_k;
    uint32_t scratch;
    uint8_t e820_entries;
    uint8_t eddbuf_entries;
    uint8_t edd_mbr_sig_buf_entries;
    uint8_t kbd_status;
    uint8_t secure_boot;
    uint8_t padding3[2];
    uint8_t sentinel;
    uint8_t padding4[1];
    setup_header_t setup_header;
    uint8_t padding5[0x290 - 0x1F1 - sizeof(setup_header_t)];
    uint32_t edd_mbr_sig_buffer[16];
    linux_e820_entry_t e820_table[128];
    uint8_t padding6[48];
    edd_info_t edd_buf[6];
    uint8_t padding7[276];
} boot_params_t;

static_assert(sizeof(boot_params_t) == PMM_GRANULARITY);

[[noreturn]] void linux_handoff(void *kernel_entry, void *boot_params);

[[noreturn]] void protocol_linux(config_t *config, vfs_node_t *kernel_node, fb_t *fb) {
    const char *command_line = config_find_string(config, "cmd", "auto");

    // RSDP
    acpi_rsdp_t *rsdp = arch_acpi_find_rsdp();
    if(rsdp == NULL) panic("linux_protocol: could not locate RSDP");

    // Validate linux signature
    uint32_t signature;
    if(kernel_node->ops->read(kernel_node, &signature, 0x202, sizeof(uint32_t)) != sizeof(uint32_t)) panic("linux_protocol: failed to read signature");
    if(signature != LINUX_IMAGE_SIGNATURE) panic("linux_protocol: invalid kernel image signature");

    // Setup boot params
    boot_params_t *boot_params = pmm_alloc(PMM_AREA_STANDARD, 1);
    memset(boot_params, 0, PMM_GRANULARITY);

    size_t setup_header_size = 0;
    if(kernel_node->ops->read(kernel_node, &setup_header_size, 0x201, sizeof(uint8_t)) != sizeof(uint8_t)) panic("linux_protocol: failed to read setup header size");
    setup_header_size += 0x11;
    if(setup_header_size > sizeof(setup_header_t)) {
        setup_header_size = sizeof(setup_header_t);
        log(LOG_LEVEL_WARN, "Setup header size largest than expected (%#lx/%#lx)", setup_header_size, sizeof(setup_header_t));
    }
    if(setup_header_size < sizeof(setup_header_t)) panic("linux_protocol: expected header size less than actual size (%#lx/%#lx)", setup_header_size, sizeof(setup_header_t));
    if(kernel_node->ops->read(kernel_node, &boot_params->setup_header, 0x1F1, setup_header_size) != setup_header_size) panic("linux_protocol: failed to read setup_header");

    log(LOG_LEVEL_INFO, "Linux boot protocol version %u.%u", boot_params->setup_header.version >> 8, boot_params->setup_header.version & 0xFF);
    if(boot_params->setup_header.version <= 0x20A) panic("linux_protocol: protocol versions under 2.10 are not supported");
    if((boot_params->setup_header.load_flags & LOAD_FLAGS_LOADED_HIGH) == 0) panic("linux_protocol: kernels without LOADED_HIGH load flag are unsupported");

    boot_params->setup_header.cmd_line_ptr = (uintptr_t) command_line;
    boot_params->setup_header.type_of_loader = 0xFF;
    boot_params->setup_header.vid_mode = 0xFFFF;
    boot_params->setup_header.load_flags &= (~LOAD_FLAGS_QUIET) | (~LOAD_FLAGS_CAN_USE_HEAP);
    boot_params->setup_header.load_flags |= LOAD_FLAGS_KEEP_SEGMENTS;
    boot_params->acpi_rsdp_address = (uintptr_t) rsdp;

    boot_params->screen_info.capabilities = VIDEO_CAP_NOQUIRKS | VIDEO_CAP_64BIT;
    boot_params->screen_info.flags = VIDEO_FLAG_NOCURSOR;
    boot_params->screen_info.lfb_base = (uint32_t) fb->address;
    boot_params->screen_info.ext_lfb_base = (uint32_t) ((uint64_t) fb->address >> 32);
    boot_params->screen_info.lfb_size = fb->size;
    boot_params->screen_info.lfb_width = fb->width;
    boot_params->screen_info.lfb_height = fb->height;
    boot_params->screen_info.lfb_line_length = fb->pitch;
    boot_params->screen_info.lfb_depth = fb->bpp;
    boot_params->screen_info.red_size = fb->mask_red_size;
    boot_params->screen_info.red_pos = fb->mask_red_position;
    boot_params->screen_info.green_size = fb->mask_green_size;
    boot_params->screen_info.green_pos = fb->mask_green_position;
    boot_params->screen_info.blue_size = fb->mask_blue_size;
    boot_params->screen_info.blue_pos = fb->mask_blue_position;
#ifdef __PLATFORM_X86_64_BIOS
    boot_params->screen_info.orig_video_is_vga = VIDEO_TYPE_VLFB;
#elif __UEFI
    boot_params->screen_info.orig_video_is_vga = VIDEO_TYPE_EFI;
#else
#error Unimplemented
#endif

    // Load kernel
    size_t setup_sector_count = boot_params->setup_header.setup_sects == 0 ? 4 : boot_params->setup_header.setup_sects;
    size_t real_mode_kernel_size = (setup_sector_count * 512) + 512;
    size_t kernel_size = kernel_node->ops->get_size(kernel_node) - real_mode_kernel_size;

    void *kernel_address = (void *) 0x100000;
    if(!pmm_alloc_at((uintptr_t) kernel_address, MATH_DIV_CEIL(kernel_size, PMM_GRANULARITY), PMM_MAP_TYPE_ALLOCATED)) {
        if(boot_params->setup_header.relocatable_kernel == 0) panic("linux_protocol: unrelocatable kernel cannot be loaded at %#lx", kernel_address);

        kernel_address = pmm_alloc_ext(PMM_AREA_STANDARD, MATH_DIV_CEIL(kernel_size, PMM_GRANULARITY), boot_params->setup_header.kernel_alignment, PMM_MAP_TYPE_ALLOCATED);
        if(kernel_address == NULL) {
            if((1 << boot_params->setup_header.min_alignment) > PMM_GRANULARITY) panic("linux_protocol: unsupported minimum kernel alignment");
            boot_params->setup_header.kernel_alignment = PMM_GRANULARITY;
            kernel_address = pmm_alloc_ext(PMM_AREA_STANDARD, MATH_DIV_CEIL(kernel_size, PMM_GRANULARITY), boot_params->setup_header.kernel_alignment, PMM_MAP_TYPE_ALLOCATED);
        }
        if(kernel_address == NULL) panic("linux_protocol: failed to allocate kernel");
    }
    if(kernel_node->ops->read(kernel_node, kernel_address, real_mode_kernel_size, kernel_size) != kernel_size) panic("linux_protocol: failed to load kernel");
    log(LOG_LEVEL_INFO, "Loaded kernel at %#lx", kernel_address);

    // Load ramdisk
    const char *ramdisk_path = config_find_string(config, "initrd", NULL);
    if(ramdisk_path != NULL) {
        vfs_node_t *ramdisk_node = vfs_lookup(kernel_node->vfs, ramdisk_path);
        if(ramdisk_node == NULL) panic("linux_protocol: initrd not present at \"%s\"", ramdisk_path);

        size_t ramdisk_size = ramdisk_node->ops->get_size(ramdisk_node);
        size_t ramdisk_pages = MATH_DIV_CEIL(ramdisk_size, PMM_GRANULARITY);
        uintptr_t ramdisk_max_addr = boot_params->setup_header.initrd_addr_max - (ramdisk_pages * PMM_GRANULARITY);
        // FIX: The alignment of `0x10000000` should not be required... Track down rootcause of initrd fail.
        void *ramdisk_address = pmm_alloc_ext((pmm_map_area_t) {.start = PMM_AREA_STANDARD.start, .end = ramdisk_max_addr}, ramdisk_pages, 0x10000000, PMM_MAP_TYPE_ALLOCATED);
        if(ramdisk_node->ops->read(ramdisk_node, ramdisk_address, 0, ramdisk_size) != ramdisk_size) panic("linux_protocol: failed to load ramdisk");
        boot_params->setup_header.ramdisk_image = (uintptr_t) ramdisk_address;
        boot_params->setup_header.ramdisk_size = ramdisk_size;
        log(LOG_LEVEL_INFO, "Loaded initrd of size %#lx at address %#lx", ramdisk_size, ramdisk_address);
    }

    // Platform exit
#ifdef __UEFI
    uefi_bootservices_exit();
#endif

    // Load memory map
    for(size_t i = 0; i < g_pmm_map_size; i++) {
        linux_e820_type_t e820_type = LINUX_E820_TYPE_RESERVED;
        switch(g_pmm_map[i].type) {
            case PMM_MAP_TYPE_RESERVED:
            case PMM_MAP_TYPE_ACPI_TABLES:
            case PMM_MAP_TYPE_ALLOCATED:
            case PMM_MAP_TYPE_EFI_RECLAIMABLE:  e820_type = LINUX_E820_TYPE_RESERVED; break;
            case PMM_MAP_TYPE_FREE:             e820_type = LINUX_E820_TYPE_USABLE; break;
            case PMM_MAP_TYPE_ACPI_RECLAIMABLE: e820_type = LINUX_E820_TYPE_ACPI_RECLAIMABLE; break;
            case PMM_MAP_TYPE_ACPI_NVS:         e820_type = LINUX_E820_TYPE_ACPI_NVS; break;
            case PMM_MAP_TYPE_BAD:              e820_type = LINUX_E820_TYPE_BAD;
        }

        boot_params->e820_table[i] = (linux_e820_entry_t) {
            .type = e820_type,
            .address = g_pmm_map[i].base,
            .size = g_pmm_map[i].length,
        };
        boot_params->e820_entries++;
    }

    // Handoff
    linux_handoff(kernel_address, boot_params);
    __builtin_unreachable();
}
