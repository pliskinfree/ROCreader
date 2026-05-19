#!/bin/sh
# RGDS Weston single-window spanning probe.
# It keeps Weston alive and asks SDL/Wayland for one borderless 1280x480 window.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
BIN="$SCRIPT_DIR/.rgds_weston_spanning_probe_files/rgds_weston_spanning_probe"
LOG_FILE="$SCRIPT_DIR/rgds_weston_spanning_probe_latest.log"
SECONDS_TO_RUN="${ROCREADER_RGDS_SPANNING_SECONDS:-120}"
APP_LIB_DIR="$SCRIPT_DIR/ROCreader_RGDS/lib"
APP_SYSTEM_SDL_DIR="$SCRIPT_DIR/ROCreader_RGDS/lib_system_sdl"

: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

choose_xdg_runtime_dir() {
  if [ -n "${XDG_RUNTIME_DIR:-}" ]; then
    return
  fi
  wayland_name="${WAYLAND_DISPLAY:-wayland-0}"
  for candidate in /run/user/0 /var/run /tmp; do
    if [ -S "$candidate/$wayland_name" ]; then
      export XDG_RUNTIME_DIR="$candidate"
      return
    fi
  done
  export XDG_RUNTIME_DIR="/var/run"
}

log "[rgds_spanning_probe_sh] started: $(date 2>/dev/null || echo unknown)"
log "[rgds_spanning_probe_sh] script=$0"
log "[rgds_spanning_probe_sh] script_dir=$SCRIPT_DIR"
log "[rgds_spanning_probe_sh] binary=$BIN"
log "[rgds_spanning_probe_sh] seconds=$SECONDS_TO_RUN"
log "[rgds_spanning_probe_sh] weston_pids=$(pidof weston 2>/dev/null || true)"
log "[rgds_spanning_probe_sh] incoming SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}"
log "[rgds_spanning_probe_sh] incoming WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
log "[rgds_spanning_probe_sh] incoming XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-}"

if [ ! -f "$BIN" ]; then
  legacy_bin="$SCRIPT_DIR/rgds_weston_spanning_probe"
  if [ -f "$legacy_bin" ]; then
    BIN="$legacy_bin"
    log "[rgds_spanning_probe_sh] using legacy root binary: $BIN"
  else
    log "[rgds_spanning_probe_sh] missing binary: $BIN"
    exit 127
  fi
fi

chmod +x "$BIN" 2>/dev/null || true

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
choose_xdg_runtime_dir
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}"

if [ -d "$APP_SYSTEM_SDL_DIR" ]; then
  export LD_LIBRARY_PATH="$APP_SYSTEM_SDL_DIR:$APP_SYSTEM_SDL_DIR/pulseaudio:/usr/lib:/lib:/mnt/vendor/lib:${LD_LIBRARY_PATH:-}"
elif [ -d "$APP_LIB_DIR" ]; then
  export LD_LIBRARY_PATH="$APP_LIB_DIR:$APP_LIB_DIR/pulseaudio:/usr/lib:/lib:/mnt/vendor/lib:${LD_LIBRARY_PATH:-}"
fi

mkdir -p "$XDG_RUNTIME_DIR" 2>/dev/null || true
chmod 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true

log "[rgds_spanning_probe_sh] using SDL_VIDEODRIVER=$SDL_VIDEODRIVER"
log "[rgds_spanning_probe_sh] using WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
log "[rgds_spanning_probe_sh] using XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
log "[rgds_spanning_probe_sh] LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}"
ls -la "$XDG_RUNTIME_DIR" >> "$LOG_FILE" 2>&1 || true

ROCREADER_RGDS_SPANNING_SECONDS="$SECONDS_TO_RUN" "$BIN" >> "$LOG_FILE" 2>&1
rc=$?

log "[rgds_spanning_probe_sh] executable exit code: $rc"
log "[rgds_spanning_probe_sh] done"
exit "$rc"
