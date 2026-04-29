# Removes reproducible CMake output only: <repo>/build (see .gitignore) and optional root compile_commands.json.
# NEVER deletes Assets/, Games/, Saved/, ThirdParty/, or other source/paid content — only the CMake build tree.
# Regenerate with: cmake --preset dev-msvc (or your preset), then optionally copy compile_commands.json for clangd.

param(
    [Parameter()]
    [string] $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
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

$ccs = Join-Path $RepoRoot 'compile_commands.json'
if (Test-Path -LiteralPath $ccs) {
    Remove-Item -LiteralPath $ccs -Force
}

Write-Host "Done. CMake build tree removed under: $(Join-Path $RepoRoot 'build')"
Write-Host "Next: cmake --preset dev-msvc   (creates build\dev-msvc and compile_commands there)"
