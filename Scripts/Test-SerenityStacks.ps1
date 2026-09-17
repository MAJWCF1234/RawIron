param([string]$BuildDirectory = 'build/dev-msvc')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = [IO.Path]::GetFullPath((Join-Path $workspace $BuildDirectory))
$game = Join-Path $buildRoot 'Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
if (-not (Test-Path -LiteralPath $game)) { throw "Build Cube Test first: $game" }
$evidence = Join-Path $workspace ('Saved/visual_checks/serenity/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
Add-Type -AssemblyName System.Drawing
$captures = @()
function Assert-NoValidationErrors([string]$LogPath) {
    if (Select-String -LiteralPath $LogPath -Pattern 'Validation Error|SYNC-HAZARD|VUID-' -Quiet) {
        throw "Vulkan validation reported a problem: $LogPath"
    }
}
foreach ($stack in @('base','serenity','soft-film')) {
    $capture = Join-Path $evidence "$stack.bmp"
    $log = Join-Path $evidence "$stack.log"
    & $game "--workspace-root=$workspace" --material-calibration --background --width=640 --height=360 "--processing-stack=$stack" "--capture-native=$capture" *> $log
    if ($LASTEXITCODE -ne 0) { throw "Stack $stack capture failed; see $log" }
    Assert-NoValidationErrors $log
    if (-not (Select-String -LiteralPath $log -SimpleMatch "Processing stack active: $stack" -Quiet)) { throw "Stack $stack was not activated" }
    $bitmap = [Drawing.Bitmap]::new($capture)
    try {
        $minimum=255; $maximum=0; $sum=0.0; $count=0
        for ($y=0; $y -lt $bitmap.Height; $y+=8) { for ($x=0; $x -lt $bitmap.Width; $x+=8) {
            $pixel=$bitmap.GetPixel($x,$y)
            $luma=([int]$pixel.R+[int]$pixel.G+[int]$pixel.B)/3.0
            $minimum=[Math]::Min($minimum,$luma); $maximum=[Math]::Max($maximum,$luma); $sum+=$luma; $count++
        } }
        if ($maximum-$minimum -lt 40 -or $sum/$count -lt 5 -or $sum/$count -gt 250) { throw "Stack $stack produced a blank/clipped frame" }
        $captures += [ordered]@{stack=$stack; image=$capture; sha256=(Get-FileHash -LiteralPath $capture).Hash; mean=$sum/$count}
    } finally { $bitmap.Dispose() }
}
if (@($captures | ForEach-Object { $_.sha256 } | Select-Object -Unique).Count -ne 3) { throw 'Stack selection did not change all three captured outputs' }
$cycleLog = Join-Path $evidence 'h-cycle.log'
& $game "--workspace-root=$workspace" --background --width=320 --height=200 --benchmark-frames=12 --verify-processing-stack-cycle *> $cycleLog
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $cycleLog -SimpleMatch 'Processing stack H-cycle verified' -Quiet)) {
    throw "Cube Test H-cycle verification failed; see $cycleLog"
}
Assert-NoValidationErrors $cycleLog
# Real multi-frame native rendering: camera movement, scene identity change and
# returning to Serenity after base. Layer errors must fail even if the app exits 0.
$probe = Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/RelWithDebInfo/VulkanNativeValidationProbe.exe'
$probeLog = Join-Path $evidence 'temporal-probe.log'
& $probe (Join-Path $evidence 'temporal-probe.bmp') --serenity $workspace *> $probeLog
if ($LASTEXITCODE -ne 0) { throw "Temporal probe failed: $probeLog" }
Assert-NoValidationErrors $probeLog
if (-not (Select-String -LiteralPath $probeLog -SimpleMatch 'Processing stack active: serenity' -Quiet)) {
    throw 'Rebuild VulkanNativeValidationProbe: Serenity scenario was not activated'
}
$meterProbe = Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/RelWithDebInfo/VulkanSerenityExposureProbe.exe'
$meterLog = Join-Path $evidence 'exposure-meter.log'
& $meterProbe (Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/shaders') *> $meterLog
if ($LASTEXITCODE -ne 0) { throw "GPU exposure math failed: $meterLog" }
Assert-NoValidationErrors $meterLog

# Disposable creator stack tests exercise shader output, not just config parsing.
$fixtureName = 'serenity-probe-' + [Guid]::NewGuid().ToString('N')
$library = [IO.Path]::GetFullPath((Join-Path $workspace 'Config/ProcessingStacks'))
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $library $fixtureName))
if ([IO.Path]::GetDirectoryName($fixtureRoot) -ne $library) { throw 'Invalid test stack directory' }
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$manualMeans = @{}
$manualHashes = @{}
try {
    foreach ($fixture in @(
        @{name='manual-low'; exposure='0.25'; multiplier='1'},
        @{name='manual-high'; exposure='2'; multiplier='1'},
        @{name='manual-multiplier'; exposure='2'; multiplier='32'})) {
        @"
version=1
backend=serenity-color
AUTO_EXPOSURE=false
Manual_exposure_value=$($fixture.exposure)
EXPOSURE_MULTIPLIER=$($fixture.multiplier)
Fake_purkinje=false
TAA=false
VOLUMETRIC_CLOUDS=false
TOGGLE_VL_FOG=false
BLOOM_STRENGTH=0
BLOOMY_FOG=0
WATER_REFRACTION=0
DITHER=false
"@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'stack.cfg') -Encoding utf8
        $fixtureCapture = Join-Path $evidence ($fixture.name + '.bmp')
        $fixtureLog = Join-Path $evidence ($fixture.name + '.log')
        & $game "--workspace-root=$workspace" --material-calibration --background --width=320 --height=200 "--processing-stack=$fixtureName" "--capture-native=$fixtureCapture" *> $fixtureLog
        if ($LASTEXITCODE -ne 0) { throw "Manual exposure capture failed: $fixtureLog" }
        Assert-NoValidationErrors $fixtureLog
        $bitmap = [Drawing.Bitmap]::new($fixtureCapture)
        try {
            $total = 0.0; $pixels = 0
            for ($y=0; $y -lt $bitmap.Height; $y+=4) { for ($x=0; $x -lt $bitmap.Width; $x+=4) {
                $pixel = $bitmap.GetPixel($x,$y)
                $total += ([int]$pixel.R + [int]$pixel.G + [int]$pixel.B)/3.0; $pixels++
            } }
            $manualMeans[$fixture.name] = $total / $pixels
            $manualHashes[$fixture.name] = (Get-FileHash -LiteralPath $fixtureCapture).Hash
        } finally { $bitmap.Dispose() }
    }
} finally {
    # Literal verified immediate child created by this test; never delete the library.
    if ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($fixtureRoot)) -eq $library) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
if ($manualMeans['manual-high'] -le $manualMeans['manual-low'] + 5) { throw 'Manual exposure did not brighten native output' }
# The calibration scene has raster/shadow-edge variation across processes. The
# isolated GPU probe above verifies exact shader behavior; allow <0.5/255 in this
# scene-level average rather than requiring identical files at every edge pixel.
if ([Math]::Abs($manualMeans['manual-high'] - $manualMeans['manual-multiplier']) -gt 0.5) {
    throw 'Manual exposure incorrectly uses the auto exposure multiplier'
}
[ordered]@{captures=$captures; hCyclePassed=$true; temporalProbePassed=$true; gpuExposureMathPassed=$true; manualExposureMeans=$manualMeans; manualMultiplierIgnored=$true; fullBlissAppearanceParity=$false} |
    ConvertTo-Json -Depth 6 | Set-Content (Join-Path $evidence 'report.json')
"Passed: native stack captures, H-cycle/repeat suppression, temporal transitions and manual exposure output. Evidence: $evidence"
