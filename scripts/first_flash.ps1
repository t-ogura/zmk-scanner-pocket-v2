# First flash of a Scanner Pocket v2 whose HY0020 still has the factory
# APPROTECT engaged (hardened APPROTECT: re-locks on every power-on).
#
# Does, in one go and WITHOUT power-cycling the target in between:
#   1. CTRL-AP ERASEALL with no cortex_m target defined (the only variant that
#      completes on this part) -> port open until the next power-on
#   2. New session with the real target: examine, halt (no reset!)
#   3. Write UICR.APPROTECT = 0x5A (HwDisabled) so the firmware's SystemInit
#      re-opens the port at every boot from now on
#   4. Program + verify the image, then reset and run
#
# After this, the normal scripts\flash.ps1 works for every later flash.
#
#   .\scripts\first_flash.ps1 build-phase-a\zephyr\zephyr.hex [-Speed 500]
param(
    [string]$Firmware = "build-phase-a\zephyr\zephyr.hex",
    [int]$Speed = 500
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path $Firmware)) { throw "firmware not found: $Firmware" }
$fw = (Resolve-Path $Firmware).Path -replace '\\','/'
$recover = (Join-Path $here "nrf52_recover_daponly.cfg") -replace '\\','/'

$openocd = if ($env:OPENOCD) { $env:OPENOCD } else { "openocd" }
if (-not (Get-Command $openocd -ErrorAction SilentlyContinue)) {
    throw "openocd not found ($openocd). Set `$env:OPENOCD."
}

Write-Host "=== 1/2  unlock (CTRL-AP ERASEALL, DAP only) ==="
& $openocd -f interface/cmsis-dap.cfg -f $recover `
    -c "adapter speed $Speed" -c "init" -c "recover_daponly" -c "exit"
if ($LASTEXITCODE -ne 0) { throw "unlock step failed" }

Write-Host ""
Write-Host "=== 2/2  UICR.APPROTECT=HwDisabled, program, verify, reset  (NO power cycle before this) ==="
& $openocd -f interface/cmsis-dap.cfg -f target/nordic/nrf52.cfg `
    -c "adapter speed $Speed" `
    -c "init" `
    -c "halt" `
    -c "flash fillw 0x10001208 0x0000005A 1" `
    -c "flash write_image erase $fw" `
    -c "verify_image $fw" `
    -c "reset run" `
    -c "exit"
if ($LASTEXITCODE -ne 0) { throw "program step failed (if 'AP lock engaged' appeared, the target was reset between the two steps - run again)" }

Write-Host ""
Write-Host "done. From now on use scripts\flash.ps1 for normal flashing."
