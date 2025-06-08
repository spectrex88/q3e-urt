#!/bin/sh

export MALLOC_CHECK_=0
export GLIBC_TUNABLES=glibc.malloc.check=0

ulimit -c unlimited

mkdir -p test

make clean release BUILD_CLIENT=0 BUILD_SERVER=1 \
  SERVER_CFLAGS="-DDUMMY_DEFINE -fno-stack-protector -D_FORTIFY_SOURCE=0 -Wno-format-security -fno-stack-check"

build/release-linux-x86_64/quake3e.ded.x64 \
  +set com_hunkmegs 192 \
  +set dedicated 2 \
  +set fs_basepath . \
  +set fs_libpath . \
  +set fs_homepath ./test \
  +set developer 1 \
  +exec server
