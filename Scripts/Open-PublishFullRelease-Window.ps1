# Opens Publish-FullRelease-Desktop.ps1 in a new PowerShell window (build + split ZIP + release notes + optional gh).
# Usage:
#   .\Scripts\Open-PublishFullRelease-Window.ps1
#   .\Scripts\Open-PublishFullRelease-Window.ps1 -SkipBuild

param(
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
$mainScript = Join-Path $scriptDir 'Publish-FullRelease-Desktop.ps1'

if (-not (Test-Path -LiteralPath $mainScript)) {
    Write-Error "Missing: $mainScript"
}

$pwshCmd = Get-Command pwsh.exe -ErrorAction SilentlyContinue
$shell = if ($pwshCmd) { $pwshCmd.Source } else { Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe' }

$argList = @(
    '-NoExit'
    '-ExecutionPolicy', 'Bypass'
    '-NoProfile'
    '-File', $mainScript
)
if ($SkipBuild) {
    $argList += '-SkipBuild'
}

Start-Process -FilePath $shell -WorkingDirectory $repoRoot -ArgumentList $argList
Write-Host "Launched full-release wizard in a new window: $shell" -ForegroundColor Green
Write-Host "That window runs the long zip step; keep it open until it finishes."
