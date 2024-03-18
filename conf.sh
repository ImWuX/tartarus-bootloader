#!/bin/sh

PREFIX="/usr/local"
ENV="dev"

while [[ $# -gt 0 ]]; do
    case $1 in
        --libgcc-dir=*)
            LIBGCC_DIR=${1#*=}
            ;;
        --prefix=*)
            PREFIX=${1#*=}
            ;;
        --target=*)
            TARGET=${1#*=}
            case $TARGET in
                x86_64-bios)
                    ;;
                *)
                    echo "Unknown target \"$TARGET\""
                    exit 1
                    ;;
            esac
            ;;
        --prod)
            ENV="prod"
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

if [ -z "$LIBGCC_DIR" ]; then
    >&2 echo "Missing libgcc-dir path"
    exit 1
fi

if [ -z "$TARGET" ]; then
    >&2 echo "No target provided"
    exit 1
fi

SRCDIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
DSTDIR=$(pwd)

cp $SRCDIR/Makefile $DSTDIR

CONFMK="$DSTDIR/conf.mk"
echo "export TARGET := $TARGET" > $CONFMK
echo "export ENV := $ENV" >> $CONFMK

echo "export PREFIX := $PREFIX" >> $CONFMK

echo "export SRC := $SRCDIR" >> $CONFMK
echo "export BUILD := $DSTDIR/build" >> $CONFMK
echo "export LIBGCC_DIR := $LIBGCC_DIR" >> $CONFMK

echo "export ASMC := nasm" >> $CONFMK
echo "export CC := x86_64-elf-gcc" >> $CONFMK
echo "export LD := x86_64-elf-ld" >> $CONFMK
echo "export OBJCOPY := x86_64-elf-objcopy" >> $CONFMK