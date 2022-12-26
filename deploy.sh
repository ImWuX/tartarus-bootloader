#!/bin/bash

if [ -z "$1" ]; then
    echo "No image to deploy on"
else
    FULL_PATH=$(realpath $1)
    cd "$(dirname "$0")"

    make
    if [ $? -eq 0 ]; then
        dd if=build/bootsector.bin of=$FULL_PATH ibs=422 seek=90 obs=1 conv=notrunc
        perl -e 'print pack("ccc",(0xEB,0x58,0x90))' | dd of=$FULL_PATH bs=1 seek=0 count=3 conv=notrunc
        mcopy -i $FULL_PATH build/bootloader.bin "::bootload.sys"
    else
        echo -e "\e[31mMake failed. Cleaning up\e[0m"
        make clean
    fi
fi