# Tartarus Config File (TCF)

## Format
The TCF should be encoded in ASCII and follow the format described below:
- The file consists of key-value pairs on every line.
- Each key-value pair should be separated by an equal sign (`=`).
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