include ../config.mk

.PHONY: clean

%.bin: %.asm
	$(ASMC) $< -f bin -o $@

clean:
	rm -f $(shell find . -type f -name "*.bin")