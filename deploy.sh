#!/bin/bash
POSITIONAL_ARGS=()

VERSION="1.2"

COLI="\e[96m"
COLE="\e[31m"
COLB="\e[1m"
COLU="\e[4m"
COLR="\e[0m"

log() {
    echo -e "$@"
}
log_important() {
    echo -e ""$COLI"$@"$COLR"" >&3
}
log_error() {
    echo -e ""$COLE"$@"$COLR"" >&3
}

TARGET="amd64-bios"
BOOT="disk"

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--target)
            TARGET="$2"
            shift
            ;;
        -b|--boot)
            BOOT="$2"
            shift
            ;;
        --create-image)
            CREATEIMAGE="$2"
            shift
            ;;
        --basedir)
            BASEDIR="$2"
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            ;;
        -?|--help)
            log "┌────────────────────────────────────────────────────────────────────────"
            log "│ "$COLI"Tartarus Deployer v$VERSION"$COLR""
            log "│ Usage: ./deploy [options] <image>"
            log "│ Options:"
            log "│ \t-t, --target <target>\n│ \t\t\tSpecify target. Supported targets: "$COLU"amd64-bios"$COLR", amd64-uefi\n│ "
            log "│ \t-b, --boot <type>\n│ \t\t\tBoot type. Supported types: "$COLU"disk"$COLR"\n│ "
            log "| \t--basedir\n| \t\t\tSpecify a directory to copy over as the root directory\n|"
            log "│ \t--create-image <sector count>\n│ \t\t\tOverride the target image with a fresh image of specified size\n│ "
            log "│ \t-v, --verbose\n│ \t\t\tTurn off log suppression for everything\n│ "
            log "│ \t-?, --help\n│ \t\t\tDisplay usage help for Tartarus Deployer\n│ "
            exit 1
            ;;
        -*|--*)
            echo "Unknown option \"$1\""
            exit 1
            ;;
        *)
            POSITIONAL_ARGS+=("$1") # save positional arg
            ;;
    esac
    shift
done

set -- "${POSITIONAL_ARGS[@]}"

exec 3>&1
if [ -z "$VERBOSE" ]; then
    exec &>/dev/null
fi

command -v make >/dev/null 2>&1 || { log_error "Missing dependency: make"; exit 1; }
command -v mcopy >/dev/null 2>&1 || { log_error "Missing dependency: mtools/mcopy"; exit 1; }
command -v mmd >/dev/null 2>&1 || { log_error "Missing dependency: mtools/mmd"; exit 1; }
command -v dd >/dev/null 2>&1 || { log_error "Missing dependency: coreutils/dd"; exit 1; }
command -v sgdisk >/dev/null 2>&1 || { log_error "Missing dependency: sgdisk"; exit 1; }

log_important "TARGET=$TARGET"

if [ -n "$1" ]; then
    IMAGE=$(realpath $1)
    BASEDIR=$(realpath $BASEDIR)

    if [ -n "$CREATEIMAGE" ]; then
        # Create a fresh image
        log_important "Creating a new image"
        dd if=/dev/zero of=$IMAGE bs=512 count=$CREATEIMAGE
    fi

    cd "$(dirname "$0")"
    if [ $? -eq 0 ]; then
        case $TARGET in
            amd64-bios)
                # Build the core
                log_important "Making core"
                make -C core TARGET="$TARGET"

                case $BOOT in
                    disk)
                        # Build the bootsector
                        log_important "Building mbr bootsector"
                        make -C bios disk/mbr.bin

                        # Create a new partition table
                        CORE_SIZE=$(wc -c core/tartarus.sys | awk '{print $1}')
                        CORE_SIZE=$((($CORE_SIZE + 512) / 512))

                        sgdisk -n=1:2048:$((2048 + $CORE_SIZE)) $IMAGE
                        sgdisk -t=1:{54524154-5241-5355-424F-4F5450415254} $IMAGE
                        sgdisk -A=1:set:0 $IMAGE

                        BIGPART_START=$(sgdisk -F $IMAGE)
                        BIGPART_END=$(sgdisk -E $IMAGE)
                        sgdisk -n=2:$BIGPART_START:$BIGPART_END $IMAGE
                        sgdisk -t=2:{54524154-5241-5355-424F-4F5450415255} $IMAGE # GUID here is not important, can be anything really, tartarus wont care
                        mkfs.fat -F 16 --offset $BIGPART_START $IMAGE
                        if [ -n "$BASEDIR" ]; then
                            BYTEOFF=$(($BIGPART_START * 512))
                            mcopy -s -i $IMAGE@@$BYTEOFF $BASEDIR "::/root"
                            mmove -i $IMAGE@@$BYTEOFF "::/root/*" "::/"
                            mdeltree -i $IMAGE@@$BYTEOFF "::/root"
                            mdir -/ -i $IMAGE@@$BYTEOFF
                        fi

                        # Write the bootsector to the image
                        log_important "Writing bootsector to image"
                        dd if=bios/disk/mbr.bin of=$IMAGE ibs=440 seek=0 obs=1 conv=notrunc

                        # Write core to the image
                        dd if=core/tartarus.sys of=$IMAGE seek=2048 bs=512 conv=notrunc
                        ;;
                    *)
                        log_error "Unsupported boot type ($BOOT)"
                        exit 1
                        ;;
                esac

                # Clean up
                log_important "Cleaning up"
                make -C bios clean
                make -C core clean TARGET="$TARGET"
                ;;
            amd64-uefi64)
                case $BOOT in
                    disk)
                        ;;
                    *)
                        log_error "Unsupported boot type ($BOOT)"
                        exit 1
                        ;;
                esac
                # Clone limine-efi if needed
                if [ ! -d "limine-efi" ]; then
                    git clone https://github.com/limine-bootloader/limine-efi.git limine-efi --depth=1
                fi

                # Build the core
                log_important "Making core"
                make -C core tartarus.efi TARGET="$TARGET" EFIDIR="$(realpath limine-efi)"

                # Write it to the image
                log_important "Writing image"
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
                rm -f tmp.img

                # Clean up
                log_important "Cleaning up"
                make -C core clean TARGET="$TARGET" EFIDIR="$(realpath limine-efi)"
                ;;
            *)
                log_error "Unsupported target ($TARGET)"
                exit 1
                ;;
        esac
    else
        log_error "Make failed. Cleaning up"
        make clean
    fi
else
    log_error "Missing target image"
fi