#!/usr/bin/env bash
# Flash a Zephyr/ZMK image into the Scanner Pocket v2 (nRF52832) over SWD
# through the Scanner Pocket SWD Programmer (CMSIS-DAP) with OpenOCD.
#
#   scripts/flash.sh [build/zephyr/zmk.hex] [--speed kHz] [--no-verify]
#
# Keeps the OpenOCD log in build/flash.log on failure.
set -euo pipefail

FW="${1:-build/zephyr/zmk.hex}"
SPEED=1000
VERIFY=verify
shift || true
while [ $# -gt 0 ]; do
  case "$1" in
    --speed) SPEED="$2"; shift 2 ;;
    --no-verify) VERIFY=""; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

OPENOCD="${OPENOCD:-$HOME/.local/opt/openocd/bin/openocd}"

if [ ! -f "$FW" ]; then
  echo "firmware not found: $FW" >&2; exit 1
fi
case "$FW" in
  *.hex|*.elf) ;;
  *) echo "use a .hex or .elf (a .bin needs an explicit load address)" >&2; exit 1 ;;
esac
if ! command -v "$OPENOCD" >/dev/null 2>&1; then
  echo "openocd not found at $OPENOCD (set OPENOCD=/path/to/openocd)" >&2; exit 1
fi

LOG="$(dirname "$FW")/flash.log"
echo "flashing $FW via CMSIS-DAP @ ${SPEED} kHz"
if "$OPENOCD" \
    -f interface/cmsis-dap.cfg \
    -f target/nordic/nrf52.cfg \
    -c "adapter speed ${SPEED}" \
    -c "program ${FW} ${VERIFY} reset exit" >"$LOG" 2>&1; then
  grep -E "Verified OK|wrote|Programming Finished" "$LOG" || true
  echo "done"
else
  echo "flash FAILED - OpenOCD log: $LOG" >&2
  tail -15 "$LOG" >&2
  exit 1
fi
