# Tartarus
Tartarus is a 64bit bootloader. Currently only targetting AMD64.
Note that this project is very much still in development.

[config.md](config.md) describes the configuration file tartarus uses to boot.

Tartarus uses [Limine EFI](https://github.com/limine-bootloader/limine-efi) for the efi buildsystem.

[Tartarus Protocol](./protocol.md) (**NOTE:** Currently the protocol is still being actively written and there is no guarantee breaking changes won't occur without notice)

- TODO: Load a GDT on the BSP for UEFI