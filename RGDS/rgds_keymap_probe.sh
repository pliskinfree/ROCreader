#!/bin/sh
# RGDS physical key mapping collector. Stops Weston, shows one prompt at a time,
# logs raw evdev key events, then restores Weston.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_keymap_probe_files/rgds_keymap_probe"
LOG_FILE="$SCRIPT_DIR/rgds_keymap_probe_latest.log"
WESTON_INIT="/etc/init.d/S49weston"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

stop_weston() {
  log "[keymap_sh] stopping Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" stop >> "$LOG_FILE" 2>&1 || true
  fi
  sleep 1
  if pidof weston >/dev/null 2>&1; then killall weston >> "$LOG_FILE" 2>&1 || true; fi
  sleep 1
  if pidof weston >/dev/null 2>&1; then killall -9 weston >> "$LOG_FILE" 2>&1 || true; fi
}

start_weston() {
  log "[keymap_sh] restoring Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" start >> "$LOG_FILE" 2>&1 || true
  fi
}

log "[keymap_sh] started: $(date 2>/dev/null || echo unknown)"
log "[keymap_sh] binary=$BIN"
if [ ! -f "$BIN" ]; then
  log "[keymap_sh] missing binary: $BIN"
  exit 127
fi
chmod +x "$BIN" 2>/dev/null || true
stop_weston
"$BIN" >> "$LOG_FILE" 2>&1
rc=$?
log "[keymap_sh] executable exit code: $rc"
start_weston
log "[keymap_sh] done"
exit "$rc"
