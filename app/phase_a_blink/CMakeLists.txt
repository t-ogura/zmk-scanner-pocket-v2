# Phase A bring-up: plain Zephyr, no ZMK. Proves board definition + SWD flash
# + main() + GPIO before any ZMK feature is involved (HANDOVER 17 / 19.5).
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(phase_a_blink)
target_sources(app PRIVATE src/main.c)
