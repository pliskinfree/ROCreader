#!/bin/sh
set -eu

ROOT=/work
SRC="$ROOT/experimental/curl-impersonate-lexiforest"
BUILD="$ROOT/experimental/curl-impersonate-v155-build-aarch64"
SYSROOT="${SYSROOT:-$ROOT/H700/sysroot_device}"
OUT_DIR="${OUT_DIR:-$ROOT/experimental/wn04_transport_v155_out}"
HOST=aarch64-linux-gnu

if [ ! -d "$SRC" ]; then
  echo "[wn04-curl-v155] missing source: $SRC" >&2
  exit 2
fi
if [ ! -d "$SYSROOT" ]; then
  echo "[wn04-curl-v155] missing sysroot: $SYSROOT" >&2
  exit 2
fi

mkdir -p "$BUILD/toolchain/bin" "$OUT_DIR/bin"

find "$SRC" -type f \( -name configure -o -name config.sub -o -name config.guess -o -name '*.sh' -o -name '*.patch' -o -name 'curl_*' \) \
  -exec sed -i 's/\r$//' {} +

cd "$BUILD"

if [ "${WN04_V155_CLEAN:-0}" = "1" ]; then
  # Re-run this experimental line from a clean source tree while keeping downloaded tarballs.
  # This avoids patch collisions from partially completed previous builds.
  rm -rf \
    "$BUILD/install" \
    "$BUILD/brotli-1.2.0" \
    "$BUILD/curl-8_15_0" \
    "$BUILD/libidn2-2.3.7" \
    "$BUILD/nghttp2-1.63.0" \
    "$BUILD/nghttp3-1.15.0" \
    "$BUILD/ngtcp2-1.20.0" \
    "$BUILD/zstd-1.5.6"
  find "$BUILD" -maxdepth 1 -type d -name 'boringssl-*' -exec rm -rf {} +
fi

cat > "$BUILD/curl" <<'EOF'
#!/bin/sh
exec /usr/bin/curl --retry 8 --retry-delay 3 --retry-all-errors --connect-timeout 30 "$@"
EOF
chmod +x "$BUILD/curl"

cat > "$BUILD/toolchain/bin/$HOST-gcc" <<EOF
#!/bin/sh
exec /usr/bin/$HOST-gcc --sysroot="$SYSROOT" "\$@"
EOF
cat > "$BUILD/toolchain/bin/$HOST-g++" <<EOF
#!/bin/sh
exec /usr/bin/$HOST-g++ --sysroot="$SYSROOT" "\$@"
EOF
chmod +x "$BUILD/toolchain/bin/$HOST-gcc" "$BUILD/toolchain/bin/$HOST-g++"

export PATH="$BUILD:$BUILD/toolchain/bin:$PATH"
export CC="$HOST-gcc"
export CXX="$HOST-g++"
export AR="$HOST-ar"
export RANLIB="$HOST-ranlib"
export STRIP="$HOST-strip"
export CFLAGS="${CFLAGS:-} -O2"
export CXXFLAGS="${CXXFLAGS:-} -O2 -static-libstdc++ -static-libgcc"
export LDFLAGS="${LDFLAGS:-} -static-libstdc++ -static-libgcc"
unset PKG_CONFIG_SYSROOT_DIR
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/usr/share/pkgconfig"

"$SRC/configure" \
  --host="$HOST" \
  --prefix="$BUILD/install" \
  --enable-static \
  --with-ca-path=/etc/ssl/certs \
  --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt

make build SUBJOBS="${SUBJOBS:-2}"
make install-strip

cp "$BUILD/install/bin/curl-impersonate" "$OUT_DIR/bin/curl-impersonate"
for target in chrome120 chrome124 chrome131 chrome133a chrome136 chrome142 chrome146 chrome131_android; do
  cp "$SRC/bin/curl_$target" "$OUT_DIR/bin/curl_$target"
  sed -i '1s|.*|#!/bin/sh|' "$OUT_DIR/bin/curl_$target"
  chmod +x "$OUT_DIR/bin/curl_$target"
done
chmod +x "$OUT_DIR/bin/curl-impersonate"

file "$OUT_DIR/bin/curl-impersonate" | tee "$OUT_DIR/curl_impersonate_file.txt"
"$HOST-readelf" -d "$OUT_DIR/bin/curl-impersonate" > "$OUT_DIR/curl_impersonate_dynamic.txt" || true
"$HOST-readelf" --version-info "$OUT_DIR/bin/curl-impersonate" > "$OUT_DIR/curl_impersonate_version_info.txt" || true
strings "$OUT_DIR/bin/curl-impersonate" | grep -E '^GLIBC_[0-9]' | sort -V | tail -20 > "$OUT_DIR/curl_impersonate_glibc_symbols.txt" || true

echo "[wn04-curl-v155] output: $OUT_DIR/bin/curl-impersonate"
