#!/usr/bin/env bash

./configure --prefix=`pwd`/install \
	--enable-mbstring --enable-intl --enable-shrike --with-gd CFLAGS=-g

make -j 2
