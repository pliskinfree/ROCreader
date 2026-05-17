#!/bin/sh
# RGDS display owner probe: find processes that may keep repainting DRM screens.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
STAMP=$(date +%Y%m%d_%H%M%S 2>/dev/null || echo unknown)
OUT_DIR="$SCRIPT_DIR/rgds_display_owner_report_$STAMP"
LOG_FILE="$SCRIPT_DIR/rgds_display_owner_probe_latest.log"

mkdir -p "$OUT_DIR" 2>/dev/null || true
: > "$LOG_FILE" 2>/dev/null || true

log() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$LOG_FILE" 2>/dev/null || true
}

save_cmd() {
  name="$1"
  shift
  {
    echo "### $*"
    "$@" 2>&1
    echo
  } > "$OUT_DIR/$name.txt" 2>&1 || true
}

log "[owner_probe] started: $(date 2>/dev/null || echo unknown)"
log "[owner_probe] out_dir=$OUT_DIR"
log "[owner_probe] log=$LOG_FILE"
log "[owner_probe] uname=$(uname -a 2>/dev/null || true)"

save_cmd ps ps w
save_cmd ps_ef ps -ef
save_cmd mounts mount
save_cmd proc_cmdline cat /proc/cmdline
save_cmd drm_tree sh -c 'find /sys/class/drm -maxdepth 3 -type f -print -exec sh -c "echo ---; cat \"$1\" 2>/dev/null" sh {} \;'
save_cmd dev_dri sh -c 'ls -l /dev/dri /dev/fb* /dev/tty* 2>/dev/null'
save_cmd services sh -c 'for d in /etc/init.d /etc/rc.d /etc/systemd/system /usr/lib/systemd/system; do [ -d "$d" ] && find "$d" -maxdepth 2 -type f -print; done'

FD_OUT="$OUT_DIR/process_display_fds.txt"
: > "$FD_OUT" 2>/dev/null || true
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
    {
      echo "pid=$pid cmd=$cmd"
      printf '%s\n' "$lines"
      echo
    } >> "$FD_OUT"
  fi
done

SUMMARY="$OUT_DIR/summary.txt"
{
  echo "RGDS display owner probe"
  echo "out_dir=$OUT_DIR"
  echo
  echo "Processes with display/input fds:"
  cat "$FD_OUT" 2>/dev/null || true
  echo
  echo "Likely frontend/loading processes:"
  ps w 2>/dev/null | grep -Ei 'loading|launcher|frontend|emulation|retro|drm|sdl|mali|fb|menu|anbernic|app' | grep -v grep || true
} > "$SUMMARY" 2>&1 || true

cp "$SUMMARY" "$SCRIPT_DIR/rgds_display_owner_latest_summary.txt" 2>/dev/null || true
log "[owner_probe] summary=$SUMMARY"
log "[owner_probe] done"
