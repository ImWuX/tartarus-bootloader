export TARGET="i386-elf"
export PREFIX="/usr/local/i386elfgcc"
export PATH="$PREFIX/bin:$PATH"

mkdir tmp
cd tmp
mkdir gcc-build
mkdir binutils-build

curl -O https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.gz
tar xvzf gcc-12.2.0.tar.gz
curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.39.tar.gz
tar xvzf binutils-2.39.tar.gz

cd binutils-build
../binutils-2.39/configure --target=$TARGET --enable-interwork --enable-multilib --disable-nls --disable-werror --prefix=$PREFIX
make all install
cd ..

cd gcc-12.2.0
./contrib/download_prerequisites
cd ../gcc-build
../gcc-12.2.0/configure --target=$TARGET --prefix=$PREFIX --disable-nls --disable-libssp --enable-languages=c --without-headers
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc

cd ../..
rm -r tmp