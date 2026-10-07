# Phase D bit-bang: talk to the LS013B7DH05 with plain GPIO and explicit
# datasheet timing - no SPI driver, no display driver. Removes every software
# layer between the firmware and the panel.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(phase_d_bitbang)
target_sources(app PRIVATE src/main.c)
