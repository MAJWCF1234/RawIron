# Push the current branch via a temporary bundle + clone on %TEMP%.
# Use when `git push` fails with "Access is denied" reading `.git/objects/pack`
# on some volumes (common with certain removable / non-NTFS filesystems).
#
# Safety:
# - Does not modify or delete anything under Assets/, Games/, Saved/, ThirdParty/, Source/, etc.
# - Only creates transient files under $env:TEMP and removes the clone after push.
# - Requires -Confirm so this never runs by accident from tooling.

param(
    [Parameter()]
    [string] $RepoRoot,

    [Parameter()]
    [string] $Branch = 'main',

    [Parameter()]
    [string] $Remote = 'origin',

    [switch] $Confirm,

    [switch] $WhatIf
)

$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
}

if (!$Confirm -and !$WhatIf) {
    Write-Host @'
Usage:
  .\Scripts\Git-PushViaBundle.ps1 -Confirm

Optional:
  -Branch other-branch -Remote origin -WhatIf

Creates a bundle from your repo, clones it under %TEMP%, pushes to the configured remote URL,
then deletes the temporary clone and bundle file.
'@
    exit 2
}

Push-Location -LiteralPath $RepoRoot
try {
    $remoteUrl = git remote get-url $Remote 2>$null
    if (!$remoteUrl) {
        throw "Remote '$Remote' is not configured. Add it first (git remote add $Remote <url>)."
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $bundle = Join-Path $env:TEMP "rawiron-push-$stamp.bundle"
    $cloneDir = Join-Path $env:TEMP "rawiron-push-clone-$stamp"

    if ($WhatIf) {
        Write-Host "WhatIf: would create bundle at $bundle"
        Write-Host "WhatIf: would clone to $cloneDir and push $Branch to $Remote ($remoteUrl)"
        exit 0
    }

    Write-Host "Creating bundle: $bundle"
    git bundle create $bundle $Branch
    if ($LASTEXITCODE -ne 0) { throw "git bundle create failed" }

    if (Test-Path -LiteralPath $cloneDir) {
        Remove-Item -LiteralPath $cloneDir -Recurse -Force
    }
    Write-Host "Cloning bundle to: $cloneDir"
    git clone $bundle $cloneDir -b $Branch
    if ($LASTEXITCODE -ne 0) { throw "git clone from bundle failed" }

    Set-Location -LiteralPath $cloneDir
    git remote remove origin 2>$null
    git remote add origin $remoteUrl
    Write-Host "Pushing $Branch to $Remote ..."
    git push origin $Branch
    if ($LASTEXITCODE -ne 0) { throw "git push failed" }

    Set-Location -LiteralPath $RepoRoot
    Remove-Item -LiteralPath $cloneDir -Recurse -Force
    Remove-Item -LiteralPath $bundle -Force
    Write-Host "Done. Push completed and temporary files removed."
}
finally {
    Pop-Location
}
