# Tartarus Config File (TCF)

## Format
The TCF should be encoded in ASCII and follow the format described below:
- The file consists of key-value pairs on every line.
- Each key-value pair should be separated by an equal sign ( `=` ).
- Both spaces and tabs are allowed around the equal sign and will be ignored.

Example:
```
a_key=somestring
somekey = A string separated by spaces
anotherkey = 432141
```

## Location
Tartarus will search every FAT12/16/32 partition of every disk at the following locations:
- `/tartarus.cfg`
- `/tartarus/config.cfg`

## Options
|Key|Value|Required|Description|
|-|-|-|-|
|KERNEL|PATH|Yes|The path of the kernel file. Currently this path is relative to the disk the config is found on.|
|PROTOCOL|`tartarus`|Yes|Which boot protocol tartarus should use to boot the kernel.|
|TRTRS_PHYS_BOOT_INFO|BOOL|No|(TARTARUS PROTOCOL ONLY) This option controls whether the `boot_info` struct will be in the HHDM or physical memory. Default: `false`|
|MODULE|PATH|No|Path to a file to be loaded as a module. Currently this path is relative to the disk the config is found on. It is possible to define this key multiple times for different modules.|

### Path
Paths consist of file/directory names separated by slashes ( `/` ). The names should only contains alphanumeric characters (`a-zA-Z0-9`). Example: `/sys/kernel.elf`