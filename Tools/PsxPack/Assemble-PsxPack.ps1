#!/usr/bin/env pwsh
# Compatible with Windows PowerShell 5.1+ and PowerShell 7+
<#
.SYNOPSIS
  Assemble Wilderness Ruins assets/PsxPack from a private purchase root (read-only).

.DESCRIPTION
  Copies a purposeful subset into Games/WildernessRuins/assets/PsxPack.
  Never writes under -SourceRoot. Do not document or commit the purchase path.

  Pass -SourceRoot or set RAWIRON_PSX_SOURCE (session-only; not tracked).
#>
param(
    [string]$SourceRoot = $env:RAWIRON_PSX_SOURCE,
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [switch]$KeepStaging
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    throw "Pass -SourceRoot or set RAWIRON_PSX_SOURCE for this session."
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$destRoot = Join-Path $RepoRoot 'Games\WildernessRuins\assets\PsxPack'
$staging = Join-Path $RepoRoot 'Saved\Temp\PsxPack.staging'
$provenancePath = Join-Path $staging 'PROVENANCE.jsonl'
$manifestPath = Join-Path $staging 'ASSEMBLY_MANIFEST.json'

function Write-Utf8File([string]$Path, [string]$Content) {
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

function Copy-SelectedFile {
    param(
        [Parameter(Mandatory)][string]$RelativeSource,
        [Parameter(Mandatory)][string]$RelativeDest,
        [Parameter(Mandatory)][string]$Category,
        [Parameter(Mandatory)][string]$Reason,
        [string]$Consumer = 'ForestRuins'
    )
    $src = Join-Path $SourceRoot $RelativeSource
    if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
        Write-Warning "Missing source file: $RelativeSource"
        return $false
    }
    # Guard: refuse any destination under the licensed source root.
    $dest = Join-Path $staging $RelativeDest
    $destFull = [System.IO.Path]::GetFullPath($dest)
    if ($destFull.StartsWith($SourceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to write into licensed source root: $destFull"
    }
    $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest)
    Copy-Item -LiteralPath $src -Destination $dest -Force
    $srcHash = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash
    $dstHash = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash
    if ($srcHash -ne $dstHash) {
        throw "Checksum mismatch after copy: $RelativeSource"
    }
    $record = [ordered]@{
        dest           = ($RelativeDest -replace '\\', '/')
        # Relative path inside the private purchase root only (no root folder name, no drive).
        catalogRel     = ($RelativeSource -replace '\\', '/')
        category       = $Category
        reason         = $Reason
        consumer       = $Consumer
        conversion     = 'none (byte-identical copy)'
        originalFormat = [System.IO.Path]::GetExtension($src).TrimStart('.')
        packagedFormat = [System.IO.Path]::GetExtension($dest).TrimStart('.')
        sourceSha256   = $srcHash
        packagedSha256 = $dstHash
        selectedUtc    = (Get-Date).ToUniversalTime().ToString('o')
    }
    ($record | ConvertTo-Json -Compress) | Add-Content -LiteralPath $provenancePath -Encoding utf8
    return $true
}

function Copy-SelectedTree {
    param(
        [Parameter(Mandatory)][string]$RelativeSourceDir,
        [Parameter(Mandatory)][string]$RelativeDestDir,
        [Parameter(Mandatory)][string]$Category,
        [Parameter(Mandatory)][string]$Reason,
        [string[]]$Include = @('*'),
        [string[]]$ExcludeNameRegex = @(),
        [string]$Consumer = 'ForestRuins'
    )
    $srcDir = Join-Path $SourceRoot $RelativeSourceDir
    if (-not (Test-Path -LiteralPath $srcDir -PathType Container)) {
        Write-Warning "Missing source directory: $RelativeSourceDir"
        return 0
    }
    $count = 0
    Get-ChildItem -LiteralPath $srcDir -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($srcDir.Length).TrimStart('\', '/')
        $name = $_.Name
        $ok = $false
        foreach ($pat in $Include) {
            if ($name -like $pat) { $ok = $true; break }
        }
        if (-not $ok) { return }
        foreach ($rx in $ExcludeNameRegex) {
            if ($name -match $rx) { return }
        }
        # Skip editor/source clutter.
        if ($name -match '\.(blend|blend\d*|psd|spp|hip|ma|mb|max|unity|meta|DS_Store)$') { return }
        if ($name -eq 'Thumbs.db') { return }
        $destRel = Join-Path $RelativeDestDir $rel
        if (Copy-SelectedFile -RelativeSource (Join-Path $RelativeSourceDir $rel) `
                -RelativeDest $destRel -Category $Category -Reason $Reason -Consumer $Consumer) {
            $count++
        }
    }
    return $count
}

Write-Host "Source (read-only): $SourceRoot"
Write-Host "Staging:            $staging"
Write-Host "Destination:        $destRoot"

if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "Source root does not exist: $SourceRoot"
}

if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
$null = New-Item -ItemType Directory -Force -Path $staging
'' | Set-Content -LiteralPath $provenancePath -Encoding utf8

$copied = 0

# --- Nature: compact tree/rock/plant GLBs ---
# DanglingBat is Class 2: license permits use in any game (including this free demo).
# It forbids marketplace resale of the assets on their own / as a standalone asset pack.
# PsxPack is engine/demo content packaging, not that kind of resale.
$treeModels = @(
    'pine_tree_01_a.glb', 'pine_tree_01_b.glb', 'pine_tree_01_c.glb',
    'aspen_tree_01.glb', 'aspen_tree_02.glb',
    'fallen_pine_tree_01.glb', 'dead_branch_01.glb', 'dead_branch_02.glb'
)
foreach ($m in $treeModels) {
    if (Copy-SelectedFile -RelativeSource "Nature\psx-trees\psx-trees_v1.0 - Copy\models\$m" `
            -RelativeDest "Nature\Trees\$m" -Category 'model/tree' `
            -Reason 'ForestRuins trees (Class 2 DanglingBat game/demo use)') { $copied++ }
}
foreach ($m in @('rock_01_small.glb', 'rock_01_small_mossy.glb', 'rock_01_large.glb', 'rock_01_large_mossy.glb')) {
    if (Copy-SelectedFile -RelativeSource "Nature\psx-trees\psx-trees_v1.0 - Copy\models\$m" `
            -RelativeDest "Nature\Rocks\$m" -Category 'model/rock' `
            -Reason 'ForestRuins rocks (Class 2 DanglingBat game/demo use)') { $copied++ }
}
$plantModels = @(
    'bracken_01.glb', 'bracken_02.glb', 'bracken_03.glb',
    'grass_01_large.glb', 'grass_01_medium.glb', 'grass_01_small.glb',
    'tall_grass_01_large.glb', 'tall_grass_01_medium.glb',
    'old-field_goldenrod_01.glb', 'old-field_goldenrod_02.glb',
    'savory-leaf_aster_01.glb', 'savory-leaf_aster_02.glb',
    'dogbane_01.glb', 'rhus_typhina_small_01.glb'
)
foreach ($m in $plantModels) {
    if (Copy-SelectedFile -RelativeSource "Nature\psx-plants\psx-plants_v1.0\models\$m" `
            -RelativeDest "Nature\Plants\$m" -Category 'model/plant' `
            -Reason 'ForestRuins plants (Class 2 DanglingBat game/demo use)') { $copied++ }
}

# Forest ground textures (PNG only)
$forestRoot = 'Nature\PSX_Forest_asset\PSX_Forest_Environment'
foreach ($tex in @('forestshortgrass.png', 'forestwildground.png', 'forestearthground.png')) {
    $srcCandidate = Join-Path $SourceRoot (Join-Path $forestRoot $tex)
    if (Test-Path -LiteralPath $srcCandidate) {
        if (Copy-SelectedFile -RelativeSource (Join-Path $forestRoot $tex) `
                -RelativeDest "Nature\Forest\$tex" -Category 'texture/ground' `
                -Reason 'ForestRuins groundDiffuse PreferExistingPath') { $copied++ }
    }
}
# Also search recursively for those names under Nature/PSX_Forest_asset
Get-ChildItem -LiteralPath (Join-Path $SourceRoot 'Nature\PSX_Forest_asset') -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -in @('forestshortgrass.png', 'forestwildground.png', 'forestearthground.png') } |
    ForEach-Object {
        $rel = $_.FullName.Substring($SourceRoot.Length).TrimStart('\')
        $destName = $_.Name
        if (-not (Test-Path (Join-Path $staging "Nature\Forest\$destName"))) {
            if (Copy-SelectedFile -RelativeSource $rel -RelativeDest "Nature\Forest\$destName" `
                    -Category 'texture/ground' -Reason 'ForestRuins groundDiffuse') { $copied++ }
        }
    }

# Loose textures used by PreferExistingPath
foreach ($pair in @(
        @{ Src = 'Textures\grass_2.png'; Dst = 'Textures\grass_2.png'; Why = 'groundDiffuse fallback' },
        @{ Src = 'Textures\pine_bark_1.png'; Dst = 'Textures\pine_bark_1.png'; Why = 'barkDiffuse' },
        @{ Src = 'Textures\pine_bark_2.png'; Dst = 'Textures\pine_bark_2.png'; Why = 'barkDiffuse alt' },
        @{ Src = 'Textures\tree_bark_5.png'; Dst = 'Textures\tree_bark_5.png'; Why = 'barkDiffuse fallback' }
    )) {
    if (Copy-SelectedFile -RelativeSource $pair.Src -RelativeDest $pair.Dst `
            -Category 'texture/loose' -Reason $pair.Why) { $copied++ }
}

# Skies — one equirect PNG (runtime picks a single sky texture)
if (Copy-SelectedFile -RelativeSource 'Textures\Skyboxes\skybox_emx_1.png' `
        -RelativeDest 'Skies\skybox_emx_1.png' -Category 'texture/sky' `
        -Reason 'ForestRuinsExperience PickSkiesEquirectRelative') { $copied++ }

# LRT tiles actually referenced by ForestRuinsWorld (RT_* names only)
$lrtTiles = @(
    'RT_coarse_dirt', 'RT_mossy_cobblestone', 'RT_cobblestone', 'RT_mossy_stone_bricks',
    'RT_cobbled_deepslate', 'RT_smooth_stone', 'RT_moss_block', 'RT_oak_leaves',
    'RT_oak_log', 'RT_gold_block', 'RT_raw_copper_block', 'RT_prismarine_bricks', 'RT_deepslate_tiles'
)
$lrtSrc = 'Textures\LRT - Texture Pack - RT28.8 - 128x\tile'
foreach ($base in $lrtTiles) {
    foreach ($suffix in @('.png', '_n.png', '_s.png')) {
        $name = $base + $suffix
        # oak_leaves has no _n/_s in source — skip missing quietly
        $src = Join-Path $SourceRoot (Join-Path $lrtSrc $name)
        if (Test-Path -LiteralPath $src) {
            if (Copy-SelectedFile -RelativeSource (Join-Path $lrtSrc $name) `
                    -RelativeDest "LRT\tile\$name" -Category 'texture/lrt' `
                    -Reason 'ForestRuins ApplyForestRuinsShowcaseMaterials') { $copied++ }
        }
    }
}

# License copies (into pack, not modifying source)
foreach ($lic in @(
        @{ Src = 'Textures\Game Asset License Agreement.pdf'; Dst = 'licenses\Textures-Game-Asset-License-Agreement.pdf' },
        @{ Src = 'Nature\psx-trees\psx-trees_v1.0 - Copy\DanglingBat-License-Agreement.rtf'; Dst = 'licenses\DanglingBat-psx-trees-License.rtf' },
        @{ Src = 'Nature\psx-plants\psx-plants_v1.0\DanglingBat-License-Agreement.rtf'; Dst = 'licenses\DanglingBat-psx-plants-License.rtf' },
        @{ Src = '_Receipt\README.txt'; Dst = 'licenses\Bundle-Receipt-README.txt' }
    )) {
    if (Copy-SelectedFile -RelativeSource $lic.Src -RelativeDest $lic.Dst `
            -Category 'license' -Reason 'provenance / redistribution review' -Consumer 'packaging') { $copied++ }
}

# World landmarks — FBX + Textures only (no Characters, no blend/dae)
function Copy-LandmarkPack {
    param([string]$EnvName, [string]$DestName)
    $srcBase = Join-Path 'Environments' $EnvName
    $srcFull = Join-Path $SourceRoot $srcBase
    if (-not (Test-Path $srcFull)) {
        Write-Warning "Landmark missing: $EnvName"
        return
    }
    Get-ChildItem -LiteralPath $srcFull -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($srcFull.Length).TrimStart('\')
        $norm = $rel -replace '/', '\'
        $allow = $false
        if ($EnvName -eq 'Industrial_exterior_v1') {
            if ($norm -match '^Industrial_exterior_v2\\(Models|Textures)\\') { $allow = $true }
        } else {
            if ($norm -match '^(Models|Textures)\\') { $allow = $true }
        }
        if (-not $allow) { return }
        if ($_.Extension -match '\.(blend\d*|blend|dae|glb|gltf|obj|unity|meta|psd)$') { return }
        if ($norm -match '\\Skybox\\') { return }
        $destRel = Join-Path "World\$DestName" $rel
        if (Copy-SelectedFile -RelativeSource (Join-Path $srcBase $rel) `
                -RelativeDest $destRel -Category 'model/landmark' `
                -Reason "ForestRuins hero landmark $DestName") {
            $script:copied = $script:copied + 1
        }
    }
}

Copy-LandmarkPack -EnvName 'Abandoned_House' -DestName 'Abandoned_House'
Copy-LandmarkPack -EnvName 'Gas_station' -DestName 'Gas_station'
Copy-LandmarkPack -EnvName 'Trailer_Park' -DestName 'Trailer_Park'
Copy-LandmarkPack -EnvName 'DINER' -DestName 'DINER'
Copy-LandmarkPack -EnvName '6twelve' -DestName 'SixTwelve'
Copy-LandmarkPack -EnvName 'Industrial_exterior_v1' -DestName 'IndustrialHorror'

# Scatter props (small MS_* packs)
$props = @(
    @{ Env = 'Campfire'; Dest = 'Campfire'; File = 'MS_Campfire.fbx' },
    @{ Env = 'Firepot'; Dest = 'Firepot'; File = 'MS_Firepot.fbx' },
    @{ Env = 'Sawbuck'; Dest = 'Sawbuck'; File = 'MS_Sawbuck.fbx' },
    @{ Env = 'Planter_Box'; Dest = 'Planter'; File = 'MS_Planter_Box.fbx' },
    @{ Env = 'Board_Message'; Dest = 'Board_Message'; File = 'MS_Board_Message.fbx' },
    @{ Env = 'Totem_Welcome'; Dest = 'Totem'; File = 'MS_Totem_Welcome.fbx' }
)
foreach ($p in $props) {
    $envDir = Join-Path 'Environments' $p.Env
    # Copy FBX + any sibling textures/images in that folder
    Get-ChildItem -LiteralPath (Join-Path $SourceRoot $envDir) -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -match '\.(fbx|png|jpg|jpeg|tga)$' } |
        ForEach-Object {
            if (Copy-SelectedFile -RelativeSource (Join-Path $envDir $_.Name) `
                    -RelativeDest (Join-Path "World\$($p.Dest)" $_.Name) `
                    -Category 'model/prop' -Reason 'ForestRuins World prop scatter') { $copied++ }
        }
}

