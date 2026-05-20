#!/bin/sh
# RGDS dual-stick mapping collector. Stops Weston, shows one prompt at a time,
# logs raw evdev ABS/KEY events, then restores Weston.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_joystick_map_probe_files/rgds_joystick_map_probe"
LOG_FILE="$SCRIPT_DIR/rgds_joystick_map_probe_latest.log"
WESTON_INIT="/etc/init.d/S49weston"
WESTON_WAS_RUNNING=0

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

stop_weston() {
  if pidof weston >/dev/null 2>&1; then
    WESTON_WAS_RUNNING=1
  fi
  log "[joystick_map_sh] stopping Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" stop >> "$LOG_FILE" 2>&1 || true
  fi
  sleep 1
  if pidof weston >/dev/null 2>&1; then killall weston >> "$LOG_FILE" 2>&1 || true; fi
  sleep 1
  if pidof weston >/dev/null 2>&1; then killall -9 weston >> "$LOG_FILE" 2>&1 || true; fi
}

start_weston() {
  if [ "$WESTON_WAS_RUNNING" -ne 1 ] && [ ! -x "$WESTON_INIT" ]; then
    return
  fi
  log "[joystick_map_sh] restoring Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" start >> "$LOG_FILE" 2>&1 || true
  fi
}

cleanup() {
  start_weston
}

trap cleanup EXIT INT TERM

log "[joystick_map_sh] started: $(date 2>/dev/null || echo unknown)"
log "[joystick_map_sh] binary=$BIN"
log "[joystick_map_sh] log=$LOG_FILE"
if [ ! -f "$BIN" ]; then
  log "[joystick_map_sh] missing binary: $BIN"
  exit 127
fi

chmod +x "$BIN" 2>/dev/null || true
stop_weston
"$BIN" >> "$LOG_FILE" 2>&1
rc=$?
log "[joystick_map_sh] executable exit code: $rc"
cleanup
trap - EXIT INT TERM
log "[joystick_map_sh] done"
exit "$rc"
