# Removes reproducible CMake output only: <repo>/build (see .gitignore) and optional root compile_commands.json.
# Optionally removes %LOCALAPPDATA%\RawIron\cmake-build (output of dev-msvc-localappdata) — still NOT any Assets/.
# NEVER deletes Assets/, Games/, Saved/, ThirdParty/, or other source/paid content — only CMake build trees.
# Regenerate with: cmake --preset dev-msvc (or your preset), then optionally copy compile_commands.json for clangd.

param(
    [Parameter()]
    [string] $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path,

    [Parameter(HelpMessage = 'Also remove %LOCALAPPDATA%\RawIron\cmake-build (MSVC preset dev-msvc-localappdata output).')]
    [switch] $IncludeProfileCMakeBuild
)

$ErrorActionPreference = 'Stop'

function Remove-RobustTree {
    param([string]$LiteralPath)

    $full = '\\?\' + $LiteralPath.TrimEnd('\')
    if (!(Test-Path -LiteralPath $LiteralPath)) {
        return
    }
    cmd.exe /c "rd /s /q `"$full`"" | Out-Null
}

Remove-RobustTree -LiteralPath (Join-Path $RepoRoot 'build')

if ($IncludeProfileCMakeBuild) {
    $profileBuild = Join-Path $env:LOCALAPPDATA 'RawIron\cmake-build'
    Write-Host "Removing profile CMake output (reproducible): $profileBuild"
    Remove-RobustTree -LiteralPath $profileBuild
}

$ccs = Join-Path $RepoRoot 'compile_commands.json'
if (Test-Path -LiteralPath $ccs) {
    Remove-Item -LiteralPath $ccs -Force
}

Write-Host "Done. CMake build tree removed under: $(Join-Path $RepoRoot 'build')"
Write-Host "Next: cmake --preset dev-msvc   (creates build\dev-msvc and compile_commands there)"
Write-Host "Or (reliable on flaky volumes): cmake --preset dev-msvc-localappdata"
