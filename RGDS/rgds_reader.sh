#!/bin/sh
# RGDS dedicated ROCreader prototype. Stops Weston, owns DRM directly, then
# attempts to restore Weston when the app exits.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_reader_files/rgds_reader_app"
LOG_FILE="$SCRIPT_DIR/rgds_reader_latest.log"
WESTON_INIT="/etc/init.d/S49weston"
SECONDS_TO_RUN="${ROCREADER_RGDS_READER_SECONDS:-600}"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

stop_weston() {
  log "[rgds_reader_sh] stopping Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" stop >> "$LOG_FILE" 2>&1 || log "[rgds_reader_sh] weston stop failed rc=$?"
  fi
  sleep 1
  if pidof weston >/dev/null 2>&1; then
    killall weston >> "$LOG_FILE" 2>&1 || true
    sleep 1
  fi
  if pidof weston >/dev/null 2>&1; then
    killall -9 weston >> "$LOG_FILE" 2>&1 || true
    sleep 1
  fi
}

start_weston() {
  log "[rgds_reader_sh] restoring Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" start >> "$LOG_FILE" 2>&1 || log "[rgds_reader_sh] weston start failed rc=$?"
  fi
}

log "[rgds_reader_sh] started: $(date 2>/dev/null || echo unknown)"
log "[rgds_reader_sh] script=$0"
log "[rgds_reader_sh] binary=$BIN"
log "[rgds_reader_sh] seconds=$SECONDS_TO_RUN"
log "[rgds_reader_sh] uname=$(uname -a 2>/dev/null || true)"

if [ ! -f "$BIN" ]; then
  log "[rgds_reader_sh] missing binary: $BIN"
  exit 127
fi

chmod +x "$BIN" 2>/dev/null || true
stop_weston

ROCREADER_RGDS_READER_SECONDS="$SECONDS_TO_RUN" "$BIN" >> "$LOG_FILE" 2>&1
rc=$?
log "[rgds_reader_sh] executable exit code: $rc"

start_weston
log "[rgds_reader_sh] done"
exit "$rc"
