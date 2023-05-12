include ../config.mk

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

ASMSRC = $(shell find src -type f -regex "src\/[^.]*?.asm")
CSRC = $(shell find src -type f -regex "src\/[^.]*?.c")

DEFINES =
ASMFLAGS =
CFLAGS = -I.. -std=gnu2x -O2 -ffreestanding -Wall -Wextra -Wshadow -Wvla -Wno-unused-variable -fno-stack-protector -fno-stack-check -fno-omit-frame-pointer -fno-strict-aliasing -fno-lto
LDFLAGS = -nostdlib -z common-page-size=0x1000 -z max-page-size=0x1000 -z noexecstack

.PHONY: all clean

ifneq (, $(filter $(TARGET), amd64-bios amd64-uefi32 amd64-uefi64))
    ASMSRC += $(shell find src -type f -name *.amd64.asm)
    CSRC += $(shell find src -type f -name *.amd64.c)
    DEFINES += -D __AMD64
endif

ifeq ($(TARGET), amd64-bios)
    ASMSRC += $(shell find src -type f -name *.amd64.bios.asm -o -name *.bios.asm)
    CSRC += $(shell find src -type f -name *.amd64.bios.c -o -name *.bios.c)
    DEFINES += -D __BIOS
    ASMFLAGS += -f elf32
    CFLAGS += -m32 -mgeneral-regs-only
endif

ifneq (, $(filter $(TARGET), amd64-uefi32 amd64-uefi64))
    ASMSRC += $(shell find src -type f -name *.amd64.uefi.asm -o -name *.uefi.asm)
    CSRC += $(shell find src -type f -name *.amd64.uefi.c -o -name *.uefi.c)
    DEFINES += -D __UEFI
    LDFLAGS += -static -pie --no-dynamic-linker -z text
    CFLAGS += -D GNU_EFI_USE_MS_ABI -I$(EFIDIR)/inc -I$(EFIDIR)/inc/x86_64 -Wno-pointer-sign -fPIE -funsigned-char -fshort-wchar
endif

ifeq ($(TARGET), amd64-uefi32)
    ASMSRC += $(shell find src -type f -name *.amd64.uefi32.asm)
    CSRC += $(shell find src -type f -name *.amd64.uefi32.c)
    DEFINES += -D __UEFI32
    ASMFLAGS += -f elf32
    CFLAGS += -m32
endif

ifeq ($(TARGET), amd64-uefi64)
    ASMSRC += $(shell find src -type f -name *.amd64.uefi64.asm)
    CSRC += $(shell find src -type f -name *.amd64.uefi64.c)
    DEFINES += -D __UEFI64
    ASMFLAGS += -f elf64
    CFLAGS += -m64 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -march=x86-64 -mabi=sysv -mno-80387
endif

# BUILD amd64-bios
ifeq ($(TARGET), amd64-bios)
tartarus.sys: tartarus.elf
	$(OBJCPY) -O binary $< $@

tartarus.elf: $(CSRC:.c=.o) $(ASMSRC:.asm=.o)
	$(LD) $(LDFLAGS) -T"link.bios.ld" -o $@ $^
endif

# BUILD amd64-uefi
ifeq ($(TARGET), amd64-uefi64)
tartarus.efi: tartarus.elf
	$(OBJCPY) -O binary $< $@

$(EFIDIR)/gnuefi/crt0-efi-x86_64.o $(EFIDIR)/gnuefi/reloc_x86_64.o:
	$(MAKE) -C $(EFIDIR)/gnuefi ARCH=x86_64

tartarus.elf: $(EFIDIR)/gnuefi/crt0-efi-x86_64.o $(EFIDIR)/gnuefi/reloc_x86_64.o $(CSRC:.c=.o) $(ASMSRC:.asm=.o)
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
	$(ASMC) $(DEFINES) $(ASMFLAGS) -isrc -o $@ $<

%.o: %.c
	$(CC) $(DEFINES) $(CFLAGS) -Isrc -o $@ -c $<