# Package docs (ASCII punctuation for portable UTF-8 without BOM)
$readme = @'
# Wilderness Ruins - PsxPack

Runtime content package for ForestRuins / Wilderness Ruins (engine free demo).
Produced by `Tools/PsxPack/Assemble-PsxPack.ps1`.

## Rules

- Game code loads assets from this pack only (not from any external purchase folder).
- See `LICENSING.md` and `CLASSIFICATION.md`.

## Layout

| Folder | Contents |
|--------|----------|
| `World/` | Hero landmarks + small scatter props |
| `Nature/Trees` | Tree GLBs |
| `Nature/Rocks` | Rock GLBs |
| `Nature/Plants` | Bush/grass/wildflower GLBs |
| `Nature/Forest` | Ground albedo textures |
| `Skies/` | Equirect skybox |
| `Textures/` | Loose bark/grass fallbacks |
| `LRT/tile/` | Curated LRT `RT_*` tiles |
| `licenses/` | License/receipt copies bundled with the pack |
| `PROVENANCE.jsonl` | Per-file pack checksums |

'@
Write-Utf8File (Join-Path $staging 'README.md') $readme

# LICENSING.md / CLASSIFICATION.md are maintained in-repo; keep assembly copies aligned.
$licensingPath = Join-Path $RepoRoot 'Games\WildernessRuins\assets\PsxPack\LICENSING.md'
$classificationPath = Join-Path $RepoRoot 'Games\WildernessRuins\assets\PsxPack\CLASSIFICATION.md'
if (Test-Path -LiteralPath $licensingPath) {
    Copy-Item -LiteralPath $licensingPath -Destination (Join-Path $staging 'LICENSING.md') -Force
} else {
    Write-Utf8File (Join-Path $staging 'LICENSING.md') "# See Tools/PsxPack and repo LICENSING.md`n"
}
if (Test-Path -LiteralPath $classificationPath) {
    Copy-Item -LiteralPath $classificationPath -Destination (Join-Path $staging 'CLASSIFICATION.md') -Force
}

