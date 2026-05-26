# Opens Publish-ToGitHub-Desktop.ps1 in a NEW visible PowerShell window on your desktop.
# Use this from Cursor's terminal when `git push` from the agent does not show credential prompts.
#
# Usage:
#   .\Scripts\Open-PublishToGitHub-Window.ps1
#   .\Scripts\Open-PublishToGitHub-Window.ps1 -Branch main -Remote origin

param(
    [string] $Remote = 'origin',
    [string] $Branch = 'main'
)

$ErrorActionPreference = 'Stop'
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
$publishScript = Join-Path $scriptDir 'Publish-ToGitHub-Desktop.ps1'

if (-not (Test-Path -LiteralPath $publishScript)) {
    Write-Error "Missing: $publishScript"
}

$pwshCmd = Get-Command pwsh.exe -ErrorAction SilentlyContinue
$shell = if ($pwshCmd) { $pwshCmd.Source } else { Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe' }

$args = @(
    '-NoExit'
    '-ExecutionPolicy', 'Bypass'
    '-NoProfile'
    '-File', $publishScript
    '-Remote', $Remote
    '-Branch', $Branch
)

Start-Process -FilePath $shell -WorkingDirectory $repoRoot -ArgumentList $args
Write-Host "Launched desktop window: $shell" -ForegroundColor Green
Write-Host "Check your taskbar for the new PowerShell window (git / credential prompts appear there)."
