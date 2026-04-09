#!/bin/sh
set -eu

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$SELF_DIR/ROCreader"
BIN="$APP_DIR/rocreader_sdl"
LOG_FILE="${ROC_NATIVE_RUNTIME_LOG:-$SELF_DIR/ROCreader.log}"
LIB_FULL_DIR="$APP_DIR/lib"
LIB_SYSTEM_SDL_DIR="$APP_DIR/lib_system_sdl"
LIB_DIR="$LIB_FULL_DIR"
UPDATE_STATUS_FILE="$APP_DIR/cache/update_boot_status.txt"
UPDATE_STAGE_DIR="$APP_DIR/cache/update_stage"
INSTALLED_VERSION_FILE="$APP_DIR/version.txt"

export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_NOMOUSE="${SDL_NOMOUSE:-1}"
export ROCREADER_ROOT="$APP_DIR"
export ROCREADER_CARD1_ROOT="/mnt/mmc"
export ROCREADER_CARD2_ROOT="/mnt/sdcard"
if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
  export XDG_RUNTIME_DIR="/tmp/rocreader-xdg"
fi
mkdir -p "$XDG_RUNTIME_DIR" 2>/dev/null || true
chmod 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true
cd "$APP_DIR"

set_runtime_libs() {
  lib_dir="$1"
  if [ -d "$lib_dir" ]; then
    LIB_DIR="$lib_dir"
  else
    LIB_DIR="$LIB_FULL_DIR"
  fi
  export LD_LIBRARY_PATH="$LIB_DIR:$LIB_DIR/pulseaudio:/usr/lib32:/usr/lib:/lib:/mnt/vendor/lib:${LD_LIBRARY_PATH_BASE:-}"
}

log_line() {
  printf '%s\n' "$1" >>"$LOG_FILE"
}

write_update_status() {
  result="$1"
  version="${2:-}"
  mkdir -p "$(dirname "$UPDATE_STATUS_FILE")" 2>/dev/null || true
  {
    printf 'result=%s\n' "$result"
    [ -n "$version" ] && printf 'version=%s\n' "$version"
  } >"$UPDATE_STATUS_FILE"
}

read_installed_version() {
  [ -f "$INSTALLED_VERSION_FILE" ] || return 0
  sed -n '1p' "$INSTALLED_VERSION_FILE"
}

write_installed_version() {
  version="$1"
  [ -n "$version" ] || return 0
  printf '%s\n' "$version" >"$INSTALLED_VERSION_FILE"
}

find_pending_marker() {
  for root in /mnt/mmc /mnt/sdcard; do
    marker="$root/Downloads/ROCreader_update_pending.txt"
    [ -f "$marker" ] && { printf '%s' "$marker"; return 0; }
  done
  return 1
}

extract_marker_value() {
  key="$1"
  marker="$2"
  awk -F= -v wanted="$key" '$1 == wanted { print substr($0, index($0, "=") + 1); exit }' "$marker"
}

extract_version_from_name() {
  file_name="$(basename "$1")"
  printf '%s\n' "$file_name" | sed -n 's/.*\(ver[0-9][0-9.]*\)\.zip$/\1/p'
}

version_sort_key() {
  version="$1"
  printf '%s\n' "$version" | awk '
    BEGIN { count = 0 }
    {
      n = split($0, parts, /[^0-9]+/)
      for (i = 1; i <= n; ++i) {
        if (parts[i] != "") {
          printf "%06d", parts[i]
          count++
        }
      }
    }
    END {
      if (count == 0) printf "000000"
    }'
}

version_is_newer() {
  candidate="$1"
  baseline="$2"
  [ "$(version_sort_key "$candidate")" \> "$(version_sort_key "$baseline")" ]
}

