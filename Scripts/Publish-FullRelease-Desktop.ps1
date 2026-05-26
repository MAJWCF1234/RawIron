#Requires -Version 5.1
<#
.SYNOPSIS
  Maintainer wizard: optional full MSVC build, split full-workspace ZIP, SHA256 for joined file,
  generated release notes, optional patch to Installer defaults, optional gh release upload.

  Run via Open-PublishFullRelease-Window.ps1 so prompts and gh auth happen on your desktop.

  See: Documentation/04 Build/GitHub Push and Publish.md
#>
param(
    [string] $RepoRoot,
    [string] $OutputDir,
    [string] $ReleaseTag = '',
    [switch] $SkipBuild,
    [switch] $SkipInstallerPatch
)

$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
    $here = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $here '..')).Path
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot 'ReleaseArtifacts'
}
if (-not $ReleaseTag) {
    $ReleaseTag = 'full-workspace-msvc-{0}' -f (Get-Date -Format 'yyyy-MM-dd')
}

Set-Location -LiteralPath $RepoRoot
Write-Host "`n=== RawIron full-workspace release ===" -ForegroundColor Cyan
Write-Host "Repo: $RepoRoot"
Write-Host "Output: $OutputDir"
Write-Host "Tag:    $ReleaseTag`n"

if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot '.git'))) {
    Write-Error "Not a git repository: $RepoRoot"
}

function Get-Sha256JoinedZipParts {
    param([string] $Dir)
    $names = @(
        'RawIron_full_release_with_builds.zip.part01',
        'RawIron_full_release_with_builds.zip.part02',
        'RawIron_full_release_with_builds.zip.part03'
    )
    foreach ($n in $names) {
        $p = Join-Path $Dir $n
        if (-not (Test-Path -LiteralPath $p)) {
            throw "Missing part for hash: $p"
        }
    }
    $joined = Join-Path $env:TEMP ("rawiron-joined-{0}.zip" -f [Guid]::NewGuid().ToString('N'))
    $outStream = [System.IO.File]::Create($joined)
    try {
        foreach ($n in $names) {
            $path = Join-Path $Dir $n
            $inStream = [System.IO.File]::OpenRead($path)
            try {
                $bufSize = 8 * 1024 * 1024
                $buf = New-Object byte[] $bufSize
                while ($true) {
                    $r = $inStream.Read($buf, 0, $buf.Length)
                    if ($r -le 0) { break }
                    $outStream.Write($buf, 0, $r)
                }
            }
            finally {
                $inStream.Dispose()
            }
        }
    }
    finally {
        $outStream.Dispose()
    }
    $hash = (Get-FileHash -LiteralPath $joined -Algorithm SHA256).Hash.ToLowerInvariant()
    Remove-Item -LiteralPath $joined -Force
    return $hash
}

function Write-ReleaseNotesFile {
    param([string] $Path, [string] $Tag, [string] $Sha)
    $lines = @(
        "# Release ``$Tag``"
        ''
        'Full workspace drop (sources + ``build\dev-msvc`` + assets) split for GitHub asset size limits.'
        ''
        '## Download'
        ''
        'Concatenate in order:'
        ''
        '- ``RawIron_full_release_with_builds.zip.part01``'
        '- ``RawIron_full_release_with_builds.zip.part02``'
        '- ``RawIron_full_release_with_builds.zip.part03``'
        ''
        'Result: single file ``RawIron_full_release_with_builds.zip``'
        ''
        '## SHA256 (joined file, before extract)'
        ''
        '```'
        $Sha
        '```'
        ''
        '## Installer'
        ''
        'Use **``RawIron_Installer.zip``** from this release, or run **``Installer/RawIron.FullWorkspace.Installer.cmd``** from a **git clone** after updating **``Installer/RawIron.FullWorkspace.Installer.ps1``** to this tag + SHA.'
        ''
        '## Notes'
        ''
        '- **``Installer/``** is excluded from the big ZIP; ship the small installer bundle separately.'
        '- If **``part03``** is 1 byte, see ``Scripts/Publish-FullWorkspaceSplitZip.ps1`` (GitHub rejects empty assets; joined hash unchanged).'
        ''
        '## Build from source'
        ''
        'Prefer cloning **``main``** and the root **README** Quick start unless you need this offline bundle.'
    )
    $body = $lines -join [Environment]::NewLine
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $body -Encoding UTF8
}

function Invoke-PatchInstallerDefaults {
    param([string] $InstallerPs1, [string] $Tag, [string] $Sha)
    $c = Get-Content -LiteralPath $InstallerPs1 -Raw -Encoding UTF8
    $before = $c
    $c2 = [regex]::Replace(
        $c,
        '\[string\]\s+\$ReleaseTag\s*=\s*''[^'']*'',',
        "    [string] `$ReleaseTag = '$Tag',",
        1
    )
    $c2 = [regex]::Replace(
        $c2,
        '\[string\]\s+\$ExpectedSha256\s*=\s*''[^'']*'',',
        "    [string] `$ExpectedSha256 = '$Sha',",
        1
    )
    if ($c2 -eq $before) {
        Write-Warning "Installer patch: could not find ReleaseTag or ExpectedSha256 defaults to replace. Edit manually."
        return $false
    }
    Set-Content -LiteralPath $InstallerPs1 -Value $c2 -Encoding UTF8
    return $true
}

