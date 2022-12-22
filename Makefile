CC = /usr/local/i386elfgcc/bin/i386-elf-gcc
LD = /usr/local/i386elfgcc/bin/i386-elf-ld
ASMC = /bin/nasm
LDFLAGS = -nostdlib -L"/usr/local/i386elfgcc/lib/gcc/i386-elf/12.2.0" -lgcc
CFLAGS = -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Wno-unused-variable

# Sources
ASM_SOURCES = $(shell find src/ -type f -name '*.asm' ! -name 'bootsector.asm')
C_SOURCES = $(shell find src/ -type f -name '*.c')

# Phony Targets
.PHONY: clean

all: build/bootsector.bin build/bootloader.bin clean

# Targets
build/bootsector.bin: src/bootsector.asm
	${ASMC} $< -f bin -o $@

build/bootloader.bin: ${ASM_SOURCES:.asm=.o} ${C_SOURCES:.c=.o}
	${LD} -o $@ -T"link.ld" $^ ${LDFLAGS}

# Wildcard Targets
%.o: %.asm
	${ASMC} -isrc $< -f elf32 -o $@

%.o: %.c
	${CC} ${CFLAGS} -Isrc -I../nestos/libs -c $< -o $@
#TODO: Libs like this bad bad bad

# Clean Targets
clean:
	rm -f $(shell find ./src -type f -name '*.o')