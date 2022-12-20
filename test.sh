#!/bin/bash

dd if=/dev/zero of=build/test.img bs=512 count=131072
mkfs.fat -F 32 -n "MAIN_DRIVE" build/test.img
./deploy.sh build/test.img
echo -e "\e[32mRunning QEMU in VNC\e[0m"
qemu-system-x86_64 \
    -m 256M \
    -drive format=raw,file=build/test.img \
    -vnc :0,websocket=on \
    -machine q35 \
    -k en-us \
    -serial stdio \
    -s
rm build/test.img