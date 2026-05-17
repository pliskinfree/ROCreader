#!/bin/sh
# RGDS Weston isolation test.
# Stops the system Weston compositor, runs the direct DRM dual-screen probe, then
# tries to restore Weston. Use this only for diagnostics.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
LOG_FILE="$SCRIPT_DIR/rgds_stop_weston_drm_probe_latest.log"
DRM_PROBE_SH="$SCRIPT_DIR/rgds_drm_dualscreen_probe.sh"
WESTON_INIT="/etc/init.d/S49weston"
SECONDS_TO_RUN="${ROCREADER_RGDS_DRM_SECONDS:-15}"
REFRESH_MS="${ROCREADER_RGDS_DRM_REFRESH_MS:-250}"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

append_cmd() {
  log "### $*"
  "$@" >> "$LOG_FILE" 2>&1 || log "[weston_test] command failed rc=$?: $*"
}

show_display_fds() {
  {
    echo "### display fds"
    for p in /proc/[0-9]*; do
      pid=${p##*/}
      cmd=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null || true)
      [ -z "$cmd" ] && cmd=$(cat "$p/comm" 2>/dev/null || true)
      hit=0
      lines=""
      for fd in "$p"/fd/*; do
        target=$(readlink "$fd" 2>/dev/null || true)
        case "$target" in
          /dev/dri/*|/dev/fb*|/dev/tty*|/dev/input/*)
            hit=1
            lines="$lines
  ${fd##*/} -> $target"
            ;;
        esac
      done
      if [ "$hit" = 1 ]; then
        echo "pid=$pid cmd=$cmd"
        printf '%s\n' "$lines"
        echo
      fi
    done
  } >> "$LOG_FILE" 2>&1 || true
}

stop_weston() {
  log "[weston_test] stopping Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" stop >> "$LOG_FILE" 2>&1 || log "[weston_test] init stop failed rc=$?"
  else
    log "[weston_test] init script missing or not executable: $WESTON_INIT"
  fi
  sleep 1

  if pidof weston >/dev/null 2>&1; then
    log "[weston_test] weston still alive after init stop, sending TERM"
    killall weston >> "$LOG_FILE" 2>&1 || true
    sleep 1
  fi
  if pidof weston >/dev/null 2>&1; then
    log "[weston_test] weston still alive after TERM, sending KILL"
    killall -9 weston >> "$LOG_FILE" 2>&1 || true
    sleep 1
  fi
}

start_weston() {
  log "[weston_test] restoring Weston"
  if [ -x "$WESTON_INIT" ]; then
    "$WESTON_INIT" start >> "$LOG_FILE" 2>&1 || log "[weston_test] init start failed rc=$?"
  else
    log "[weston_test] cannot restore through missing init script: $WESTON_INIT"
  fi
}

log "[weston_test] started: $(date 2>/dev/null || echo unknown)"
log "[weston_test] script=$0"
log "[weston_test] script_dir=$SCRIPT_DIR"
log "[weston_test] drm_probe=$DRM_PROBE_SH"
log "[weston_test] seconds=$SECONDS_TO_RUN refresh_ms=$REFRESH_MS"
log "[weston_test] uname=$(uname -a 2>/dev/null || true)"

append_cmd ps w
show_display_fds

stop_weston
log "[weston_test] after stop"
append_cmd ps w
show_display_fds

if [ ! -f "$DRM_PROBE_SH" ]; then
  log "[weston_test] missing DRM probe script: $DRM_PROBE_SH"
  start_weston
  exit 127
fi

log "[weston_test] running DRM probe without Weston"
ROCREADER_RGDS_DRM_SECONDS="$SECONDS_TO_RUN" \
ROCREADER_RGDS_DRM_REFRESH_MS="$REFRESH_MS" \
ROCREADER_RGDS_DRM_NO_RESTORE=1 \
sh "$DRM_PROBE_SH" >> "$LOG_FILE" 2>&1
probe_rc=$?
log "[weston_test] drm probe rc=$probe_rc"

log "[weston_test] after DRM probe before restore"
append_cmd ps w
show_display_fds

start_weston
sleep 1
log "[weston_test] after restore"
append_cmd ps w
show_display_fds

log "[weston_test] done"
exit "$probe_rc"
