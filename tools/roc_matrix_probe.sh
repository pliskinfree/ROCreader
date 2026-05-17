#!/bin/sh
# ROCreader hardware/system matrix probe for Linux ports.
# Run on the target device:
#   sh tools/roc_matrix_probe.sh
# Optional:
#   sh tools/roc_matrix_probe.sh --keys 30
#   sh tools/roc_matrix_probe.sh --out /tmp/roc_probe

set -u

VERSION="2026-05-17"
KEY_SECONDS=20
OUT_BASE=""
RUN_KEY_PROBE=1

usage() {
  cat <<'EOF'
ROCreader matrix probe

Usage:
  sh roc_matrix_probe.sh [--out DIR] [--keys SECONDS] [--no-keys]

The script collects read-only hardware/system information useful when porting
ROCreader to another Linux handheld/chipset. It writes a report directory and
a compressed tarball next to it when tar is available.

Options:
  --out DIR       Output directory base. Default: ./roc_matrix_report_YYYYmmdd_HHMMSS
  --keys N        Try interactive input/key capture for N seconds. Default: 20
  --no-keys       Skip interactive key capture.
  -h, --help      Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --out)
      [ "$#" -ge 2 ] || { echo "missing value for --out" >&2; exit 2; }
      OUT_BASE=$2
      shift 2
      ;;
    --keys)
      [ "$#" -ge 2 ] || { echo "missing value for --keys" >&2; exit 2; }
      KEY_SECONDS=$2
      RUN_KEY_PROBE=1
      shift 2
      ;;
    --no-keys)
      RUN_KEY_PROBE=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

timestamp() {
  date '+%Y%m%d_%H%M%S' 2>/dev/null || echo "unknown_time"
}

NOW=$(timestamp)
if [ -z "$OUT_BASE" ]; then
  OUT_BASE="./roc_matrix_report_$NOW"
fi

REPORT_DIR=$OUT_BASE
if [ -e "$REPORT_DIR" ]; then
  i=1
  while [ -e "${OUT_BASE}_$i" ]; do
    i=$((i + 1))
  done
  REPORT_DIR="${OUT_BASE}_$i"
fi
RAW_DIR="$REPORT_DIR/raw"
SUMMARY="$REPORT_DIR/summary.txt"
mkdir -p "$RAW_DIR" || exit 1
for d in sys proc commands input display audio network power storage devices; do
  mkdir -p "$RAW_DIR/$d" || exit 1
done

log() {
  printf '%s\n' "$*"
}

section() {
  {
    printf '\n'
    printf '============================================================\n'
    printf '%s\n' "$1"
    printf '============================================================\n'
  } >> "$SUMMARY"
}

append_file() {
  label=$1
  path=$2
  section "$label: $path"
  if [ -r "$path" ]; then
    sed -n '1,240p' "$path" >> "$SUMMARY" 2>&1
  else
    echo "not readable or missing" >> "$SUMMARY"
  fi
}

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

run_cmd() {
  name=$1
  shift
  out="$RAW_DIR/commands/$name.txt"
  {
    echo "\$ $*"
    echo
    "$@"
  } > "$out" 2>&1
  rc=$?
  echo "exit_code=$rc" >> "$out"
  return 0
}

run_shell() {
  name=$1
  cmd=$2
  out="$RAW_DIR/commands/$name.txt"
  {
    echo "\$ $cmd"
    echo
    sh -c "$cmd"
  } > "$out" 2>&1
  rc=$?
  echo "exit_code=$rc" >> "$out"
  return 0
}

copy_if_readable() {
  src=$1
  dst=$2
  if [ -r "$src" ]; then
    cp "$src" "$dst" 2>/dev/null || true
  fi
}

