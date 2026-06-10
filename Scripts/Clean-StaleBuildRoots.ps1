# Removes legacy / duplicate CMake trees under <repo>/build while keeping build/dev-msvc.
#
# Deletes:
#   - Flat ninja configure at build/ root (cmake -B build without a preset)
#   - Unused preset subdirs: build/dev-clang, build/dev-mingw, build/dev-linux-clang
#
# Keeps:
#   - build/dev-msvc  (canonical Windows output; see CMakePresets.json)
#
# Full wipe including dev-msvc: Scripts/Clean-RawIronBuild.ps1

param(
    [Parameter()]
    [string] $RepoRoot
)

$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
}

function Remove-RobustTree {
    param([string]$LiteralPath)

    if (!(Test-Path -LiteralPath $LiteralPath)) {
        return
    }
    $full = '\\?\' + $LiteralPath.TrimEnd('\')
    cmd.exe /c "rd /s /q `"$full`"" | Out-Null
}

$buildDir = Join-Path $RepoRoot 'build'
if (!(Test-Path -LiteralPath $buildDir)) {
    Write-Host "Nothing to clean: $buildDir does not exist."
    exit 0
}

foreach ($presetDir in @('dev-clang', 'dev-mingw', 'dev-linux-clang')) {
    $path = Join-Path $buildDir $presetDir
    if (Test-Path -LiteralPath $path) {
        Write-Host "Removing stale preset tree: $path"
        Remove-RobustTree -LiteralPath $path
    }
}

$rootArtifacts = @(
    'Apps', 'Games', 'Source', 'Tools', 'Testing', '_deps', 'CMakeFiles',
    'CMakeCache.txt', 'build.ninja', 'cmake_install.cmake', 'compile_commands.json',
    '.ninja_deps', '.ninja_log'
)

foreach ($name in $rootArtifacts) {
    $path = Join-Path $buildDir $name
    if (Test-Path -LiteralPath $path) {
        Write-Host "Removing legacy build root artifact: $path"
        if ((Get-Item -LiteralPath $path) -is [System.IO.DirectoryInfo]) {
            Remove-RobustTree -LiteralPath $path
        } else {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

Write-Host "Done. Canonical build tree: $(Join-Path $buildDir 'dev-msvc')"
Write-Host "Configure: cmake --preset dev-msvc"
