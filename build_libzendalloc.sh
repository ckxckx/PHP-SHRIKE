#!/bin/sh

set -e

currdir="$(pwd)"
install_dir="$currdir/install"
make clean

cd libzendalloc
make clean
make

cd "$currdir"

cp libzendalloc/orig/Makefile.am Zend/

./configure --prefix="$install_dir" \
    --enable-zip --enable-shrike --enable-dve --enable-mbstring --enable-intl \
    --enable-exif --with-gd CXXFLAGS="-DU_USING_ICU_NAMESPACE=1 "\
    CFLAGS="-DU_USING_ICU_NAMESPACE=1 -O2" LDFLAGS="-L ./libzendalloc" \
    LIBS="-lzendalloc"

make -j 5

make install
