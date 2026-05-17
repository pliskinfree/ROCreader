#!/bin/sh
# RGDS SDL2/Wayland display route test. It keeps Weston running and lets the
# compositor own page flipping.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_sdl_reader_files/rgds_sdl_reader_app"
LOG_FILE="$SCRIPT_DIR/rgds_sdl_reader_latest.log"
SECONDS_TO_RUN="${ROCREADER_RGDS_DEMO_SECONDS:-600}"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

log "[rgds_sdl_reader_sh] started: $(date 2>/dev/null || echo unknown)"
log "[rgds_sdl_reader_sh] binary=$BIN"
log "[rgds_sdl_reader_sh] seconds=$SECONDS_TO_RUN"
log "[rgds_sdl_reader_sh] uname=$(uname -a 2>/dev/null || true)"
log "[rgds_sdl_reader_sh] weston_pids=$(pidof weston 2>/dev/null || true)"
log "[rgds_sdl_reader_sh] incoming WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
log "[rgds_sdl_reader_sh] incoming XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-}"

if [ ! -f "$BIN" ]; then
  log "[rgds_sdl_reader_sh] missing binary: $BIN"
  exit 127
fi

chmod +x "$BIN" 2>/dev/null || true

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/0}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"

log "[rgds_sdl_reader_sh] using WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
log "[rgds_sdl_reader_sh] using XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
ls -l "$XDG_RUNTIME_DIR" >> "$LOG_FILE" 2>&1 || true

SDL_VIDEODRIVER=wayland \
ROCREADER_RGDS_LAYOUT="${ROCREADER_RGDS_LAYOUT:-dual}" \
ROCREADER_RGDS_DEMO_SECONDS="$SECONDS_TO_RUN" \
"$BIN" >> "$LOG_FILE" 2>&1
rc=$?
log "[rgds_sdl_reader_sh] executable exit code: $rc"
log "[rgds_sdl_reader_sh] done"
exit "$rc"
