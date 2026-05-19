#!/bin/sh
set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SELF_DIR=$(script_dir)
APP_DIR="$SELF_DIR/ROCreader_RGDS"
BIN="$APP_DIR/rocreader_sdl"
LOG_FILE="$SELF_DIR/ROCreader_RGDS_latest.log"
LIB_FULL_DIR="$APP_DIR/lib"
LIB_SYSTEM_SDL_DIR="$APP_DIR/lib_system_sdl"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

log "[rgds_official] started: $(date 2>/dev/null || echo unknown)"
log "[rgds_official] app=$APP_DIR"
log "[rgds_official] bin=$BIN"
log "[rgds_official] weston_pids=$(pidof weston 2>/dev/null || true)"

if [ ! -x "$BIN" ]; then
  chmod +x "$BIN" 2>/dev/null || true
fi
if [ ! -f "$BIN" ]; then
  log "[rgds_official] missing binary"
  exit 127
fi

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/var/run}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_NOMOUSE="${SDL_NOMOUSE:-1}"
export ROCREADER_DEVICE_MODEL="${ROCREADER_DEVICE_MODEL:-rgds}"
export ROCREADER_SCREEN_PROFILE="${ROCREADER_SCREEN_PROFILE:-640x480}"
export ROCREADER_SCREEN_W="${ROCREADER_SCREEN_W:-640}"
export ROCREADER_SCREEN_H="${ROCREADER_SCREEN_H:-480}"
export ROCREADER_ROOT="$APP_DIR"
export ROCREADER_CARD1_ROOT="${ROCREADER_CARD1_ROOT:-/mnt/mmc}"
export ROCREADER_CARD2_ROOT="${ROCREADER_CARD2_ROOT:-/mnt/sdcard}"
export ROCREADER_SCAN_CARD2="${ROCREADER_SCAN_CARD2:-1}"
export ROCREADER_RUNTIME_LOG="${ROCREADER_RUNTIME_LOG:-1}"
export ROCREADER_VERBOSE_LOG="${ROCREADER_VERBOSE_LOG:-1}"
export ROCREADER_FULL_INPUT_LOG="${ROCREADER_FULL_INPUT_LOG:-0}"
export ROCREADER_LOG_MAX_BYTES="${ROCREADER_LOG_MAX_BYTES:-524288}"
export ROCREADER_RGDS_DUALSCREEN=1
export ROCREADER_RGDS_LAYOUT=spanning
export ROCREADER_PWR_SCRIPT="${ROCREADER_PWR_SCRIPT:-$APP_DIR/rgds_power_control.sh}"
export ROCREADER_BOOKS_ROOT="${ROCREADER_BOOKS_ROOT:-$APP_DIR/books}"
export ROCREADER_COVER_ROOT="${ROCREADER_COVER_ROOT:-$APP_DIR/book_covers}"

if [ -d "$LIB_SYSTEM_SDL_DIR" ]; then
  LIB_DIR="$LIB_SYSTEM_SDL_DIR"
else
  LIB_DIR="$LIB_FULL_DIR"
fi
export LD_LIBRARY_PATH="$LIB_DIR:$LIB_DIR/pulseaudio:/usr/lib:/lib:/mnt/vendor/lib:${LD_LIBRARY_PATH:-}"

mkdir -p "$XDG_RUNTIME_DIR" "$APP_DIR/cache" 2>/dev/null || true
chmod 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true

cd "$APP_DIR" || exit 126
log "[rgds_official] SDL_VIDEODRIVER=$SDL_VIDEODRIVER WAYLAND_DISPLAY=$WAYLAND_DISPLAY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
log "[rgds_official] ROCREADER_RGDS_LAYOUT=$ROCREADER_RGDS_LAYOUT"
log "[rgds_official] ROCREADER_PWR_SCRIPT=$ROCREADER_PWR_SCRIPT"
log "[rgds_official] LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
log "[rgds_official] books_root=$ROCREADER_BOOKS_ROOT"
log "[rgds_official] cover_root=$ROCREADER_COVER_ROOT"
uname -a >> "$LOG_FILE" 2>&1 || true
ls -la "$APP_DIR" >> "$LOG_FILE" 2>&1 || true

"$BIN" >> "$LOG_FILE" 2>&1
rc=$?
log "[rgds_official] exit code=$rc"
exit "$rc"