copy_tree_text_limited() {
  src=$1
  dst=$2
  max_files=${3:-400}
  mkdir -p "$dst"
  [ -d "$src" ] || return 0
  count=0
  find "$src" -maxdepth 3 -type f 2>/dev/null | while IFS= read -r f; do
    count=$((count + 1))
    [ "$count" -le "$max_files" ] || continue
    rel=${f#"$src"/}
    target="$dst/$rel.txt"
    mkdir -p "$(dirname "$target")"
    if [ -r "$f" ]; then
      {
        echo "# $f"
        sed -n '1,120p' "$f" 2>/dev/null
      } > "$target" 2>/dev/null || true
    fi
  done
}

ratio_of() {
  w=$1
  h=$2
  if have_cmd awk; then
    awk -v w="$w" -v h="$h" 'function gcd(a,b){while(b){t=a%b;a=b;b=t}return a} BEGIN{if(w>0&&h>0){g=gcd(w,h); printf "%d:%d %.4f", w/g, h/g, w/h}}'
  else
    echo "${w}:${h}"
  fi
}

human_header() {
  cat > "$SUMMARY" <<EOF
ROCreader Linux Hardware Matrix Probe
Version: $VERSION
Started: $(date 2>/dev/null || echo unknown)
Report directory: $REPORT_DIR

Purpose:
  Collect display, input, audio, Wi-Fi, power, storage, GPU, kernel and system
  interfaces needed to port ROCreader to a new Linux handheld or chip.

Privacy note:
  This report may include hostnames, Wi-Fi interface names/MAC addresses,
  mounted paths, kernel command line and process names. Review before sharing.
EOF
}

human_header

log "ROCreader matrix probe starting..."
log "Report: $REPORT_DIR"

section "Quick Porting Checklist"
cat >> "$SUMMARY" <<'EOF'
Key ROCreader integration points to verify on the target:
  1. Display: framebuffer/DRM resolution, rotation, pixel format, refresh rate, dual-screen layout.
  2. SDL video backend: available fbdev/KMSDRM/Wayland/X11 libraries and permissions.
  3. Input: /dev/input/event* devices, EV_KEY codes, joystick button indexes, power key code.
  4. Brightness: /sys/class/backlight/* or vendor ioctl such as /dev/disp.
  5. Audio: ALSA card/control names, amixer behavior, headphone/speaker output.
  6. Wi-Fi: interface name, rfkill, firmware/driver, wpa_supplicant or NetworkManager.
  7. Power: battery/charger sysfs, lid/suspend/power button paths.
  8. Storage: writable app/data paths, filesystem type, SD mount behavior.
  9. Runtime ABI: libc, libstdc++, SDL2, SDL2_image, SDL2_ttf, SDL2_mixer availability.
  10. Performance: CPU/GPU, memory, zram/swap, governor, thermal throttling.
EOF

section "Environment"
{
  echo "user=$(id 2>/dev/null || true)"
  echo "pwd=$(pwd 2>/dev/null || true)"
  echo "shell=${SHELL:-unknown}"
  echo "path=${PATH:-unknown}"
  echo "uname=$(uname -a 2>/dev/null || true)"
  echo "hostname=$(hostname 2>/dev/null || true)"
  echo "date=$(date 2>/dev/null || true)"
} >> "$SUMMARY"

for f in \
  /etc/os-release /etc/issue /proc/version /proc/cmdline /proc/cpuinfo /proc/meminfo \
  /proc/partitions /proc/mounts /proc/filesystems /proc/swaps /proc/modules \
  /proc/bus/input/devices /proc/asound/cards /proc/asound/devices /proc/asound/pcm \
  /proc/device-tree/model /sys/firmware/devicetree/base/model
do
  safe=$(echo "$f" | sed 's#^/##; s#[/:]#_#g')
  copy_if_readable "$f" "$RAW_DIR/proc/$safe.txt"
done

append_file "Kernel command line" /proc/cmdline
append_file "CPU" /proc/cpuinfo
append_file "Memory" /proc/meminfo
append_file "Input devices from proc" /proc/bus/input/devices
append_file "ALSA cards" /proc/asound/cards

for c in uname id uptime dmesg lsmod ldd ldconfig getconf free df mount findmnt lsblk blkid; do
  if have_cmd "$c"; then
    case "$c" in
      dmesg) run_cmd dmesg "$c" ;;
      df) run_cmd df "$c" -hT ;;
      findmnt) run_cmd findmnt "$c" -A ;;
      lsblk) run_cmd lsblk "$c" -o NAME,MAJ:MIN,SIZE,TYPE,FSTYPE,MOUNTPOINT,LABEL,MODEL ;;
      ldconfig) run_cmd ldconfig "$c" -p ;;
      getconf) run_cmd getconf_libc "$c" GNU_LIBC_VERSION ;;
      *) run_cmd "$c" "$c" ;;
    esac
  fi
done

section "Display Summary"
{
  echo "Framebuffer devices:"
  if ls /dev/fb* >/dev/null 2>&1; then
    for fb in /dev/fb*; do
      [ -e "$fb" ] || continue
      name=$(basename "$fb")
      echo "- $fb"
      for item in name virtual_size modes mode bits_per_pixel stride rotate blank; do
        p="/sys/class/graphics/$name/$item"
        [ -r "$p" ] && echo "  $item=$(cat "$p" 2>/dev/null)"
      done
      if [ -r "/sys/class/graphics/$name/virtual_size" ]; then
        vs=$(cat "/sys/class/graphics/$name/virtual_size" 2>/dev/null | tr ',' ' ')
        set -- $vs
        if [ "$#" -ge 2 ]; then
          echo "  ratio=$(ratio_of "$1" "$2")"
        fi
      fi
    done
  else
    echo "- no /dev/fb* found"
  fi

  echo
  echo "DRM cards/connectors:"
  if [ -d /sys/class/drm ]; then
    for d in /sys/class/drm/*; do
      [ -e "$d" ] || continue
      b=$(basename "$d")
      case "$b" in version) continue ;; esac
      echo "- $b"
      for item in status enabled modes mode dpms edid; do
        p="$d/$item"
        if [ -r "$p" ]; then
          if [ "$item" = "edid" ]; then
            echo "  edid_bytes=$(wc -c < "$p" 2>/dev/null)"
          else
            echo "  $item=$(tr '\n' ' ' < "$p" 2>/dev/null)"
          fi
        fi
      done
    done
  else
    echo "- no /sys/class/drm found"
  fi

  echo
  echo "Backlight:"
  if [ -d /sys/class/backlight ]; then
    for b in /sys/class/backlight/*; do
      [ -d "$b" ] || continue
      echo "- $(basename "$b")"
      for item in brightness actual_brightness max_brightness bl_power type scale; do
        [ -r "$b/$item" ] && echo "  $item=$(cat "$b/$item" 2>/dev/null)"
      done
    done
  else
    echo "- no /sys/class/backlight found"
  fi

  echo
  echo "Vendor display device candidates:"
  for d in /dev/disp /dev/disp0 /dev/graphics/fb0 /dev/mali /dev/dri/card0 /dev/dri/renderD128; do
    [ -e "$d" ] && ls -l "$d"
  done
} >> "$SUMMARY"

copy_tree_text_limited /sys/class/graphics "$RAW_DIR/display/sys_class_graphics" 300
copy_tree_text_limited /sys/class/drm "$RAW_DIR/display/sys_class_drm" 500
copy_tree_text_limited /sys/class/backlight "$RAW_DIR/display/sys_class_backlight" 200
copy_tree_text_limited /sys/class/leds "$RAW_DIR/display/sys_class_leds" 300

for c in fbset modetest kmsprint xrandr weston-info xdpyinfo glxinfo eglinfo; do
  have_cmd "$c" && run_cmd "$c" "$c"
done

section "Audio Summary"
{
  echo "ALSA proc:"
  [ -r /proc/asound/cards ] && sed -n '1,120p' /proc/asound/cards
  echo
  echo "Mixer controls:"
  if have_cmd amixer; then
    amixer scontrols 2>&1
    echo
    amixer 2>&1 | sed -n '1,260p'
  else
    echo "amixer not found"
  fi
  echo
  echo "Trimui shmvar candidate:"
  for p in /usr/trimui/bin/shmvar /mnt/SDCARD/System/bin/shmvar /usr/bin/shmvar; do
    if [ -x "$p" ]; then
      echo "- $p"
      "$p" vol 2>&1 || true
    fi
  done
} >> "$SUMMARY"

copy_tree_text_limited /proc/asound "$RAW_DIR/audio/proc_asound" 500
for c in aplay arecord amixer pactl wpctl; do
  if have_cmd "$c"; then
    case "$c" in
      aplay) run_cmd aplay_l "$c" -l ;;
      arecord) run_cmd arecord_l "$c" -l ;;
      amixer) run_cmd amixer_full "$c" ;;
      pactl) run_cmd pactl_info "$c" info ;;
      wpctl) run_cmd wpctl_status "$c" status ;;
    esac
  fi
done

section "Input Summary"
{
  echo "Input devices:"
  if [ -r /proc/bus/input/devices ]; then
    sed -n '1,260p' /proc/bus/input/devices
  else
    echo "/proc/bus/input/devices not readable"
  fi
  echo
  echo "/dev/input:"
  ls -la /dev/input 2>&1 || true
  echo
  echo "ROCreader expected logical buttons:"
  echo "UP DOWN LEFT RIGHT A B X Y MENU L1 L2 R1 R2 START SELECT VOLUP VOLDOWN POWER"
  echo
  echo "Known ROCreader default joystick hints:"
  echo "H700/Trimui usually: joy.14=POWER joy.15=VOLDOWN joy.16=VOLUP, with profile-specific face/menu buttons."
} >> "$SUMMARY"

copy_tree_text_limited /sys/class/input "$RAW_DIR/input/sys_class_input" 800
copy_if_readable /proc/bus/input/devices "$RAW_DIR/input/proc_bus_input_devices.txt"

if have_cmd getevent; then
  run_cmd getevent_list getevent -il
fi
if have_cmd evtest; then
  run_shell evtest_list "printf '\n' | evtest 2>&1 | sed -n '1,220p'"
fi

if [ "$RUN_KEY_PROBE" = "1" ]; then
  section "Interactive Input Capture"
  {
    echo "The script attempted a ${KEY_SECONDS}s key capture."
    echo "Suggested sequence: D-pad, A/B/X/Y, L/R, Start/Select/Menu, Volume +/-, Power."
    echo "If evtest/getevent is missing or permissions are insufficient, use the static input summary above."
  } >> "$SUMMARY"

  log ""
  log "Input capture: press D-pad, A/B/X/Y, shoulders, Start/Select/Menu, Vol+/Vol-, Power for ${KEY_SECONDS}s."
  log "If the device has no timeout command, this step will be skipped."
  if have_cmd timeout && have_cmd getevent; then
    run_shell "interactive_getevent_${KEY_SECONDS}s" "timeout '$KEY_SECONDS' getevent -lt 2>&1"
  elif have_cmd timeout && have_cmd evtest; then
    for ev in /dev/input/event*; do
      [ -e "$ev" ] || continue
      safe=$(basename "$ev")
      run_shell "interactive_evtest_${safe}_${KEY_SECONDS}s" "timeout '$KEY_SECONDS' evtest '$ev' 2>&1"
    done
  else
    echo "timeout+getevent/evtest not available; skipped interactive input capture" >> "$SUMMARY"
  fi
fi

section "Network and Wi-Fi Summary"
{
  echo "Interfaces:"
  ip addr 2>&1 || ifconfig -a 2>&1 || true
  echo
  echo "Routes:"
  ip route 2>&1 || route -n 2>&1 || true
  echo
  echo "Wireless:"
  iw dev 2>&1 || true
  iwconfig 2>&1 || true
  echo
  echo "rfkill:"
  rfkill list 2>&1 || true
} >> "$SUMMARY"

copy_tree_text_limited /sys/class/net "$RAW_DIR/network/sys_class_net" 700
copy_tree_text_limited /sys/class/rfkill "$RAW_DIR/network/sys_class_rfkill" 200
for c in ip ifconfig iw iwconfig rfkill nmcli wpa_cli resolvectl; do
  if have_cmd "$c"; then
    case "$c" in
      ip) run_cmd ip_addr "$c" addr; run_cmd ip_route "$c" route ;;
      iw) run_cmd iw_dev "$c" dev; run_cmd iw_phy "$c" phy ;;
      rfkill) run_cmd rfkill_list "$c" list ;;
      nmcli) run_cmd nmcli_device "$c" device status ;;
      wpa_cli) run_cmd wpa_cli_status "$c" status ;;
      *) run_cmd "$c" "$c" ;;
    esac
  fi
done

section "Power, Battery, Thermal, Buttons"
{
  echo "Power supplies:"
  if [ -d /sys/class/power_supply ]; then
    for p in /sys/class/power_supply/*; do
      [ -d "$p" ] || continue
      echo "- $(basename "$p")"
      for item in type status present online capacity voltage_now current_now charge_now charge_full energy_now energy_full temp model_name manufacturer; do
        [ -r "$p/$item" ] && echo "  $item=$(cat "$p/$item" 2>/dev/null)"
      done
    done
  else
    echo "no /sys/class/power_supply"
  fi
  echo
  echo "Wakeup-capable devices:"
  find /sys/devices -path '*/power/wakeup' -type f -print 2>/dev/null | while IFS= read -r w; do
    echo "$w=$(cat "$w" 2>/dev/null)"
  done | sed -n '1,240p'
  echo
  echo "Thermal zones:"
  if [ -d /sys/class/thermal ]; then
    for t in /sys/class/thermal/thermal_zone*; do
      [ -d "$t" ] || continue
      echo "- $(basename "$t")"
      [ -r "$t/type" ] && echo "  type=$(cat "$t/type" 2>/dev/null)"
      [ -r "$t/temp" ] && echo "  temp=$(cat "$t/temp" 2>/dev/null)"
    done
  fi
} >> "$SUMMARY"

