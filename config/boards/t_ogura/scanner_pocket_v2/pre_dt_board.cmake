# SPDX-License-Identifier: MIT
#
# Suppresses duplicate unit-address warnings for power, clock, acl and
# flash-controller nodes in the Nordic SoC dtsi (same as ZMK's own nRF boards).
list(APPEND EXTRA_DTC_FLAGS "-Wno-unique_unit_address_if_enabled")