$licensesIndex = @'
# License documents in this pack

Gitignored with the binary pack payload.

| File | Role |
|------|------|
| Textures-Game-Asset-License-Agreement.pdf | Pizza Doggy / Textures catalog license |
| DanglingBat-psx-trees-License.rtf | DanglingBat trees/rocks |
| DanglingBat-psx-plants-License.rtf | DanglingBat plants |
| Bundle-Receipt-README.txt | Purchase receipt note |
'@
$null = New-Item -ItemType Directory -Force -Path (Join-Path $staging 'licenses')
Write-Utf8File (Join-Path $staging 'licenses\INDEX.md') $licensesIndex

$files = Get-ChildItem -LiteralPath $staging -Recurse -File
$totalBytes = ($files | Measure-Object Length -Sum).Sum
$summary = [ordered]@{
    assembledUtc = (Get-Date).ToUniversalTime().ToString('o')
    destination  = 'Games/WildernessRuins/assets/PsxPack'
    fileCount    = $files.Count
    totalBytes   = $totalBytes
    totalMiB     = [math]::Round($totalBytes / 1MB, 2)
    notes        = @(
        'Byte-identical copies only; purchase root is never written and never named here.',
        'Excluded: unused bulk, full LRT tile set, landmark Characters/, blend/dae/obj duplicates.',
        'Material pad uses RT_* tile names (no invented rt2_* aliases).'
    )
}
Write-Utf8File $manifestPath (($summary | ConvertTo-Json -Depth 4) + "`n")