copy_tree_text_limited /sys/class/power_supply "$RAW_DIR/power/sys_class_power_supply" 500
copy_tree_text_limited /sys/class/thermal "$RAW_DIR/power/sys_class_thermal" 500
copy_tree_text_limited /sys/class/hwmon "$RAW_DIR/power/sys_class_hwmon" 500

section "Storage Summary"
{
  echo "Mounts:"
  sed -n '1,260p' /proc/mounts 2>/dev/null || true
  echo
  echo "Disk usage:"
  df -hT 2>&1 || true
  echo
  echo "Block devices:"
  lsblk -o NAME,MAJ:MIN,SIZE,TYPE,FSTYPE,MOUNTPOINT,LABEL,MODEL 2>&1 || true
} >> "$SUMMARY"

copy_tree_text_limited /sys/class/block "$RAW_DIR/storage/sys_class_block" 800

section "Runtime ABI and Library Candidates"
{
  echo "libc:"
  getconf GNU_LIBC_VERSION 2>&1 || true
  ldd --version 2>&1 | sed -n '1,20p' || true
  echo
  echo "SDL/library candidates:"
  for lib in SDL2 SDL2_image SDL2_ttf SDL2_mixer asound jpeg png webp freetype z; do
    echo "-- $lib --"
    pkg-config --modversion "$lib" 2>&1 || true
    pkg-config --libs --cflags "$lib" 2>&1 || true
  done
  echo
  echo "Common env vars:"
  env 2>/dev/null | sort | sed -n '1,240p'
} >> "$SUMMARY"

