#!/bin/sh
# RGDS dedicated platform prototype launcher.
# Put this script next to the prebuilt rgds_platform_demo binary on the SD card,
# then run it from the device UI or through sh.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_platform_demo_files/rgds_platform_demo"
LOG_FILE="$SCRIPT_DIR/rgds_platform_demo_latest.log"
SECONDS_TO_RUN="${ROCREADER_RGDS_DEMO_SECONDS:-300}"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

log "[rgds_demo_sh] started: $(date 2>/dev/null || echo unknown)"
log "[rgds_demo_sh] script=$0"
log "[rgds_demo_sh] script_dir=$SCRIPT_DIR"
log "[rgds_demo_sh] binary=$BIN"
log "[rgds_demo_sh] seconds=$SECONDS_TO_RUN"
log "[rgds_demo_sh] SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}"
log "[rgds_demo_sh] PATH=${PATH:-}"
log "[rgds_demo_sh] uname=$(uname -a 2>/dev/null || true)"
log "[rgds_demo_sh] log=$LOG_FILE"

if [ ! -f "$BIN" ]; then
  legacy_bin="$SCRIPT_DIR/rgds_platform_demo"
  if [ -f "$legacy_bin" ]; then
    BIN="$legacy_bin"
    log "[rgds_demo_sh] using legacy root binary: $BIN"
  else
    log "[rgds_demo_sh] missing binary: $BIN"
    exit 127
  fi
fi

chmod +x "$BIN" 2>/dev/null || true

if [ -z "${SDL_VIDEODRIVER:-}" ]; then
  SDL_VIDEODRIVER=KMSDRM ROCREADER_RGDS_LAYOUT="${ROCREADER_RGDS_LAYOUT:-spanning}" ROCREADER_RGDS_DEMO_SECONDS="$SECONDS_TO_RUN" "$BIN" >> "$LOG_FILE" 2>&1
else
  ROCREADER_RGDS_LAYOUT="${ROCREADER_RGDS_LAYOUT:-spanning}" ROCREADER_RGDS_DEMO_SECONDS="$SECONDS_TO_RUN" "$BIN" >> "$LOG_FILE" 2>&1
fi
rc=$?

log "[rgds_demo_sh] executable exit code: $rc"
exit "$rc"
