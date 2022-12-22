CC = /usr/local/x86_64elfgcc/bin/x86_64-elf-gcc
LD = /usr/local/x86_64elfgcc/bin/x86_64-elf-ld
LDFLAGS = -nostdlib
CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Wno-unused-variable

.PHONY: clean

all: kernel.elf clean

kernel.elf: kernel.o
	${LD} ${LDFLAGS} -o $@ -T"link.ld" $^

%.o: %.c
	${CC} ${CFLAGS} -I../shared -c $< -o $@

clean:
	rm -f $(shell find . -type f -name '*.o')
