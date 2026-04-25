#!/bin/bash
set -euo pipefail

rm -rf /var/lib/apt/lists/*
rm -rf /etc/apt/sources.list.d/*
touch /tmp/empty-status
cat >/etc/apt/sources.list <<'EOF'
deb [arch=arm64 trusted=yes] http://ports.ubuntu.com/ubuntu-ports/ xenial main universe multiverse restricted
deb [arch=arm64 trusted=yes] http://ports.ubuntu.com/ubuntu-ports/ xenial-updates main universe multiverse restricted
deb [arch=arm64 trusted=yes] http://ports.ubuntu.com/ubuntu-ports/ xenial-security main universe multiverse restricted
EOF

export DEBIAN_FRONTEND=noninteractive
apt-get -o APT::Architecture=arm64 \
        -o APT::Architectures::=arm64 \
        -o Acquire::Check-Valid-Until=false \
        -o Dir::State::status=/tmp/empty-status \
        update

apt-get -o APT::Architecture=arm64 \
        -o APT::Architectures::=arm64 \
        -o Acquire::Check-Valid-Until=false \
        -o Dir::State::status=/tmp/empty-status \
        -o APT::Get::AllowUnauthenticated=true \
        -o Dir::Cache::Archives=/out/debs \
        -y --download-only install \
        libpoppler-cpp-dev \
        libmupdf-dev \
        mupdf-tools \
        libzip-dev

for deb in /out/debs/*.deb; do
  [ -f "$deb" ] || continue
  echo "[pdf-backends] extract ${deb##*/}"
  dpkg-deb -x "$deb" /out/overlay
done

mkdir -p /out/overlay/usr/lib/pkgconfig
mkdir -p /out/overlay/usr/lib/aarch64-linux-gnu/pkgconfig

if [ ! -f /out/overlay/usr/lib/pkgconfig/mupdf.pc ] && \
   [ -f /out/overlay/usr/include/mupdf/fitz.h ]; then
  {
    printf '%s\n' 'prefix=/usr'
    printf '%s\n' 'exec_prefix=${prefix}'
    printf '%s\n' 'libdir=${exec_prefix}/lib/aarch64-linux-gnu'
    printf '%s\n' 'includedir=${prefix}/include'
    printf '\n'
    printf '%s\n' 'Name: mupdf'
    printf '%s\n' 'Description: MuPDF PDF rendering library'
    printf '%s\n' 'Version: 1.7a'
    printf '%s\n' 'Cflags: -I${includedir}'
    printf '%s\n' 'Libs: -L${libdir} -lmupdf -lmupdf-js-none -lopenjpeg -ljbig2dec -ljpeg -lfreetype -lz -lm'
  } >/out/overlay/usr/lib/pkgconfig/mupdf.pc
fi

if [ ! -f /out/overlay/usr/lib/aarch64-linux-gnu/pkgconfig/libzip.pc ] && \
   [ -f /out/overlay/usr/include/zip.h ]; then
  {
    printf '%s\n' 'prefix=/usr'
    printf '%s\n' 'exec_prefix=${prefix}'
    printf '%s\n' 'libdir=${exec_prefix}/lib/aarch64-linux-gnu'
    printf '%s\n' 'includedir=${prefix}/include'
    printf '\n'
    printf '%s\n' 'Name: libzip'
    printf '%s\n' 'Description: library for handling zip archives'
    printf '%s\n' 'Version: 1.0.1'
    printf '%s\n' 'Cflags: -I${includedir}'
    printf '%s\n' 'Libs: -L${libdir} -lzip -lz'
  } >/out/overlay/usr/lib/aarch64-linux-gnu/pkgconfig/libzip.pc
fi

echo "[pdf-backends] overlay ready: /out/overlay"
find /out/overlay/usr/include -maxdepth 3 \( -iname 'fitz.h' -o -iname 'poppler-document.h' -o -iname 'zip.h' \) -print
find /out/overlay/usr/lib /out/overlay/usr/lib/aarch64-linux-gnu -maxdepth 2 \( -iname 'libmupdf*' -o -iname 'libpoppler*' -o -iname 'libzip*' \) -print 2>/dev/null | sort
