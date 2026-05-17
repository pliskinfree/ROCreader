#!/bin/sh
# RGDS direct DRM/KMS dual-screen probe.
# This bypasses SDL rendering and writes dumb buffers directly through DRM/KMS.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_drm_probe_files/rgds_drm_dualscreen_probe"
LOG_FILE="$SCRIPT_DIR/rgds_drm_dualscreen_probe_latest.log"
SECONDS_TO_RUN="${ROCREADER_RGDS_DRM_SECONDS:-12}"
REFRESH_MS="${ROCREADER_RGDS_DRM_REFRESH_MS:-250}"
NO_RESTORE="${ROCREADER_RGDS_DRM_NO_RESTORE:-0}"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

log "[rgds_drm_probe_sh] started: $(date 2>/dev/null || echo unknown)"
log "[rgds_drm_probe_sh] script=$0"
log "[rgds_drm_probe_sh] script_dir=$SCRIPT_DIR"
log "[rgds_drm_probe_sh] binary=$BIN"
log "[rgds_drm_probe_sh] seconds=$SECONDS_TO_RUN"
log "[rgds_drm_probe_sh] refresh_ms=$REFRESH_MS"
log "[rgds_drm_probe_sh] no_restore=$NO_RESTORE"
log "[rgds_drm_probe_sh] PATH=${PATH:-}"
log "[rgds_drm_probe_sh] uname=$(uname -a 2>/dev/null || true)"
log "[rgds_drm_probe_sh] log=$LOG_FILE"

if [ ! -f "$BIN" ]; then
  log "[rgds_drm_probe_sh] missing binary: $BIN"
  exit 127
fi

chmod +x "$BIN" 2>/dev/null || true
ROCREADER_RGDS_DRM_SECONDS="$SECONDS_TO_RUN" ROCREADER_RGDS_DRM_REFRESH_MS="$REFRESH_MS" ROCREADER_RGDS_DRM_NO_RESTORE="$NO_RESTORE" "$BIN" >> "$LOG_FILE" 2>&1
rc=$?

log "[rgds_drm_probe_sh] executable exit code: $rc"
exit "$rc"
