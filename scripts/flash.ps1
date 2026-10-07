# Flash a Zephyr/ZMK image into the Scanner Pocket v2 (nRF52832) over SWD
# through the Scanner Pocket SWD Programmer (CMSIS-DAP) with a native
# Windows OpenOCD (so no usbipd pass-through into WSL is needed).
#
#   .\scripts\flash.ps1 build\zephyr\zmk.hex [-Speed 1000] [-NoVerify]
#
# Set $env:OPENOCD to the openocd.exe path if it is not on PATH.
param(
    [string]$Firmware = "build\zephyr\zmk.hex",
    [int]$Speed = 1000,
    [switch]$NoVerify
)
$ErrorActionPreference = "Stop"

if (-not (Test-Path $Firmware)) { throw "firmware not found: $Firmware" }
if ($Firmware -notmatch '\.(hex|elf)$') { throw "use a .hex or .elf" }

$openocd = if ($env:OPENOCD) { $env:OPENOCD } else { "openocd" }
if (-not (Get-Command $openocd -ErrorAction SilentlyContinue)) {
    throw "openocd not found ($openocd). Install xPack OpenOCD and set `$env:OPENOCD or PATH."
}

$verify = if ($NoVerify) { "" } else { "verify" }
$log = Join-Path (Split-Path $Firmware -Parent) "flash.log"
Write-Host "flashing $Firmware via CMSIS-DAP @ $Speed kHz"

& $openocd -f interface/cmsis-dap.cfg -f target/nordic/nrf52.cfg `
    -c "adapter speed $Speed" `
    -c "program $Firmware $verify reset exit" *> $log

if ($LASTEXITCODE -ne 0) {
    Get-Content $log -Tail 15 | Write-Host
    throw "flash FAILED - OpenOCD log: $log"
}
Select-String -Path $log -Pattern "Verified OK|Programming Finished" | ForEach-Object { $_.Line }
Write-Host "done"