for c in pkg-config sdl2-config; do
  have_cmd "$c" && run_cmd "$c" "$c" --version
done
run_shell library_find_sdl "find /lib /usr/lib /usr/local/lib /opt -maxdepth 5 \\( -name 'libSDL2*' -o -name 'libasound*' -o -name 'libMali*' -o -name 'libGLES*' \\) 2>/dev/null | sed -n '1,300p'"

section "Device Tree and Platform Buses"
{
  echo "Device tree compatible/model:"
  for p in /proc/device-tree/model /proc/device-tree/compatible /sys/firmware/devicetree/base/model /sys/firmware/devicetree/base/compatible; do
    if [ -r "$p" ]; then
      echo "- $p:"
      tr '\000' '\n' < "$p" 2>/dev/null
      echo
    fi
  done
  echo
  echo "Platform devices:"
  ls -la /sys/bus/platform/devices 2>&1 | sed -n '1,220p'
  echo
  echo "I2C/SPI/UART/GPIO/PWM classes:"
  for d in /sys/class/gpio /sys/class/pwm /sys/class/i2c-dev /sys/class/spidev /sys/class/tty; do
    echo "-- $d --"
    ls -la "$d" 2>&1 | sed -n '1,120p'
  done
} >> "$SUMMARY"

copy_tree_text_limited /sys/class/gpio "$RAW_DIR/devices/sys_class_gpio" 400
copy_tree_text_limited /sys/class/pwm "$RAW_DIR/devices/sys_class_pwm" 300
copy_tree_text_limited /sys/class/i2c-dev "$RAW_DIR/devices/sys_class_i2c_dev" 300
copy_tree_text_limited /sys/class/spidev "$RAW_DIR/devices/sys_class_spidev" 300
copy_tree_text_limited /sys/class/tty "$RAW_DIR/devices/sys_class_tty" 400

