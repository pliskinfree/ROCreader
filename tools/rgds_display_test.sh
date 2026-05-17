#!/bin/sh
# Click-friendly RGDS display test launcher.
# Put this file in /mnt/mmc/Roms/APPS and run/click it on the device.
# It always runs the matrix probe with the RGDS dual-screen visual test enabled.

set -u

script_dir() {
  case "$0" in
    */*) dir=${0%/*} ;;
    *) dir=. ;;
  esac
  cd "$dir" 2>/dev/null && pwd -P || pwd
}

SCRIPT_DIR=$(script_dir)
LOG_FILE="$SCRIPT_DIR/rgds_display_test_latest.log"
MATRIX="$SCRIPT_DIR/roc_matrix_probe.sh"

{
  echo "RGDS display test launcher"
  echo "Started: $(date 2>/dev/null || echo unknown)"
  echo "Script dir: $SCRIPT_DIR"
  echo

  if [ ! -r "$MATRIX" ]; then
    echo "ERROR: missing matrix probe: $MATRIX"
    exit 2
  fi

  echo "Running:"
  echo "  sh $MATRIX --no-keys --display-test 15"
  echo
  sh "$MATRIX" --no-keys --display-test 15
  rc=$?
  echo
  echo "Matrix exit code: $rc"
  echo "Latest path marker:"
  if [ -r "$SCRIPT_DIR/roc_matrix_probe_latest_path.txt" ]; then
    sed -n '1,40p' "$SCRIPT_DIR/roc_matrix_probe_latest_path.txt"
  else
    echo "not found"
  fi
  echo "Finished: $(date 2>/dev/null || echo unknown)"
  exit "$rc"
} > "$LOG_FILE" 2>&1
