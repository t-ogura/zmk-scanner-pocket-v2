# Phase D pin finder: drive each HY0020 GPIO high in turn so the DISP line of
# the LS013B7DH05 can be found by eye (noise appears when DISP goes high).
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(phase_d_pinfind)
target_sources(app PRIVATE src/main.c)
