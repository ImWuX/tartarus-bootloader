include ../config.mk

CFLAGS = -std=gnu2x -ffreestanding -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Wno-unused-variable -fno-stack-protector -fno-stack-check -fno-omit-frame-pointer -O2

.PHONY: clean

testkernel.elf: src/kernel.o
	${LD} -o $@ -T"link.ld" $^

# WILDCARD
%.o: %.asm
	$(ASMC) -isrc -f elf64 -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -Isrc -I.. -o $@ -c $<

# Clean
clean:
	rm -f $(shell find . -type f -name "*.o")