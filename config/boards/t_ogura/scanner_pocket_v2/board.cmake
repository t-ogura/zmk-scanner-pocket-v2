# SPDX-License-Identifier: MIT
#
# Flashed over SWD with the Scanner Pocket SWD Programmer (CMSIS-DAP) and
# OpenOCD, so `west flash` uses the openocd runner. The board name has no
# "nrf52" token for the nrf5 helper to pattern-match, hence the explicit
# subfamily.
set(OPENOCD_NRF5_SUBFAMILY nrf52)
set(OPENOCD_NRF5_INTERFACE cmsis-dap)
include(${ZEPHYR_BASE}/boards/common/openocd-nrf5.board.cmake)
