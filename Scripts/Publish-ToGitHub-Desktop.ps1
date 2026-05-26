# Interactive git push in a normal desktop PowerShell window (credential / MFA prompts work).
# Prefer starting this via Open-PublishToGitHub-Window.ps1 from Cursor so you get a new window.
#
# Usage (from repo root or anywhere):
#   .\Scripts\Open-PublishToGitHub-Window.ps1
#   .\Scripts\Publish-ToGitHub-Desktop.ps1 -Remote origin -Branch main

param(
    [Parameter()]
    [string] $RepoRoot,

    [Parameter()]
    [string] $Remote = 'origin',

    [Parameter()]
    [string] $Branch = 'main',

    [switch] $SkipConfirm
)

$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
    $here = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $here '..')).Path
}

Set-Location -LiteralPath $RepoRoot
Write-Host "Repo: $RepoRoot" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot '.git'))) {
    Write-Error "Not a git repository: $RepoRoot"
}

$git = Get-Command git -ErrorAction Stop

& $git status -sb
Write-Host ""

$ahead = 0
try {
    $cnt = & $git rev-list --count "${Remote}/${Branch}..HEAD" 2>$null
    if ($cnt -match '^\d+$') { $ahead = [int]$cnt }
} catch { }

if ($ahead -gt 0) {
    Write-Host "Local HEAD is $ahead commit(s) ahead of ${Remote}/${Branch}." -ForegroundColor Yellow
} else {
    Write-Host "Nothing to push (or cannot compare to ${Remote}/${Branch})." -ForegroundColor Yellow
}
Write-Host ""

if (-not $SkipConfirm) {
    $ans = Read-Host "Push to ${Remote} ${Branch}? [y/N]"
    if ($ans -notin @('y', 'Y', 'yes', 'YES')) {
        Write-Host "Cancelled."
        exit 0
    }
}

Write-Host "Running: git push ${Remote} ${Branch}" -ForegroundColor Green
& $git push $Remote $Branch
$code = $LASTEXITCODE
if ($code -ne 0) {
    Write-Host "`ngit push exited with $code" -ForegroundColor Red
} else {
    Write-Host "`nPush finished OK." -ForegroundColor Green
}

Write-Host "`nPress Enter to close this window..."
$null = Read-Host

exit $code
