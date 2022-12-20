if [ -z "$1" ]; then
    echo "No image to deploy on"
else
    make
    if [ $? -eq 0 ]; then
        dd if=build/bootsector.bin of=$1 ibs=422 seek=90 obs=1 conv=notrunc
        perl -e 'print pack("ccc",(0xEB,0x58,0x90))' | dd of=$1 bs=1 seek=0 count=3 conv=notrunc
        mcopy -i $1 build/bootloader.bin "::bootload.sys"
    else
        echo -e "\e[31mMake failed. Cleaning up\e[0m"
        make clean
    fi
fi