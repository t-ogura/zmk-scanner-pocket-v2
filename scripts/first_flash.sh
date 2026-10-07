#!/usr/bin/env bash
# Linux/WSL twin of first_flash.ps1 - see that file for what and why.
#   scripts/first_flash.sh [build-phase-a/zephyr/zephyr.hex] [--speed kHz]
set -euo pipefail
FW="${1:-build-phase-a/zephyr/zephyr.hex}"; SPEED=500; shift || true
while [ $# -gt 0 ]; do case "$1" in --speed) SPEED="$2"; shift 2;; *) echo "unknown: $1" >&2; exit 2;; esac; done
OPENOCD="${OPENOCD:-$HOME/.local/opt/openocd/bin/openocd}"
HERE="$(cd "$(dirname "$0")" && pwd)"
[ -f "$FW" ] || { echo "firmware not found: $FW" >&2; exit 1; }

echo "=== 1/2 unlock (CTRL-AP ERASEALL, DAP only) ==="
"$OPENOCD" -f interface/cmsis-dap.cfg -f "$HERE/nrf52_recover_daponly.cfg" \
  -c "adapter speed $SPEED" -c "init" -c "recover_daponly" -c "exit"

echo; echo "=== 2/2 UICR.APPROTECT=HwDisabled, program, verify, reset (no power cycle before this) ==="
"$OPENOCD" -f interface/cmsis-dap.cfg -f target/nordic/nrf52.cfg \
  -c "adapter speed $SPEED" -c "init" -c "halt" \
  -c "flash fillw 0x10001208 0x0000005A 1" \
  -c "flash write_image erase $FW" -c "verify_image $FW" -c "reset run" -c "exit"
echo; echo "done. Use scripts/flash.sh for normal flashing from now on."