# --- Optional full build (same targets as README Quick start) ---
if (-not $SkipBuild) {
    Write-Host "Run a full RelWithDebInfo build (cmake preset dev-msvc + all main apps/games)?" -ForegroundColor Yellow
    Write-Host "  Requires cmake + MSVC on PATH (e.g. x64 Native Tools or VS Developer shell)."
    $b = Read-Host "[y/N]"
    if ($b -in @('y', 'Y', 'yes', 'YES')) {
        $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if (-not $cmake) {
            Write-Error "cmake.exe not on PATH. Open 'x64 Native Tools Command Prompt for VS 2022' or add CMake to PATH, then re-run."
        }
        Write-Host "`nConfiguring..."
        & cmake.exe --preset dev-msvc
        if ($LASTEXITCODE -ne 0) { throw "cmake --preset dev-msvc failed ($LASTEXITCODE)" }

        $targets = @(
            'RawIron.Player', 'RawIron.Preview', 'RawIron.Editor', 'RawIron.VisualShell',
            'RawIron.UiMenu', 'RawIron.ParticleShowcase', 'RawIron.LiminalGame', 'RawIron.ForestRuinsGame'
        )
        $args = @('--build', 'build/dev-msvc', '--config', 'RelWithDebInfo', '--parallel')
        foreach ($t in $targets) {
            $args += @('--target', $t)
        }
        Write-Host "`nBuilding targets: $($targets -join ', ') ..."
        & cmake.exe @args
        if ($LASTEXITCODE -ne 0) { throw "cmake --build failed ($LASTEXITCODE)" }
        Write-Host "`nBuild step finished OK.`n" -ForegroundColor Green
    }
}

# --- Split ZIP (existing script prints SHA; we recompute from parts after) ---
Write-Host "Run Publish-FullWorkspaceSplitZip.ps1 (robocopy stage + zip + split)? This can take a long time and needs free disk space." -ForegroundColor Yellow
$z = Read-Host "[Y/n]"
if ($z -notin @('', 'y', 'Y', 'yes', 'YES')) {
    Write-Host "Cancelled."
    exit 0
}

$splitScript = Join-Path $PSScriptRoot 'Publish-FullWorkspaceSplitZip.ps1'
if (-not (Test-Path -LiteralPath $splitScript)) {
    Write-Error "Missing $splitScript"
}

try {
    & $splitScript -RepoRoot $RepoRoot -OutputDir $OutputDir
}
catch {
    throw "Publish-FullWorkspaceSplitZip.ps1 failed: $_"
}

Write-Host "`nComputing SHA256 of joined parts (matches installer reassembly)..." -ForegroundColor Cyan
$sha = Get-Sha256JoinedZipParts -Dir $OutputDir
Write-Host "Joined SHA256: $sha" -ForegroundColor Green

$notesOut = Join-Path $OutputDir 'release-notes-GITHUB-DRAFT.md'
Write-ReleaseNotesFile -Path $notesOut -Tag $ReleaseTag -Sha $sha
Write-Host "Wrote: $notesOut" -ForegroundColor Green

# --- Optional installer patch ---
if (-not $SkipInstallerPatch) {
    $inst = Join-Path $RepoRoot 'Installer\RawIron.FullWorkspace.Installer.ps1'
    if (Test-Path -LiteralPath $inst) {
        Write-Host "`nPatch $inst defaults (ReleaseTag + ExpectedSha256)? [y/N]" -ForegroundColor Yellow
        $p = Read-Host
        if ($p -in @('y', 'Y', 'yes', 'YES')) {
            if (Invoke-PatchInstallerDefaults -InstallerPs1 $inst -Tag $ReleaseTag -Sha $sha) {
                Write-Host "Installer defaults updated. Commit this change on main when you are ready." -ForegroundColor Green
            }
        }
    }
}

# --- Optional gh release ---
$gh = Get-Command gh.exe -ErrorAction SilentlyContinue
if ($gh) {
    Write-Host "`nGitHub CLI found. Create a release with attached part01-03?" -ForegroundColor Yellow
    Write-Host "  (You must be logged in: gh auth login. Large uploads can take a while.)"
    $draft = Read-Host "Create as draft? [Y/n]"
    $isDraft = $draft -notin @('n', 'N', 'no', 'NO')
    $doGh = Read-Host "Run gh release create now? [y/N]"
    if ($doGh -in @('y', 'Y', 'yes', 'YES')) {
        $p1 = Join-Path $OutputDir 'RawIron_full_release_with_builds.zip.part01'
        $p2 = Join-Path $OutputDir 'RawIron_full_release_with_builds.zip.part02'
        $p3 = Join-Path $OutputDir 'RawIron_full_release_with_builds.zip.part03'
        $ghArgs = @(
            'release', 'create', $ReleaseTag,
            $p1, $p2, $p3,
            '--title', "RawIron full workspace ($ReleaseTag)",
            '--notes-file', $notesOut
        )
        if ($isDraft) {
            $ghArgs += '--draft'
        }
        Write-Host "`nRunning: gh $($ghArgs -join ' ')`n" -ForegroundColor Cyan
        & gh.exe @ghArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "gh release create exited $LASTEXITCODE. Upload parts manually from $OutputDir"
        }
    }
}
else {
    Write-Host "`nInstall GitHub CLI (gh) and run 'gh auth login' to automate release upload; otherwise attach files manually:" -ForegroundColor Yellow
    Write-Host "  $OutputDir\RawIron_full_release_with_builds.zip.part0*"
    Write-Host "  $notesOut"
}

Write-Host "`n=== Done ===" -ForegroundColor Cyan
Write-Host "Remember: copy SHA into ReleaseArtifacts/release-notes.md if you keep that file in sync, and verify Installer defaults match this release."
Write-Host "`nPress Enter to close..."
$null = Read-Host
