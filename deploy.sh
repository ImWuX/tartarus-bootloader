#!/bin/bash
POSITIONAL_ARGS=()

VERSION="1.3"

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
            log "│ \t--create-image <mb>\n│ \t\t\tOverride the target image with a fresh image of specified size (Minumum: 128)\n│ "
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
command -v mktemp >/dev/null 2>&1 || { log_error "Missing dependency: mktemp"; exit 1; }

log_important "TARGET=$TARGET"

if [ -n "$1" ]; then
    IMAGE=$(realpath $1)
    if [ -n "$BASEDIR" ]; then
        BASEDIR=$(realpath $BASEDIR)
    fi

    if [ -n "$CREATEIMAGE" ]; then
        # Create a fresh image
        log_important "Creating a new image"
        dd if=/dev/zero of=$IMAGE bs=512 count=$(($CREATEIMAGE * 1024 * 1024 / 512))
    fi

    cd "$(dirname "$0")"
    if [ $? -eq 0 ]; then
        case $TARGET in
            amd64-bios)
                # Build the core
                log_important "Building core"
                make -C core TARGET="$TARGET"

                case $BOOT in
                    disk)
                        CORE_OFFSET=2048
                        CORE_SIZE=$(wc -c core/tartarus.sys | awk '{print $1}')
                        CORE_SIZE=$((($CORE_SIZE + 512) / 512))

                        # Create a partition for the core
                        log_important "Creating partition 1"
                        sgdisk -n=1:$CORE_OFFSET:$(($CORE_OFFSET + $CORE_SIZE)) $IMAGE
                        sgdisk -t=1:{54524154-5241-5355-424F-4F5450415254} $IMAGE # Tartarus Boot Partition GUID
                        sgdisk -A=1:set:0 $IMAGE

                        # Write core to the partition
                        log_important "Writing core to partition 1"
                        dd if=core/tartarus.sys of=$IMAGE seek=$CORE_OFFSET bs=512 conv=notrunc

                        # Build the bootsector
                        log_important "Building bootsector"
                        make -C bios disk/mbr.bin

                        # Write the bootsector to the image
                        log_important "Writing bootsector to image"
                        dd if=bios/disk/mbr.bin of=$IMAGE ibs=440 seek=0 obs=1 conv=notrunc
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
                # Build the core
                log_important "Building core"
                make -C core tartarus.efi TARGET="$TARGET" EFIDIR="$(realpath limine-efi)"

                case $BOOT in
                    disk)
                        ESP_OFFSET=2048
                        ESP_SIZE=$(wc -c core/tartarus.efi | awk '{print $1}')
                        ESP_SIZE=$(($ESP_SIZE * 2))
                        ESP_SIZE=$(($ESP_SIZE / 512))
                        if (( $ESP_SIZE < 65536 * 2 )); then
                            ESP_SIZE=$((65536 * 2))
                        fi

                        # Create the ESP partition
                        log_important "Creating ESP partition"
                        sgdisk -n=1:$ESP_OFFSET:$(($ESP_OFFSET + $ESP_SIZE)) $IMAGE
                        sgdisk -t=1:{C12A7328-F81F-11D2-BA4B-00A0C93EC93B} $IMAGE # EFI System Partition GUID

                        # Create a temp ESP partition file and dd it onto the main file
                        log_important "Writing ESP partition"
                        TMPIMG=$(mktemp /tmp/tartarus-XXXXX)
                        dd if=/dev/zero of=$TMPIMG bs=512 count=$ESP_SIZE
                        mkfs.fat -F 32 $TMPIMG
                        mmd -i $TMPIMG "::EFI"
                        mmd -i $TMPIMG "::EFI/BOOT"
                        mcopy -i $TMPIMG core/tartarus.efi "::EFI/BOOT/BOOTX64.EFI"
                        dd if=$TMPIMG of=$IMAGE seek=$ESP_OFFSET bs=512 count=$ESP_SIZE conv=notrunc
                        rm $TMPIMG
                        ;;
                    *)
                        log_error "Unsupported boot type ($BOOT)"
                        exit 1
                        ;;
                esac

                # Clean up
                log_important "Cleaning up"
                make -C core clean TARGET="$TARGET" EFIDIR="$(realpath limine-efi)"
                ;;
            *)
                log_error "Unsupported target ($TARGET)"
                exit 1
                ;;
        esac

        # Fill the rest of the image with a partition
        log_important "Creating partition 2"
        BIGPART_START=$(sgdisk -F $IMAGE)
        BIGPART_END=$(sgdisk -E $IMAGE)
        sgdisk -n=2:$BIGPART_START:$BIGPART_END $IMAGE
        sgdisk -t=2:{54524154-5241-5355-424F-4F5450415255} $IMAGE # GUID here is not important, can be anything really, tartarus wont care

        # Create a FAT FS and copy over the basedir
        log_important "Copying basedir to partition 2"
        mkfs.fat -F 32 --offset $BIGPART_START $IMAGE
        if [ -n "$BASEDIR" ]; then
            BYTEOFF=$(($BIGPART_START * 512))
            for FILE in $BASEDIR/*; do
                mcopy -s -i $IMAGE@@$BYTEOFF $FILE "::/"
            done
        fi
    else
        log_error "Make failed. Cleaning up"
        make clean
    fi
else
    log_error "Missing target image"
fi