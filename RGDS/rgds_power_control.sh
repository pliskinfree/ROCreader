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
APP_DIR="${ROCREADER_ROOT:-$SELF_DIR}"
LOG_FILE="${ROCREADER_RGDS_POWER_LOG:-$APP_DIR/rgds_power_control.log}"
STATE_FILE="${ROCREADER_RGDS_POWER_STATE_FILE:-$APP_DIR/rgds_power_state.txt}"
MODE="${1:-auto}"

mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true

log() {
  printf '%s %s\n' "$(date '+%F %T' 2>/dev/null || echo unknown)" "$*" >> "$LOG_FILE" 2>/dev/null || true
}

run_quiet() {
  desc="$1"
  shift
  log "try $desc: $*"
  "$@" >> "$LOG_FILE" 2>&1
  rc=$?
  log "result $desc rc=$rc"
  return "$rc"
}

run_shell() {
  desc="$1"
  command="$2"
  log "try $desc: $command"
  sh -c "$command" >> "$LOG_FILE" 2>&1
  rc=$?
  log "result $desc rc=$rc"
  return "$rc"
}

write_state() {
  printf '%s\n' "$1" > "$STATE_FILE" 2>/dev/null || true
  log "state=$1 file=$STATE_FILE"
}

blank_fb() {
  touched=0
  failed=0
  for fb in /sys/class/graphics/fb*/blank; do
    [ -e "$fb" ] || continue
    touched=$((touched + 1))
    run_shell "fb blank $fb" "echo 1 > '$fb'" || failed=$((failed + 1))
  done
  [ "$touched" -gt 0 ] && [ "$failed" -eq 0 ]
}

unblank_fb() {
  touched=0
  failed=0
  for fb in /sys/class/graphics/fb*/blank; do
    [ -e "$fb" ] || continue
    touched=$((touched + 1))
    run_shell "fb unblank $fb" "echo 0 > '$fb'" || failed=$((failed + 1))
  done
  [ "$touched" -gt 0 ] && [ "$failed" -eq 0 ]
}

set_backlight_power() {
  value="$1"
  touched=0
  failed=0
  for power in /sys/class/backlight/*/bl_power; do
    [ -e "$power" ] || continue
    touched=$((touched + 1))
    run_shell "backlight power $power=$value" "echo '$value' > '$power'" || failed=$((failed + 1))
  done
  [ "$touched" -gt 0 ] && [ "$failed" -eq 0 ]
}

set_drm_dpms() {
  value="$1"
  touched=0
  failed=0
  for dpms in /sys/class/drm/*/dpms; do
    [ -e "$dpms" ] || continue
    touched=$((touched + 1))
    run_shell "drm dpms $dpms=$value" "echo '$value' > '$dpms'" || failed=$((failed + 1))
  done
  [ "$touched" -gt 0 ] && [ "$failed" -eq 0 ]
}

legacy_power_script() {
  arg="$1"
  script="${ROCREADER_RGDS_LEGACY_PWR_SCRIPT:-}"
  [ -n "$script" ] || return 1
  [ -x "$script" ] || [ -f "$script" ] || return 1
  run_quiet "legacy power script $arg" "$script" "$arg"
}

screen_off() {
  if [ -n "${ROCREADER_RGDS_SCREEN_OFF_COMMAND:-}" ]; then
    run_shell "env screen off command" "$ROCREADER_RGDS_SCREEN_OFF_COMMAND" && return 0
  fi
  ok=1
  set_backlight_power 4 && ok=0
  blank_fb && ok=0
  set_drm_dpms Off && ok=0
  if [ "$ok" = 0 ]; then
    return 0
  fi
  legacy_power_script "$MODE" && return 0
  legacy_power_script "powerkey" && return 0
  legacy_power_script "manual" && return 0
  legacy_power_script "off" && return 0
  return 1
}

screen_on() {
  if [ -n "${ROCREADER_RGDS_SCREEN_ON_COMMAND:-}" ]; then
    run_shell "env screen on command" "$ROCREADER_RGDS_SCREEN_ON_COMMAND" && return 0
  fi
  ok=1
  set_backlight_power 0 && ok=0
  unblank_fb && ok=0
  set_drm_dpms On && ok=0
  if [ "$ok" = 0 ]; then
    return 0
  fi
  legacy_power_script "on" && return 0
  legacy_power_script "wake" && return 0
  legacy_power_script "resume" && return 0
  return 1
}

log "mode=$MODE app_dir=$APP_DIR script=$0"

case "$MODE" in
  auto|powerkey|manual|off)
    if screen_off; then
      write_state off
      exit 0
    fi
    write_state on
    exit 1
    ;;
  on|wake|resume)
    if screen_on; then
      write_state on
      exit 0
    fi
    write_state off
    exit 1
    ;;
  *)
    log "unsupported mode=$MODE"
    exit 2
    ;;
esac
