include ../../config.mk

BUILDDIR ?=
ifndef BUILDDIR
    $(error BUILDDIR is not specified)
endif

.PHONY: clean $(BUILDDIR)/boot.bin

$(BUILDDIR)/boot.bin: fat32.asm
	${ASMC} $< -f bin -o $@

clean:
	rm -f boot.bin