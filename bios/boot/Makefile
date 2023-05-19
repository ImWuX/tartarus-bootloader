include ../../config.mk

.PHONY: clean boot.bin

boot.bin: fat32.asm
	$(ASMC) $< -f bin -o $@

clean:
	rm -f boot.bin