find_latest_download_zip() {
  best_path=""
  best_version=""
  for root in /mnt/mmc /mnt/sdcard; do
    downloads_dir="$root/Downloads"
    [ -d "$downloads_dir" ] || continue
    for zip_file in "$downloads_dir"/*.zip; do
      [ -f "$zip_file" ] || continue
      zip_version="$(extract_version_from_name "$zip_file")"
      [ -n "$zip_version" ] || continue
      if [ -z "$best_path" ] || version_is_newer "$zip_version" "$best_version"; then
        best_path="$zip_file"
        best_version="$zip_version"
      fi
    done
  done
  [ -n "$best_path" ] && printf '%s\n' "$best_path"
}

extract_zip_to_stage() {
  zip_file="$1"
  stage_dir="$2"
  rm -rf "$stage_dir"
  mkdir -p "$stage_dir"
  if command -v unzip >/dev/null 2>&1; then
    unzip -oq "$zip_file" -d "$stage_dir" >>"$LOG_FILE" 2>&1
    return $?
  fi
  if command -v busybox >/dev/null 2>&1; then
    busybox unzip -o "$zip_file" -d "$stage_dir" >>"$LOG_FILE" 2>&1
    return $?
  fi
  return 127
}

replace_runtime_entry() {
  name="$1"
  src="$2/$name"
  dst="$APP_DIR/$name"
  [ -e "$src" ] || return 0
  rm -rf "$dst"
  cp -a "$src" "$APP_DIR/"
}

perform_pending_update_if_any() {
  marker="$(find_pending_marker || true)"
  installed_version="$(read_installed_version || true)"
  package_path=""
  package_version=""
  latest_zip="$(find_latest_download_zip || true)"
  latest_version=""
  if [ -n "$latest_zip" ]; then
    latest_version="$(extract_version_from_name "$latest_zip")"
  fi
  if [ -n "$latest_zip" ] && [ -n "$latest_version" ]; then
    if [ -z "$installed_version" ] || version_is_newer "$latest_version" "$installed_version"; then
      package_path="$latest_zip"
      package_version="$latest_version"
    fi
  fi
  if [ -z "$package_path" ] && [ -n "$marker" ]; then
    package_dir="$(dirname "$marker")"
    package_name="$(extract_marker_value filename "$marker")"
    package_version="$(extract_marker_value version "$marker")"
    package_path="$package_dir/$package_name"
    if [ -z "$package_version" ]; then
      package_version="$(extract_version_from_name "$package_path")"
    fi
  fi
  [ -n "$package_path" ] || return 0

  staged_runtime="$UPDATE_STAGE_DIR/Roms/APPS/ROCreader"
  staged_launcher="$UPDATE_STAGE_DIR/Roms/APPS/ROCreader.sh"

  log_line "[update] pending marker: $marker"
  log_line "[update] installed version: ${installed_version:-unknown}"
  log_line "[update] latest zip: ${latest_zip:-none}"
  log_line "[update] package: $package_path"
  log_line "[update] package version: ${package_version:-unknown}"

  if [ ! -f "$package_path" ]; then
    log_line "[update] missing package, skip install"
    write_update_status "failed" "$package_version"
    return 0
  fi

  if ! extract_zip_to_stage "$package_path" "$UPDATE_STAGE_DIR"; then
    log_line "[update] extract failed"
    write_update_status "failed" "$package_version"
    return 0
  fi

  if [ ! -d "$staged_runtime" ]; then
    log_line "[update] staged runtime missing: $staged_runtime"
    write_update_status "failed" "$package_version"
    rm -rf "$UPDATE_STAGE_DIR"
    return 0
  fi

  replace_runtime_entry "rocreader_sdl" "$staged_runtime"
  replace_runtime_entry "ui.pack" "$staged_runtime"
  replace_runtime_entry "native_config.ini" "$staged_runtime"
  replace_runtime_entry "native_keymap.ini" "$staged_runtime"
  replace_runtime_entry "fonts" "$staged_runtime"
  replace_runtime_entry "sounds" "$staged_runtime"
  replace_runtime_entry "lib" "$staged_runtime"
  replace_runtime_entry "lib_system_sdl" "$staged_runtime"

  if [ -f "$staged_launcher" ]; then
    cp "$staged_launcher" "$SELF_DIR/ROCreader.sh.new"
    mv "$SELF_DIR/ROCreader.sh.new" "$SELF_DIR/ROCreader.sh"
  fi

  chmod +x "$APP_DIR/rocreader_sdl" 2>/dev/null || true
  chmod +x "$SELF_DIR/ROCreader.sh" 2>/dev/null || true

  write_installed_version "$package_version"
  rm -f /mnt/mmc/Downloads/ROCreader_update_pending.txt /mnt/sdcard/Downloads/ROCreader_update_pending.txt
  rm -rf "$UPDATE_STAGE_DIR"
  write_update_status "success" "$package_version"
  log_line "[update] install success version=${package_version:-unknown}"
}

run_with_driver() {
  drv="$1"
  mode="$2"
  lib_dir="$3"
  set_runtime_libs "$lib_dir"
  SDL_VIDEODRIVER="$drv" "$BIN" >>"$LOG_FILE" 2>&1
}

run_default() {
  mode="$1"
  lib_dir="$2"
  set_runtime_libs "$lib_dir"
  "$BIN" >>"$LOG_FILE" 2>&1
}

LD_LIBRARY_PATH_BASE="${LD_LIBRARY_PATH:-}"
set_runtime_libs "$LIB_SYSTEM_SDL_DIR"

if [ ! -x "$BIN" ]; then
  log_line "[launcher] binary missing: $BIN"
  exit 4
fi

log_line "===== $(date '+%F %T %Z') ====="
perform_pending_update_if_any

if [ -n "${SDL_VIDEODRIVER:-}" ]; then
  run_with_driver "$SDL_VIDEODRIVER" "forced" "$LIB_FULL_DIR"
  exit $?
fi

try_mode() {
  mode="$1"
  lib_dir="$2"

  if run_default "$mode" "$lib_dir"; then
    exit 0
  fi

  for drv in KMSDRM kmsdrm wayland x11; do
    if run_with_driver "$drv" "$mode" "$lib_dir"; then
      exit 0
    fi
  done
}

try_mode "system_sdl" "$LIB_SYSTEM_SDL_DIR"
try_mode "full" "$LIB_FULL_DIR"
log_line "[launcher] all drivers failed"
exit 5
