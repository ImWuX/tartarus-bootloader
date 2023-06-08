#!/bin/bash
case $@ in
    bios)
        rm -f test.img
        ../deploy.sh -t amd64-bios -b disk --create-image 131072 -v test.img

        echo -e "\e[32mRunning QEMU in VNC (BIOS)\e[0m"
        qemu-system-x86_64 \
            -m 256M \
            -machine q35 \
            -drive format=raw,file=test.img \
            -smp cores=4 \
            -net none \
            \
            -vnc :0,websocket=on \
            -D ./log.txt -d int \
            -M smm=off \
            -k en-us \
            -monitor stdio
        ;;
    uefi)
        rm -f test.img
        ../deploy.sh --create-image fat32 -t amd64-uefi64 test.img -v

        echo -e "\e[32mRunning QEMU in VNC (UEFI)\e[0m"
        qemu-system-x86_64 \
            -m 256M \
            -machine q35 \
            -drive format=raw,file=test.img \
            -smp cores=4 \
            -net none \
            \
            -bios /usr/share/ovmf/OVMF.fd \
            \
            -vnc :0,websocket=on \
            -D ./log.txt -d int \
            -M smm=off \
            -k en-us \
            -monitor stdio
        ;;
esac




