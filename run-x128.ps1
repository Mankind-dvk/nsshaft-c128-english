# Builds and starts the game in PAL C128 mode with mouse-controlled paddles.
# Mounts the existing score disk as writable device 8; a first launch creates it.
#
# Quick reference:
#   line 18: build
#   line 20: score-disk setup and emulator seed
#   line 25: VICE options and launch

param(
    [string]$VicePath = $env:VICE_X128,
    [string]$Cc65Bin = $env:CC65_BIN
)

$ErrorActionPreference = 'Stop'
if (!$VicePath) { $VicePath = (Get-Command x128 -ErrorAction Stop).Source }
if (!(Test-Path -LiteralPath $VicePath)) { throw 'Set VICE_X128 or pass -VicePath with the path to x128.exe.' }

& (Join-Path $PSScriptRoot 'build.ps1') -Cc65Bin $Cc65Bin

$c1541 = Join-Path (Split-Path -Parent $VicePath) 'c1541.exe'
$scoreDisk = & (Join-Path $PSScriptRoot 'ensure-score-disk.ps1') -C1541Path $c1541
$milliseconds = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$seed = [int](($milliseconds % 2147483646) + 1)

& $VicePath `
    +saveres `
    -pal `
    -40col `
    -hidevdcwindow `
    -seed $seed `
    -sound `
    -sounddev wmm `
    -soundvolume 100 `
    -controlport1device 2 `
    -controlport2device 0 `
    -paddles1inputmouse `
    -mouse `
    +autostart-warp `
    -autostart-drop-mode 2 `
    -drive8type 1571 `
    -drive8truedrive `
    -attach8rw `
    -8 $scoreDisk `
    -autostart-handle-tde `
    -autostartprgmode 1 `
    -autostart (Join-Path $PSScriptRoot 'build\nsshaft-c128.prg')
if ($LASTEXITCODE -ne 0) { throw "VICE failed with exit code $LASTEXITCODE" }
