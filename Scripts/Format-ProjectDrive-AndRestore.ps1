#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Format O: (or another data volume) as NTFS, verify health, then restore from a backup folder.

.DESCRIPTION
  Use when the project SSD was exFAT/non-NTFS and showed corruption or "Full Repair Needed".
  WARNING: Destroys all data on the target volume. Confirm disk letter and backup path before running.

  Close applications using the drive (including Cursor/VS Code if the workspace lives on that letter).

.NOTES
  Typical backup layout: copy root of `P:\project backup` onto `O:\` so paths match prior layout.

.EXAMPLE
  .\Scripts\Format-ProjectDrive-AndRestore.ps1 -DriveLetter O -BackupRoot 'P:\project backup'
.EXAMPLE
  .\Scripts\Format-ProjectDrive-AndRestore.ps1 -DriveLetter O -BackupRoot 'P:\project backup' -RestoreOnly
.EXAMPLE
  .\Scripts\Format-ProjectDrive-AndRestore.ps1 -WhatIf  # lists checks only; does not format
#>
param(
    [Parameter()]
    [char] $DriveLetter = 'O',

    [Parameter()]
    [string] $BackupRoot = 'P:\project backup',

    [Parameter()]
    [string] $VolumeLabel = 'RawIron',

    [Parameter(HelpMessage = 'Slow format that exercises sectors (optional).')]
    [switch] $FullFormat,

    [Parameter(HelpMessage = 'Only robocopy from BackupRoot to the drive (no format).')]
    [switch] $RestoreOnly,

    [Parameter()]
    [switch] $WhatIf
)

$ErrorActionPreference = 'Stop'

function Assert-NotSystemVolume {
    param([char]$Letter)
    $p = Get-Partition -DriveLetter $Letter -ErrorAction Stop
    if ($p.IsSystem -or $p.IsBoot -or $p.IsActive) {
        throw "Refusing to format drive letter '$Letter': partition is marked system/boot/active."
    }
}

Assert-NotSystemVolume -Letter $DriveLetter

if (-not (Test-Path -LiteralPath $BackupRoot)) {
    throw "Backup path not found: $BackupRoot"
}

$volBefore = Get-Volume -DriveLetter $DriveLetter -ErrorAction Stop
Write-Host "Volume before: $($volBefore.FileSystem) label='$($volBefore.FileSystemLabel)' health=$($volBefore.HealthStatus) op=$($volBefore.OperationalStatus)"

if ($WhatIf) {
    if ($RestoreOnly) {
        Write-Host 'WhatIf: would robocopy only from:'
    }
    else {
        Write-Host 'WhatIf: would Format-Volume NTFS, Repair-Volume -Scan, robocopy from:'
    }
    Write-Host "  $BackupRoot"
    Write-Host "  -> $($DriveLetter):\"
    exit 0
}

if (-not $RestoreOnly) {
    Write-Host "Formatting $($DriveLetter): as NTFS (label '$VolumeLabel')..."
    if ($FullFormat) {
        Format-Volume -DriveLetter $DriveLetter -FileSystem NTFS -NewFileSystemLabel $VolumeLabel -Full -Force -Confirm:$false
    }
    else {
        Format-Volume -DriveLetter $DriveLetter -FileSystem NTFS -NewFileSystemLabel $VolumeLabel -Confirm:$false
    }

    $volAfter = Get-Volume -DriveLetter $DriveLetter
    Write-Host "Volume after: $($volAfter.FileSystem) label='$($volAfter.FileSystemLabel)'"

    Write-Host 'Running Repair-Volume -Scan...'
    Repair-Volume -DriveLetter $DriveLetter -Scan
}
else {
    Write-Host 'RestoreOnly: skipping format and scan.'
}

$destRoot = Join-Path "$DriveLetter`:" ''
Write-Host "Robocopy (full tree copy):`n  From: $BackupRoot`n  To:   $destRoot"

$rcExe = Join-Path $env:SystemRoot 'System32\robocopy.exe'
if (-not (Test-Path -LiteralPath $rcExe)) {
    throw "robocopy not found: $rcExe"
}
# Use call operator & so paths with spaces stay single arguments (Start-Process -ArgumentList can split them).
$rcArgs = @(
    $BackupRoot
    $destRoot
    '/E'
    '/COPY:DAT'
    '/DCOPY:DA'
    '/R:3'
    '/W:5'
    '/MT:8'
    '/XJ'
    '/NP'
)
& $rcExe @rcArgs
$exitCode = $LASTEXITCODE
# Robocopy exit codes 0-7 are success variants
if ($exitCode -ge 8) {
    throw "robocopy exited $exitCode. See https://learn.microsoft.com/windows-server/administration/windows-commands/robocopy#exit-codes"
}

Write-Host 'Done. Re-open your workspace from the restored tree if needed.'
Write-Host 'Next: cmake --preset dev-msvc (or dev-msvc-localappdata) and build on NTFS.'