Write-Host ("Staged files={0} size={1:N2} MiB" -f $files.Count, ($totalBytes / 1MB))

# Post-copy integrity: every provenance row must still match the source file hash.
$mismatches = 0
Get-Content -LiteralPath $provenancePath | Where-Object { $_.Trim().Length -gt 0 } | ForEach-Object {
    $row = $_ | ConvertFrom-Json
    $srcRel = if ($row.catalogRel) { $row.catalogRel } else { $row.source }
    if ([string]::IsNullOrWhiteSpace($srcRel)) { return }
    # Strip any legacy root prefix if present; never store purchase-folder names going forward.
    $srcRel = $srcRel -replace '^(psx official|retro official)/', ''
    $srcPath = Join-Path $SourceRoot ($srcRel -replace '/', '\')
    if (-not (Test-Path -LiteralPath $srcPath -PathType Leaf)) {
        Write-Warning "Source missing during post-check: $srcRel"
        $mismatches++
        return
    }
    $live = (Get-FileHash -LiteralPath $srcPath -Algorithm SHA256).Hash
    if ($live -ne $row.sourceSha256) {
        Write-Warning "Source hash changed during/after assembly: $srcRel"
        $mismatches++
    }
}
if ($mismatches -gt 0) {
    throw "Source integrity check failed ($mismatches mismatches). Aborting install."
}

# Swap into destination (never under source)
$destFull = [System.IO.Path]::GetFullPath($destRoot)
if ($destFull.StartsWith($SourceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Destination resolves inside licensed source root"
}
$backup = Join-Path $RepoRoot 'Saved\Temp\PsxPack.previous'
if (Test-Path -LiteralPath $backup) {
    Remove-Item -LiteralPath $backup -Recurse -Force
}
if (Test-Path -LiteralPath $destRoot) {
    Move-Item -LiteralPath $destRoot -Destination $backup
}
Move-Item -LiteralPath $staging -Destination $destRoot
Write-Host "Installed PsxPack -> $destRoot"
if (-not $KeepStaging -and (Test-Path $backup)) {
    Write-Host "Previous pack kept at $backup (delete manually if desired)"
}

Write-Host 'Done. Source SHA256 values matched provenance for all copied files at install time.'
