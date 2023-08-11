#!/bin/bash

# set -x

CURRENT_SOURCE_DIR=$PWD

BUILD_WORK_DIR=$CURRENT_SOURCE_DIR/build_work

FFMPEG_SOURCE_DIR=$BUILD_WORK_DIR/FFmpeg
LIBPAG_SOURCE_DIR=$BUILD_WORK_DIR/libpag
LIBPAG_BUILD_DIR=$LIBPAG_SOURCE_DIR/linux/vendor
INSTALL_FFMPEG_DIR=$BUILD_WORK_DIR/ffmpeg_build

cd $FFMPEG_SOURCE_DIR

cp -rf $CURRENT_SOURCE_DIR/pag ./libavfilter
cp -f $CURRENT_SOURCE_DIR/vf_pagtransition.c ./libavfilter/vf_pagtransition.c

export MACOSX_DEPLOYMENT_TARGET=11.0

if [[ "$(uname -m)" == "arm64" ]]; then
  export ARCH=arm64
else
  export ARCH=x86_64
fi

export LDFLAGS=${LDFLAGS:-}
export CFLAGS=${CFLAGS:-}

function ensure_package () {
  if [[ "$ARCH" == "arm64" ]]; then
    if [[ ! -e "/opt/homebrew/opt/$1" ]]; then
      echo "Installing $1 using Homebrew"
      brew install "$1"
    fi

    export LDFLAGS="-L/opt/homebrew/opt/$1/lib ${LDFLAGS}"
    export CFLAGS="-I/opt/homebrew/opt/$1/include ${CFLAGS}"
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/homebrew/opt/$1/lib/pkgconfig"
  else
    if [[ ! -e "/usr/local/opt/$1" ]]; then
      echo "Installing $1 using Homebrew"
      brew install "$1"
    fi
    export LDFLAGS="-L/usr/local/opt/$1/lib ${LDFLAGS}"
    export CFLAGS="-I/usr/local/opt/$1/include ${CFLAGS}"
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/opt/$1/lib/pkgconfig"
  fi
}

ensure_package pkgconfig
ensure_package libtool
ensure_package glib
ensure_package lame
ensure_package opencore-amr
ensure_package theora
ensure_package libogg
ensure_package opus
ensure_package speex
ensure_package libvorbis
ensure_package libvpx
ensure_package x264
ensure_package xvid

MACOS_SDK=$(xcrun --sdk macosx --show-sdk-path)

export LDFLAGS="-L$LIBPAG_BUILD_DIR/pag/mac/x64 -lpag -lc++ ${LDFLAGS}"
export CFLAGS="-isysroot $MACOS_SDK -I$LIBPAG_BUILD_DIR/pag/include ${CFLAGS}"

echo "ARCH: $ARCH"
echo "LDFLAGS: $LDFLAGS"
echo "CFLAGS: $CFLAGS"

if [[ -f "config.h" ]]; then
    rm config.h
fi

# exit 0



# export CC=`xcrun --sdk macosx --find clang`
# export CXX=`xcrun --sdk macosx --find clang++`

./configure --prefix=$INSTALL_FFMPEG_DIR \
--enable-cross-compile \
--arch=$ARCH \
--cc="clang -arch $ARCH" \
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
--enable-videotoolbox \
--enable-audiotoolbox \
--disable-static \
--enable-shared \
--enable-filter=pagtransition
# --extra-cflags="-I$LIBPAG_BUILD_DIR/pag/include" \
# --extra-libs="-L$LIBPAG_BUILD_DIR/pag/mac/x64 -lpag"
# --extra-libs="-L$LIBPAG_BUILD_DIR/pag/mac/x64 -lpag -liconv -lc++ -lcompression" \
# --extra-libs="-framework ApplicationServices -framework AGL -framework OpenGL -framework QuartzCore -framework Cocoa -framework Foundation -framework VideoToolbox -framework CoreMedia"
# --extra-cflags="-isysroot $MACOS_SDK -std=c++11"


make install

cd $CURRENT_SOURCE_DIR
