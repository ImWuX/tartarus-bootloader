TARGET ?=
ifndef TARGET
    $(error TARGET is not specified)
endif

EFIDIR ?=
ifeq ($(TARGET), $(filter $(TARGET), amd64-uefi32 amd64-uefi64))
    ifndef EFIDIR
        $(error EFIDIR is not specified)
    endif
endif

include ../config.mk

.PHONY: all clean

# SOURCES
ASMSRC = $(shell find src -type f -regex "src\/[^.]*?.asm")
CSRC = $(shell find src -type f -regex "src\/[^.]*?.c")

ifneq (, $(filter $(TARGET), amd64-bios amd64-uefi32 amd64-uefi64))
    ASMSRC += $(shell find src -type f -name *.amd64.asm)
    CSRC += $(shell find src -type f -name *.amd64.c)
endif

ifeq ($(TARGET), amd64-bios)
    ASMSRC += $(shell find src -type f -name *.amd64.bios.asm -o -name *.bios.asm)
    CSRC += $(shell find src -type f -name *.amd64.bios.c -o -name *.bios.c)
endif

ifneq (, $(filter $(TARGET), amd64-uefi32 amd64-uefi64))
    ASMSRC += $(shell find src -type f -name *.amd64.uefi.asm -o -name *.uefi.asm)
    CSRC += $(shell find src -type f -name *.amd64.uefi.c -o -name *.uefi.c)
endif

ifeq ($(TARGET), amd64-uefi32)
    ASMSRC += $(shell find src -type f -name *.amd64.uefi32.asm)
    CSRC += $(shell find src -type f -name *.amd64.uefi32.c)
endif

ifeq ($(TARGET), amd64-uefi64)
    ASMSRC += $(shell find src -type f -name *.amd64.uefi64.asm)
    CSRC += $(shell find src -type f -name *.amd64.uefi64.c)
endif

# FLAGS
DEFINES =
ASMFLAGS = -isrc -i..
CFLAGS = -Isrc -I.. -std=gnu2x -O2 -ffreestanding -Wall -Wextra -Wshadow -Wvla -Wno-unused-variable -fno-stack-protector -fno-stack-check -fno-omit-frame-pointer -fno-strict-aliasing -fno-lto --param min-pagesize=0
LDFLAGS = -nostdlib -static -z noexecstack

# FLAGS ( amd64 )
ifneq (, $(filter $(TARGET), amd64-bios amd64-uefi32 amd64-uefi64))
    DEFINES += -D __AMD64
    CFLAGS += -mabi=sysv -mno-80387
    LDFLAGS += -z common-page-size=0x1000 -z max-page-size=0x1000
endif

# FLAGS ( 32-bit )
ifneq (, $(filter $(TARGET), amd64-bios amd64-uefi32))
    ASMFLAGS += -f elf32
    CFLAGS += -m32 -march=i686
endif

# FLAGS ( 64-bit )
ifeq ($(TARGET), amd64-uefi64)
    ASMFLAGS += -f elf64
    CFLAGS += -m64 -march=x86-64 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone
endif

# FLAGS ( bios )
ifeq ($(TARGET), amd64-bios)
    DEFINES += -D __BIOS -D __TARTARUS_NO_PTR
    CFLAGS += -mgeneral-regs-only
endif

# FLAGS ( uefi )
ifneq (, $(filter $(TARGET), amd64-uefi32 amd64-uefi64))
    DEFINES += -D __UEFI
    LDFLAGS += -pie --no-dynamic-linker -z text
    CFLAGS += -D GNU_EFI_USE_MS_ABI -I$(EFIDIR)/inc -Wno-pointer-sign -fPIE -funsigned-char -fshort-wchar
endif

# FLAGS ( uefi64 )
ifeq ($(TARGET), amd64-uefi64)
    DEFINES += -D __UEFI64
    CFLAGS += -D ARCH=x86_64 -I$(EFIDIR)/inc/x86_64
endif

# FLAGS ( uefi32 )
ifeq ($(TARGET), amd64-uefi32)
    DEFINES += -D __UEFI32 -D __TARTARUS_NO_PTR
endif

# BUILD amd64-bios
ifeq ($(TARGET), amd64-bios)
tartarus.sys: tartarus.elf
	$(OBJCPY) -O binary $< $@

tartarus.elf: $(CSRC:.c=.o) $(ASMSRC:.asm=.o) $(IA32_LIBGCC)
	$(LD) $(LDFLAGS) -T"link.bios.ld" -o $@ $^
endif

# BUILD amd64-uefi64
ifeq ($(TARGET), amd64-uefi64)
tartarus.efi: tartarus.elf
	$(OBJCPY) -O binary $< $@

$(EFIDIR)/gnuefi/crt0-efi-x86_64.o $(EFIDIR)/gnuefi/reloc_x86_64.o:
	$(MAKE) -C $(EFIDIR)/gnuefi

tartarus.elf: $(EFIDIR)/gnuefi/crt0-efi-x86_64.o $(EFIDIR)/gnuefi/reloc_x86_64.o $(CSRC:.c=.o) $(ASMSRC:.asm=.o) $(AMD64_LIBGCC)
	$(LD) $(LDFLAGS) -T"$(EFIDIR)/gnuefi/elf_x86_64_efi.lds" -o $@ $^
endif

# CLEAN
clean:
	rm -f $(shell find src -type f -name '*.o')
	rm -f tartarus.elf
ifeq ($(TARGET), amd64-bios)
	rm -f tartarus.sys
endif
ifneq (, $(filter $(TARGET), amd64-uefi32 amd64-uefi64))
	rm -f tartarus.efi
endif

# WILDCARD
%.o: %.asm
	$(ASMC) $(DEFINES) $(ASMFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(DEFINES) $(CFLAGS) -o $@ -c $<