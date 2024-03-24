#include "protocol.h"
#include <stdint.h>
#include <common/log.h>
#include <lib/mem.h>
#include <lib/math.h>
#include <memory/pmm.h>

#define LINUX_IMAGE_SIGNATURE 0x53726448

#define LOAD_FLAGS_LOADED_HIGH (1 << 0)
#define LOAD_FLAGS_QUIET (1 << 5)
#define LOAD_FLAGS_KEEP_SEGMENTS (1 << 6)
#define LOAD_FLAGS_CAN_USE_HEAP (1 << 7)

typedef struct {
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
} __attribute__((packed)) screen_info_t;

typedef struct {
    uint16_t version;
    uint16_t cseg;
    uint32_t offset;
    uint16_t cseg_16;
    uint16_t dseg;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
} __attribute__((packed)) apm_bios_info_t;

typedef struct {
    uint32_t signature;
    uint32_t command;
    uint32_t event;
    uint32_t perf_level;
} __attribute__((packed)) ist_info_t;

typedef struct {
    uint16_t length;
    uint8_t table[14];
} __attribute__((packed)) sys_desc_table_t;

typedef struct {
    uint32_t ofw_magic, ofw_version;
    uint32_t cif_handler;
    uint32_t irq_desc_table;
} __attribute__((packed)) olpc_ofw_header_t;

typedef struct {
    unsigned char dummy[128];
} __attribute__((packed)) edid_info_t;

typedef struct {
    uint32_t efi_loader_signature;
    uint32_t efi_systab;
    uint32_t efi_memdesc_size, efi_memdesc_version;
    uint32_t efi_memmap, efi_memmap_size;
    uint32_t efi_systab_hi;
    uint32_t efi_memmap_hi;
} __attribute__((packed)) efi_info_t;

typedef struct {
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
} __attribute__((packed)) setup_header_t;

typedef struct {
    uint64_t address;
    uint64_t size;
    uint32_t type;
} __attribute__((packed)) linux_e820_entry_t;

typedef struct {
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
        struct {
            uint16_t base_address;
            uint16_t rsv0;
            uint32_t rsv1;
        } __attribute__((packed)) isa;
        struct {
            uint8_t bus, slot, function, channel;
            uint32_t rsv0;
        } __attribute__((packed)) pci;
        struct {
            uint64_t rsv0;
        } __attribute__((packed)) ibnd;
        struct {
            uint64_t rsv0;
        } __attribute__((packed)) xprs;
        struct {
            uint64_t rsv0;
        } __attribute__((packed)) htpt;
        struct {
            uint64_t rsv0;
        } __attribute__((packed)) unknown;
    } interface_path;
    union {
        struct {
            uint8_t device;
            uint8_t rsv0;
            uint16_t rsv1;
            uint32_t rsv2;
            uint64_t rsv3;
        } __attribute__((packed)) ata;
        struct {
            uint8_t device, lun;
            uint8_t rsv0, rsv1;
            uint32_t rsv2;
            uint64_t rsv3;
        } __attribute__((packed)) atapi;
        struct {
            uint16_t id;
            uint64_t lun;
            uint16_t rsv0;
            uint32_t rsv1;
        } __attribute__((packed)) scsi;
        struct {
            uint64_t serial_number;
            uint64_t rsv0;
        } __attribute__((packed)) usb;
        struct {
            uint64_t eui;
            uint64_t rsv0;
        } __attribute__((packed)) i1394;
        struct {
            uint64_t wwid, lun;
        } __attribute__((packed)) fibre;
        struct {
            uint64_t identity_tag;
            uint64_t rsv0;
        } __attribute__((packed)) i2o;
        struct {
            uint32_t array_number;
            uint32_t rsv0;
            uint64_t rsv1;
        } __attribute__((packed)) raid;
        struct {
            uint8_t device;
            uint8_t rsv0;
            uint16_t rsv1;
            uint32_t rsv2;
            uint64_t rsv3;
        } __attribute__((packed)) sata;
        struct {
            uint64_t rsv0, rsv1;
        } __attribute__((packed)) unknown;
    } device_path;
    uint8_t rsv2;
    uint8_t checksum;
} __attribute__((packed)) edd_device_params_t;

