#Requires -Version 5.1
<#
.SYNOPSIS
  Build RawIron_full_release_with_builds.zip from the repo tree, split into .part01-.part03 for GitHub Releases.
  Installer/ is excluded from the big archive (ship Installer_upload separately or from repo).

.EXAMPLE
  .\Scripts\Publish-FullWorkspaceSplitZip.ps1 -OutputDir D:\RawIronReleaseDrop

.EXAMPLE
  .\Scripts\Publish-FullWorkspaceSplitZip.ps1 -WhatIf
#>
param(
    [string] $RepoRoot,
    [string] $OutputDir,
    [ValidateRange(100MB, 1950MB)]
    [long] $MaxPartBytes = 1900MB,
    [switch] $IncludeGit,
    [switch] $WhatIf
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot 'ReleaseArtifacts'
}

$zipBaseName = 'RawIron_full_release_with_builds.zip'
$partNames = @(
    "$zipBaseName.part01",
    "$zipBaseName.part02",
    "$zipBaseName.part03"
)

function Split-FileIntoParts {
    param(
        [string] $SourcePath,
        [string] $OutputDirectory,
        [long] $ChunkSize,
        [string[]] $PartBasenames
    )
    $in = [System.IO.File]::OpenRead($SourcePath)
    $buffer = New-Object byte[] ([Math]::Min([long](64MB), $ChunkSize))
    try {
        $partIdx = 0
        while ($in.Position -lt $in.Length) {
            if ($partIdx -ge $PartBasenames.Count) {
                throw "Archive exceeds $($PartBasenames.Count) parts at chunk size $ChunkSize bytes. Increase MaxPartBytes or add part names."
            }
            $outPath = Join-Path $OutputDirectory $PartBasenames[$partIdx]
            if (Test-Path -LiteralPath $outPath) { Remove-Item -LiteralPath $outPath -Force }
            $out = [System.IO.File]::Create($outPath)
            try {
                [long]$writtenPart = 0
                while ($writtenPart -lt $ChunkSize -and $in.Position -lt $in.Length) {
                    $need = [long]($ChunkSize - $writtenPart)
                    $toRead = [int][Math]::Min([long]$buffer.Length, $need)
                    $read = $in.Read($buffer, 0, $toRead)
                    if ($read -eq 0) { break }
                    $out.Write($buffer, 0, $read)
                    $writtenPart += $read
                }
            }
            finally {
                $out.Dispose()
            }
            $partIdx++
        }
    }
    finally {
        $in.Dispose()
    }
}

function Invoke-RobocopyStage {
    param([string]$Src, [string]$Dst, [switch]$IncludeDotGit)
    New-Item -ItemType Directory -Force -Path $Dst | Out-Null
    $xd = @(
        '/XD', 'node_modules',
        '/XD', '.vs',
        '/XD', 'Installer',
        '/XD', 'out',
        '/XD', '.tmp'
    )
    if (-not $IncludeDotGit) {
        $xd = @('/XD', '.git') + $xd
    }
    $rcArgs = @($Src, $Dst, '/MIR', '/R:2', '/W:2', '/NFL', '/NDL', '/NJH', '/NJS', '/NP') + $xd
    & robocopy.exe @rcArgs
    $code = $LASTEXITCODE
    if ($code -ge 8) {
        throw "robocopy failed with exit code $code"
    }
}

if ($WhatIf) {
    Write-Host "WhatIf: mirror repo -> temp excluding node_modules, .vs, Installer, out, .tmp$(if (-not $IncludeGit) { ', .git' })"
    Write-Host "WhatIf: zip -> $zipBaseName then split ($MaxPartBytes bytes/part max) -> $OutputDir"
    Write-Host "WhatIf: copy Installer -> $OutputDir\Installer_upload\"
    Write-Host "RepoRoot: $RepoRoot"
    exit 0
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$stage = Join-Path $env:TEMP "rawiron-fullstage-$stamp"
$zipPath = Join-Path $env:TEMP "rawiron-full-$stamp.zip"

try {
    Write-Host "Staging workspace -> $stage (Installer excluded from archive) ..."
    Invoke-RobocopyStage -Src $RepoRoot -Dst $stage -IncludeDotGit:$IncludeGit

    Write-Host "Compressing (may take several minutes for multi-GB tree) ..."
    if (Test-Path -LiteralPath $zipPath) { Remove-Item $zipPath -Force }
    [System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $zipPath, [System.IO.Compression.CompressionLevel]::Fastest, $false)

    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host ""
    Write-Host 'SHA256 (joined zip - paste into Installer ExpectedSha256):'
    Write-Host $hash
    Write-Host ""

    $len = (Get-Item -LiteralPath $zipPath).Length
    Write-Host ("Zip size: {0:N3} GB" -f ($len / 1GB))

    Write-Host "Splitting into parts -> $OutputDir ..."
    Split-FileIntoParts -SourcePath $zipPath -OutputDirectory $OutputDir -ChunkSize $MaxPartBytes -PartBasenames $partNames

    # GitHub rejects 0-byte release assets. If part03 is missing or empty, move the last byte of part02 into part03
    # (concatenation part01||part02||part03 is unchanged; SHA256 of joined zip unchanged).
    $p2path = Join-Path $OutputDir $partNames[1]
    $p3path = Join-Path $OutputDir $partNames[2]
    $p3len = 0L
    if (Test-Path -LiteralPath $p3path) { $p3len = (Get-Item -LiteralPath $p3path).Length }
    if ($p3len -eq 0 -and (Test-Path -LiteralPath $p2path)) {
        $len2 = (Get-Item -LiteralPath $p2path).Length
        if ($len2 -lt 1) { throw 'Cannot emit part03: part02 is empty.' }
        $fs = [System.IO.File]::OpenRead($p2path)
        try {
            $null = $fs.Seek(-1, [System.IO.SeekOrigin]::End)
            $one = New-Object byte[] 1
            [void]$fs.Read($one, 0, 1)
        }
        finally {
            $fs.Dispose()
        }
        $rw = [System.IO.File]::Open($p2path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite)
        try {
            $rw.SetLength($len2 - 1)
        }
        finally {
            $rw.Dispose()
        }
        [System.IO.File]::WriteAllBytes($p3path, $one)
        Write-Host 'Adjusted part03 for GitHub (non-zero): moved last byte from part02; joined ZIP hash unchanged.'
    }

    $iu = Join-Path $OutputDir 'Installer_upload'
    Write-Host "Copying Installer (outside big zip) -> $iu"
    New-Item -ItemType Directory -Force -Path $iu | Out-Null
    Copy-Item -Path (Join-Path $RepoRoot 'Installer\*') -Destination $iu -Recurse -Force

    Write-Host ""
    Write-Host "Done. Upload to GitHub Release:"
    Write-Host "  $($partNames -join "`n  ") "
    Write-Host "  $iu\ (optional zip Installer_upload.zip)"
    Write-Host "Then set Installer\RawIron.FullWorkspace.Installer.ps1 ReleaseTag + ExpectedSha256 above."
}
finally {
    if (Test-Path -LiteralPath $stage) {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue
    }
}
