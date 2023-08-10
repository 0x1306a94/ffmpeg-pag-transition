#!/bin/bash

set -x

SAVE_PWD_DIR=$PWD

FFMPEG_SOURCE_DIR=$PWD/FFmpeg
INSTALL_FFMPEG_DIR=$PWD/ffmpeg_build

cd $FFMPEG_SOURCE_DIR

if [[ -f "config.h" ]]; then
    rm config.h
fi

# MACOS_SDK=$(xcrun --sdk macosx --show-sdk-path)

# export CC=`xcrun --sdk macosx --find clang`
# export CXX=`xcrun --sdk macosx --find clang++`

./configure --prefix=../ffmpeg_build \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-postproc \
--enable-libass \
--enable-libfdk-aac \
--enable-libfreetype \
--enable-libmp3lame \
--enable-libopencore-amrnb \
--enable-libopencore-amrwb \
--enable-libopenjpeg \
--enable-openssl \
--enable-libopus \
--enable-libspeex \
--enable-libtheora \
--enable-libvorbis \
--enable-libvpx \
--enable-libx264 \
--enable-libxvid \
--disable-static \
--enable-shared \
--enable-filter=pagtransition \
--extra-cflags="-I$SAVE_PWD_DIR/libpag/linux/vendor/pag/include" \
--extra-libs="-L$SAVE_PWD_DIR/libpag/linux/vendor/pag/mac/x64 -lpag -liconv -lc++ -lcompression" \
--extra-libs="-framework ApplicationServices -framework AGL -framework OpenGL -framework QuartzCore -framework Cocoa -framework Foundation -framework VideoToolbox -framework CoreMedia"
# --extra-cflags="-isysroot $MACOS_SDK -std=c++11"


make install

cd $SAVE_PWD_DIR