typedef struct {
    uint8_t device, version;
    uint16_t interface_support;
    uint16_t legacy_max_cylinder;
    uint8_t legacy_max_head;
    uint8_t legacy_sectors_per_track;
    edd_device_params_t params;
} __attribute__((packed)) edd_info_t;

typedef struct {
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
} __attribute__((packed)) boot_params_t;

static_assert(sizeof(boot_params_t) == PMM_PAGE_SIZE);

[[noreturn]] void linux_handoff(void *kernel_entry, void *boot_params);

[[noreturn]] void protocol_linux(vfs_node_t *kernel_node, vfs_node_t *ramdisk_node, char *cmd, acpi_rsdp_t *rsdp, e820_entry_t e820[], size_t e820_size, fb_t fb) {
    if(cmd == NULL) cmd = "";

    // Validate signature
    uint32_t signature;
    if(kernel_node->ops->read(kernel_node, &signature, 0x202, sizeof(uint32_t)) != sizeof(uint32_t)) log_panic("PROTO_LINUX", "Failed to read signature");
    if(signature != LINUX_IMAGE_SIGNATURE) log_panic("PROTO_LINUX", "Invalid kernel image signature");

    // Setup boot params
    boot_params_t *boot_params = pmm_alloc(PMM_AREA_MAX, 1);
    memset(boot_params, 0, PMM_PAGE_SIZE);

    size_t setup_header_size = 0;
    if(kernel_node->ops->read(kernel_node, &setup_header_size, 0x201, sizeof(uint8_t)) != sizeof(uint8_t)) log_panic("PROTO_LINUX", "Failed to read setup header size");
    setup_header_size += 0x11;
    if(kernel_node->ops->read(kernel_node, &boot_params->setup_header, 0x1F1, setup_header_size) != setup_header_size) log_panic("PROTO_LINUX", "Failed to read setup_header");

    log("PROTO_LINUX", "Protocol Version %u.%u", boot_params->setup_header.version >> 8, boot_params->setup_header.version & 0xFF);
    if(boot_params->setup_header.version <= 0x20A) log_panic("PROTO_LINUX", "Protocol versions under 2.10 are not supported");
    if((boot_params->setup_header.load_flags & LOAD_FLAGS_LOADED_HIGH) == 0) log_panic("PROTO_LINUX", "Kernels without LOADED_HIGH load flag are unsupported");

    boot_params->setup_header.type_of_loader = 0xFF;
    boot_params->setup_header.cmd_line_ptr = (uintptr_t) cmd;
    boot_params->setup_header.vid_mode = 0xFFFF;
    boot_params->setup_header.load_flags &= (~LOAD_FLAGS_QUIET) | (~LOAD_FLAGS_CAN_USE_HEAP);
    boot_params->setup_header.load_flags |= LOAD_FLAGS_KEEP_SEGMENTS;
    boot_params->acpi_rsdp_address = (uintptr_t) rsdp;

    boot_params->screen_info.capabilities = 0x3;
    boot_params->screen_info.flags = 0x1;
    boot_params->screen_info.orig_video_is_vga = 0x23;
    boot_params->screen_info.lfb_base = (uint32_t) fb.address;
    boot_params->screen_info.ext_lfb_base = (uint32_t) ((uint64_t) fb.address >> 32);
    boot_params->screen_info.lfb_size = fb.size;
    boot_params->screen_info.lfb_width = fb.width;
    boot_params->screen_info.lfb_height = fb.height;
    boot_params->screen_info.lfb_line_length = fb.pitch;
    boot_params->screen_info.lfb_depth = fb.bpp;
    boot_params->screen_info.red_size = fb.mask_red_size;
    boot_params->screen_info.red_pos = fb.mask_red_position;
    boot_params->screen_info.green_size = fb.mask_green_size;
    boot_params->screen_info.green_pos = fb.mask_green_position;
    boot_params->screen_info.blue_size = fb.mask_blue_size;
    boot_params->screen_info.blue_pos = fb.mask_blue_position;

    for(size_t i = 0; i < e820_size; i++) {
        if(i >= 128) log_panic("PROTO_LINUX", "Cannot fit e820 into boot_params");
        boot_params->e820_table[i].type = e820[i].type;
        boot_params->e820_table[i].size = e820[i].length;
        boot_params->e820_table[i].address = e820[i].address;
        boot_params->e820_entries++;
    }

    // Load kernel
    vfs_attr_t kernel_attr;
    kernel_node->ops->attr(kernel_node, &kernel_attr);
    size_t real_mode_kernel_size = (boot_params->setup_header.setup_sects > 0 ? boot_params->setup_header.setup_sects : 4) * 512;
    size_t kernel_size = kernel_attr.size - real_mode_kernel_size;

    uintptr_t kernel_addr = 0x100000;
    for(;; kernel_addr += boot_params->setup_header.kernel_alignment) {
        if(kernel_addr >= MATH_FLOOR(UINT32_MAX, boot_params->setup_header.kernel_alignment)) {
            if(boot_params->setup_header.kernel_alignment == PMM_PAGE_SIZE) log_panic("PROTO_LINUX", "Failed to allocate kernel");
            boot_params->setup_header.kernel_alignment = PMM_PAGE_SIZE;
            kernel_addr = 0x100000;
        }
        if(!pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, kernel_addr, MATH_CEIL(kernel_size, PMM_PAGE_SIZE))) break;
        if(boot_params->setup_header.relocatable_kernel == 0) log_panic("PROTO_LINUX", "Unrelocatable kernel cannot be allocated at %#lx", kernel_addr);
    }
    if(kernel_node->ops->read(kernel_node, (void *) kernel_addr, real_mode_kernel_size, kernel_size) != kernel_size) log_panic("PROTO_LINUX", "Failed to load kernel");
    log("PROTO_LINUX", "Loaded kernel at %#lx", kernel_addr);

    // Load ramdisk
    vfs_attr_t ramdisk_attr;
    ramdisk_node->ops->attr(ramdisk_node, &ramdisk_attr);

    uintptr_t ramdisk_addr = boot_params->setup_header.initrd_addr_max;
    if(ramdisk_addr == 0) ramdisk_addr = 0x37FFFFFF;
    size_t aligned_ramdisk_size = MATH_CEIL(ramdisk_attr.size, PMM_PAGE_SIZE);
    ramdisk_addr -= aligned_ramdisk_size;
    ramdisk_addr = MATH_FLOOR(ramdisk_addr, PMM_PAGE_SIZE);
    for (; ramdisk_addr != 0; ramdisk_addr -= PMM_PAGE_SIZE) {
        if(!pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, ramdisk_addr, aligned_ramdisk_size)) break;
    }
    if(ramdisk_addr == 0) log_panic("PROTO_LINUX", "Failed to allocate ramdisk");
    if(ramdisk_node->ops->read(ramdisk_node, (void *) ramdisk_addr, 0, ramdisk_attr.size) != ramdisk_attr.size) log_panic("PROTO_LINUX", "Failed to load ramdisk");
    boot_params->setup_header.ramdisk_image = ramdisk_addr;
    boot_params->setup_header.ramdisk_size = ramdisk_attr.size;
    log("PROTO_LINUX", "Loaded initrd size %#lx at address %#lx", ramdisk_attr.size, ramdisk_addr);

    // Handoff
    log("PROTO_LINUX", "Handoff");
    linux_handoff((void *) kernel_addr, boot_params);
    __builtin_unreachable();
}