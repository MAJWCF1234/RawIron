# Mirrors the MSVC build tree from %LOCALAPPDATA%\RawIron\cmake-build\dev-msvc
# into <repo>\build\dev-msvc so the whole RawIron folder can be copied or archived with binaries inside it.
#
# Use when you configured with dev-msvc-localappdata (build output outside the repo).
# Does NOT touch Assets/, Source/, etc. — only copies generated build products into build\.
#
# Requires: prior successful cmake --build --preset build-dev-msvc-localappdata

param(
    [Parameter()]
    [string] $RepoRoot,

    [Parameter()]
    [string] $Source = (Join-Path $env:LOCALAPPDATA 'RawIron\cmake-build\dev-msvc'),

    [Parameter()]
    [string] $DestSubdir = 'build\dev-msvc'
)

$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
}

if (!(Test-Path -LiteralPath $Source)) {
    throw "Source build not found: $Source`nBuild first with: cmake --preset dev-msvc-localappdata && cmake --build --preset build-dev-msvc-localappdata"
}

$dest = Join-Path $RepoRoot $DestSubdir
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null

Write-Host "Robocopy mirror:`n  From: $Source`n  To:   $dest`n"

$rcArgs = @(
    $Source
    $dest
    '/MIR'
    '/R:2'
    '/W:2'
    '/XJD'
    '/XJF'
    '/NFL'
    '/NDL'
    '/NP'
)

$p = Start-Process -FilePath 'robocopy.exe' -ArgumentList $rcArgs -Wait -PassThru -NoNewWindow
# Exit codes 0-7: success (with various copy outcomes); 8+ = failure
if ($p.ExitCode -ge 8) {
    throw "robocopy exited $($p.ExitCode). See https://learn.microsoft.com/windows-server/administration/windows-commands/robocopy#exit-codes"
}

Write-Host "Done. Binaries are now under: $dest"
Write-Host "You can copy or zip the entire repo folder; build\ is gitignored but travels with the folder."
