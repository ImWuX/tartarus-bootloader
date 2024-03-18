# Expects TARGET, CC, LD, OBJCOPY, ASMC, LIBGCC_DIR, BUILD

ifneq (, $(filter $(TARGET), x86_64-bios))
    ARCH := x86_64
endif

# Sources
C_SOURCES := $(shell find src/common -type f -name *.c)

ifeq ($(ARCH), x86_64)
    C_SOURCES += $(shell find src/x86_64/common -type f -name *.c)
    ASM_SOURCES := $(shell find src/x86_64/common -type f -name *.asm)
endif

ifeq ($(TARGET), x86_64-bios)
    C_SOURCES += $(shell find src/x86_64/bios -type f -name *.c)
    ASM_SOURCES += $(shell find src/x86_64/bios -type f -name *.asm)
endif

ASM_OBJ := $(patsubst %.asm,$(BUILD)/core/%.o,$(ASM_SOURCES))
C_OBJ := $(patsubst %.c,$(BUILD)/core/%.o,$(C_SOURCES))

# Flags
DEFINES :=
ASMFLAGS := -i$(SRC) -i$(SRC)/core/src
CFLAGS := -I$(SRC) -I$(SRC)/core/src -std=gnu2x -O2 -ffreestanding -fno-stack-protector -fno-stack-check -fno-omit-frame-pointer -fno-strict-aliasing -fno-lto --param min-pagesize=0
CFLAGS += -Wall -Wextra -Wshadow -Wvla -Wno-unused-variable
LDFLAGS := -nostdlib -static -z noexecstack

ifeq ($(ARCH), x86_64)
    DEFINES += -D __ARCH_X86_64
    CFLAGS += -mabi=sysv -mgeneral-regs-only
    LDFLAGS += -z common-page-size=0x1000 -z max-page-size=0x1000
endif

ifeq ($(TARGET), x86_64-bios)
    DEFINES += -D __TARTARUS_NO_PTR
    ASMFLAGS += -f elf32
    CFLAGS += -m32 -march=i686
endif

# Final Targets
ifeq ($(TARGET), x86_64-bios)
$(BUILD)/core/tartarus.sys: $(BUILD)/core/tartarus.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/core/tartarus.elf: $(C_OBJ) $(ASM_OBJ) $(LIBGCC_DIR)/i686.a
	$(LD) $(LDFLAGS) -T"link.x86_64-bios.ld" -o $@ $^
endif

# Wildcard Targets
$(BUILD)/core/%.o: %.asm
	mkdir -p $(@D)
	$(ASMC) $(DEFINES) $(ASMFLAGS) -o $@ $<

$(BUILD)/core/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(DEFINES) $(CFLAGS) -o $@ -c $<
