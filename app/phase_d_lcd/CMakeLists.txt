# Phase D bring-up: drive the Sharp LS013B7DH05 through Zephyr's display API
# with no ZMK and no LVGL (HANDOVER 17 Phase D). Proves the five LCD nets,
# SPI timing, VCOM inversion and DISP before anything heavier is added.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(phase_d_lcd)
target_sources(app PRIVATE src/main.c)
