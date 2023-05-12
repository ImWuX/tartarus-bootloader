#!/bin/bash
POSITIONAL_ARGS=()

COLL="\e[96m"
COLE="\e[31m"
COLR="\e[0m"

TARGET="amd64-bios"

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--target)
            TARGET="$2"
            shift
            shift
            ;;
        --create-image)
            CREATEIMAGE="$2"
            shift
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -?|--help)
            echo -e "| "$COLL"Tartarus Deployer v1.1"$COLR""
            echo -e "| Usage: ./deploy [options] <image>"
            echo -e "| Options:"
            echo -e "| \t-t, --target <target>\n| \t\t\tSpecify target. Supported targets: amd64-bios, amd64-uefi\n| "
            echo -e "| \t-v, --verbose\n| \t\t\tTurn off log suppression for everything\n| "
            echo -e "| \t--create-image\n| \t\t\tCreate a 64MB raw disk image. Supported types: fat32\n| "
            echo -e "| \t-?, --help\n| \t\t\tDisplay usage help for Tartarus Deployer\n| "
            exit 1
            shift
            ;;
        -*|--*)
            echo "Unknown option ($1)"
            exit 1
            ;;
        *)
            POSITIONAL_ARGS+=("$1") # save positional arg
            shift # past argument
            ;;
    esac
done

set -- "${POSITIONAL_ARGS[@]}"

exec 3>&1
if [ -z "$VERBOSE" ]; then
    exec &>/dev/null
fi

echo -e ""$COLL"TARGET=$TARGET\e[0m" >&3

if [ -n "$1" ]; then
    IMAGE=$(realpath $1)

    if [ -n "$CREATEIMAGE" ]; then
        case "$CREATEIMAGE" in
            fat32)
                echo -e ""$COLL"Creating fat32 image (64mb)"$COLR"" >&3
                dd if=/dev/zero of=$IMAGE bs=512 count=131072
                mkfs.fat -F 32 -n "MAIN_DRIVE" $IMAGE
                ;;
            *)
                echo -e ""$COLE"Unknown image type ($CREATEIMAGE)"$COLR"" >&3
                exit 1
                ;;
        esac
    fi

    cd "$(dirname "$0")"
    if [ $? -eq 0 ]; then
        case $TARGET in
            amd64-bios)
                echo -e ""$COLL"Making bootsector"$COLR"" >&3
                make -C bios/boot boot.bin

                echo -e ""$COLL"Writing bootsector to image"$COLR"" >&3
                dd if=bios/boot/boot.bin of=$IMAGE ibs=422 seek=90 obs=1 conv=notrunc
                perl -e 'print pack("ccc",(0xEB,0x58,0x90))' | dd of=$IMAGE bs=1 seek=0 count=3 conv=notrunc

                echo -e ""$COLL"Making core"$COLR"" >&3
                make -C core TARGET="$TARGET"

                if [ "$CREATEIMAGE" == "fat32" ]; then
                    echo -e ""$COLL"Writing Tartarus into fat32 filesystem"$COLR"" >&3
                    mcopy -i $IMAGE core/tartarus.sys "::tartarus.sys"
                fi

                echo -e ""$COLL"Cleaning up"$COLR"" >&3
                make -C bios/boot clean
                make -C core clean TARGET="$TARGET"
                ;;
            amd64-uefi64)
                if [ ! -d "limine-efi" ]; then
                    git clone https://github.com/limine-bootloader/limine-efi.git limine-efi --depth=1
                fi

                echo -e ""$COLL"Making core"$COLR"" >&3
                make -C core tartarus.efi TARGET="$TARGET" EFIDIR="$(realpath limine-efi)"

                echo -e ""$COLL"Writing image"$COLR"" >&3
                dd if=/dev/zero of=$IMAGE bs=512 count=131072
                parted -s $IMAGE mklabel gpt
                parted -s $IMAGE mkpart ESP fat32 2048s 100%
                parted -s $IMAGE set 1 esp on

                dd if=/dev/zero of=tmp.img bs=512 count=100000
                mkfs.fat -F 32 tmp.img
                mmd -i tmp.img "::EFI"
                mmd -i tmp.img "::EFI/BOOT"
                mcopy -i tmp.img core/tartarus.efi "::EFI/BOOT/BOOTX64.EFI"
                dd if=tmp.img of=$IMAGE bs=512 count=100000 seek=2048 conv=notrunc

                echo -e ""$COLL"Cleaning up"$COLR"" >&3
                make -C core clean TARGET="$TARGET" EFIDIR="$(realpath limine-efi)"
                ;;
            *)
                echo -e ""$COLE"Unsupported target ($TARGET)"$COLR""
                exit 1
                ;;
        esac
    else
        echo -e ""$COLE"Make failed. Cleaning up"$COLR"" >&3
        make clean
    fi
else
    echo -e ""$COLE"Missing destination image"$COLR"" >&3
fi