section "Suggested native_keymap.ini Work Area"
cat >> "$SUMMARY" <<'EOF'
Fill this after reviewing raw/commands/interactive_getevent_* or evtest output:

# joy.<button_index>=<BUTTON_NAME>
# pad.<button_index>=<BUTTON_NAME>
# BUTTON_NAME: UP DOWN LEFT RIGHT A B X Y MENU L1 L2 R1 R2 START SELECT VOLUP VOLDOWN POWER NONE
#
# Example:
# joy.0=A
# joy.1=B
# joy.2=Y
# joy.3=X
# joy.6=SELECT
# joy.7=START
# joy.8=MENU
# joy.14=POWER
# joy.15=VOLDOWN
# joy.16=VOLUP
EOF

section "Files Written"
{
  echo "Summary: $SUMMARY"
  echo "Raw command output: $RAW_DIR/commands"
  echo "Raw sysfs/proc snapshots: $RAW_DIR"
} >> "$SUMMARY"

if have_cmd tar; then
  archive="${REPORT_DIR}.tar.gz"
  tar -czf "$archive" "$REPORT_DIR" >/dev/null 2>&1 && {
    echo "Archive: $archive" >> "$SUMMARY"
    log "Archive: $archive"
  }
fi

log "Done."
log "Summary: $SUMMARY"
log "Raw data: $RAW_DIR"
