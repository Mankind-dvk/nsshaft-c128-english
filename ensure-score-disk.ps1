# Creates the score D64 only when it is missing, then checks writable access.
# Returns the absolute path for run-x128.ps1. Existing score disks are kept.
#
# Quick reference:
#   line 17: first-launch disk creation
#   line 41: file-path check
#   line 46: writable-access check

param(
    [string]$DiskPath = (Join-Path $PSScriptRoot 'saves\nsshaft-scores.d64'),
    [string]$C1541Path
)

$ErrorActionPreference = 'Stop'
$DiskPath = [IO.Path]::GetFullPath($DiskPath)

if (-not (Test-Path -LiteralPath $DiskPath)) {
    if (!$C1541Path) { $C1541Path = (Get-Command c1541 -ErrorAction Stop).Source }
    if (-not (Test-Path -LiteralPath $C1541Path -PathType Leaf)) {
        throw "VICE c1541 was not found: $C1541Path"
    }

    $directory = Split-Path -Parent $DiskPath
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $temporaryPath = Join-Path $directory ('scores-' + [Guid]::NewGuid().ToString('N') + '.d64')
    try {
        $output = & $C1541Path -format 'NSSHAFT SCORES,NS' d64 $temporaryPath 2>&1
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $temporaryPath -PathType Leaf)) {
            throw "Could not create the score disk: $output"
        }
        # Never overwrite an existing disk, including one created by another launcher.
        Move-Item -LiteralPath $temporaryPath -Destination $DiskPath -ErrorAction Stop
    }
    finally {
        if (Test-Path -LiteralPath $temporaryPath -PathType Leaf) {
            Remove-Item -LiteralPath $temporaryPath
        }
    }
}

if (-not (Test-Path -LiteralPath $DiskPath -PathType Leaf)) {
    throw "The score disk path is not a file: $DiskPath"
}

# Check writable access before launching; VICE may already have this disk open.
$stream = [IO.File]::Open($DiskPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
$stream.Dispose()

Write-Output $DiskPath
