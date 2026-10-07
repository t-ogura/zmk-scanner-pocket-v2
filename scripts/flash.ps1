# Flash a Zephyr/ZMK image into the Scanner Pocket v2 (nRF52832) over SWD
# through the Scanner Pocket SWD Programmer (CMSIS-DAP) with a native
# Windows OpenOCD.
#
#   .\scripts\flash.ps1 build\zephyr\zmk.hex [-Speed 1000] [-NoVerify]
#
# Set $env:OPENOCD to the openocd.exe path if it is not on PATH.
#
# Implementation notes (both bit PowerShell 5.1 users once):
#  - OpenOCD writes everything to stderr. Redirecting that inside PowerShell
#    turns the first line into an ErrorRecord and, with ErrorActionPreference
#    Stop, aborts on the banner. So the capture is done by cmd.exe instead.
#  - The OpenOCD commands go into a temporary .cfg rather than -c "..." so no
#    quoting survives the PowerShell -> native boundary.
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

$fwPath = (Resolve-Path $Firmware).Path
$fw     = $fwPath -replace '\\','/'
$log    = Join-Path (Split-Path $fwPath -Parent) "flash.log"
$verify = if ($NoVerify) { "" } else { "verify" }

$cfg = Join-Path $env:TEMP "scanner_pocket_flash.cfg"
@"
adapter speed $Speed
program {$fw} $verify reset exit
"@ | Set-Content -Path $cfg -Encoding ASCII

Write-Host "flashing $Firmware via CMSIS-DAP @ $Speed kHz"
$cfgFwd = $cfg -replace '\\','/'
cmd /c "`"$openocd`" -f interface/cmsis-dap.cfg -f target/nordic/nrf52.cfg -f `"$cfgFwd`" > `"$log`" 2>&1"
$code = $LASTEXITCODE

if ($code -ne 0) {
    Get-Content $log -Tail 20 | Write-Host
    throw "flash FAILED (exit $code) - OpenOCD log: $log"
}
Select-String -Path $log -Pattern "wrote \d+ bytes|verified \d+ bytes|Verified OK" | ForEach-Object { $_.Line }
Write-Host "done"
