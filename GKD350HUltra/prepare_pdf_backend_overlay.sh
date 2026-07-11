#!/bin/sh
set -eu

SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SELF_DIR/.." && pwd)"
SYSROOT="${SYSROOT:-$SELF_DIR/sysroot_device}"
OVERLAY_SOURCE="${POPPLER_OVERLAY_SOURCE:-$REPO_ROOT/TrimuiBrick/sysroot_overlay}"
OVERLAY_LIB="$OVERLAY_SOURCE/usr/lib/aarch64-linux-gnu"
OVERLAY_BASE_LIB="$OVERLAY_SOURCE/lib/aarch64-linux-gnu"

copy_required() {
  src="$1"
  dst="$2"
  if [ ! -f "$src" ]; then
    echo "[gkd_pdf] ERROR: missing overlay library: $src"
    exit 1
  fi
  cp -f "$src" "$dst"
}

has_soname() {
  name="$1"
  find "$SYSROOT/usr/lib" "$SYSROOT/lib" -name "$name" -o -name "$name.*" 2>/dev/null | grep -q .
}

copy_optional_if_missing() {
  file_name="$1"
  soname="$2"
  source_dir="$3"
  if has_soname "$soname"; then
    return 0
  fi
  copy_required "$source_dir/$file_name" "$SYSROOT/usr/lib/$file_name"
  (
    cd "$SYSROOT/usr/lib"
    ln -sfn "$file_name" "$soname"
  )
}

if [ ! -d "$OVERLAY_SOURCE/usr/include/poppler" ]; then
  echo "[gkd_pdf] ERROR: Poppler overlay headers missing: $OVERLAY_SOURCE/usr/include/poppler"
  exit 1
fi

mkdir -p "$SYSROOT/usr/include/poppler" "$SYSROOT/usr/lib"
echo "[gkd_pdf] source: $OVERLAY_SOURCE"
echo "[gkd_pdf] target: $SYSROOT"
rsync -a "$OVERLAY_SOURCE/usr/include/poppler/" "$SYSROOT/usr/include/poppler/"

copy_required "$OVERLAY_LIB/libpoppler-cpp.so.0.2.1" "$SYSROOT/usr/lib/libpoppler-cpp.so.0.2.1"
copy_required "$OVERLAY_LIB/libpoppler.so.58.0.0" "$SYSROOT/usr/lib/libpoppler.so.58.0.0"
copy_required "$OVERLAY_LIB/libtiff.so.5.2.4" "$SYSROOT/usr/lib/libtiff.so.5.2.4"
copy_required "$OVERLAY_LIB/libjbig.so.0" "$SYSROOT/usr/lib/libjbig.so.0"
copy_required "$OVERLAY_BASE_LIB/libpng12.so.0.54.0" "$SYSROOT/usr/lib/libpng12.so.0.54.0"

(
  cd "$SYSROOT/usr/lib"
  ln -sfn libpoppler-cpp.so.0.2.1 libpoppler-cpp.so.0
  ln -sfn libpoppler-cpp.so.0.2.1 libpoppler-cpp.so
  ln -sfn libpoppler.so.58.0.0 libpoppler.so.58
  ln -sfn libpoppler.so.58.0.0 libpoppler.so
  ln -sfn libtiff.so.5.2.4 libtiff.so.5
  ln -sfn libtiff.so.5.2.4 libtiff.so
  ln -sfn libpng12.so.0.54.0 libpng12.so.0
  ln -sfn libjbig.so.0 libjbig.so
)

copy_optional_if_missing liblcms2.so.2.0.6 liblcms2.so.2 "$OVERLAY_LIB"
copy_optional_if_missing libjpeg.so.8.0.2 libjpeg.so.8 "$OVERLAY_LIB"
copy_optional_if_missing libfontconfig.so.1.9.0 libfontconfig.so.1 "$OVERLAY_LIB"
copy_optional_if_missing libfreetype.so.6.12.1 libfreetype.so.6 "$OVERLAY_LIB"
copy_optional_if_missing liblzma.so.5.0.0 liblzma.so.5 "$OVERLAY_BASE_LIB"
copy_optional_if_missing libexpat.so.1.6.0 libexpat.so.1 "$OVERLAY_BASE_LIB"
copy_optional_if_missing libz.so.1.2.8 libz.so.1 "$OVERLAY_BASE_LIB"

echo "[gkd_pdf] done"
