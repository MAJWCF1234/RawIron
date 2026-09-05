param(
    [string]$BuildDirectory = 'build/dev-msvc',
    [string]$OutputRoot = 'Saved/visual_checks/calibration',
    [switch]$IncludeGallery,
    [switch]$IncludeExtendedPost
)

$ErrorActionPreference = 'Stop'
if ($IncludeExtendedPost -and -not $IncludeGallery) { throw '-IncludeExtendedPost requires -IncludeGallery' }
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = [IO.Path]::GetFullPath((Join-Path $workspace $BuildDirectory))
$executable = Join-Path $buildRoot 'Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
$shaderRoot = Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/shaders'
$runRoot = Join-Path ([IO.Path]::GetFullPath((Join-Path $workspace $OutputRoot))) (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Build Cube Test first: $executable" }
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$captures = @()
for ($frame = 1; $frame -le 2; $frame++) {
    $imagePath = Join-Path $runRoot "frame-$frame.bmp"
    $logPath = Join-Path $runRoot "frame-$frame.log"
    $launchArguments = @("--workspace-root=$workspace", '--material-calibration', '--background',
        '--width=1280', '--height=720', "--capture-native=$imagePath")
    & $executable @launchArguments *> $logPath
    if ($LASTEXITCODE -ne 0) { throw "Native capture failed (exit $LASTEXITCODE); see $logPath" }
    if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf)) { throw "Native capture did not produce $imagePath" }
    $captures += [ordered]@{ image=$imagePath; log=$logPath; arguments=$launchArguments;
        sha256=(Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash }
}

Add-Type -AssemblyName System.Drawing
$checks = [Collections.Generic.List[object]]::new()
function Add-Check([string]$Name, [bool]$Passed, [string]$Detail) {
    $checks.Add([ordered]@{name=$Name; passed=$Passed; detail=$Detail})
}
function Get-LuminanceRange($Bitmap, [int]$X, [int]$Y, [int]$Width, [int]$Height) {
    $minimum = 255.0
    $maximum = 0.0
    for ($row = $Y; $row -lt ($Y + $Height); $row++) {
        for ($column = $X; $column -lt ($X + $Width); $column++) {
            $pixel = $Bitmap.GetPixel($column, $row)
            $luminance = ($pixel.R + $pixel.G + $pixel.B) / 3.0
            $minimum = [Math]::Min($minimum, $luminance)
            $maximum = [Math]::Max($maximum, $luminance)
        }
    }
    return [ordered]@{minimum=$minimum; maximum=$maximum}
}

$bitmap = [Drawing.Bitmap]::new($captures[0].image)
try {
    $correctSize = $bitmap.Width -eq 1280 -and $bitmap.Height -eq 720
    Add-Check 'dimensions' $correctSize "$($bitmap.Width)x$($bitmap.Height)"
    if ($correctSize) {
        # These interior ROIs belong to the fixed engine calibration camera. They exclude edges,
        # normal-map detail, and real cast shadows; they are not a broad image-quality score.
        $flat = Get-LuminanceRange $bitmap 713 304 77 37
        Add-Check 'flat receiver has no self-shadow pattern' (($flat.maximum - $flat.minimum) -le 4) ($flat | ConvertTo-Json -Compress)
        $floor = Get-LuminanceRange $bitmap 80 650 100 40
        Add-Check 'rough floor has no black non-finite region' ($floor.minimum -gt 40) ($floor | ConvertTo-Json -Compress)
        $red = $bitmap.GetPixel(750, 220)
        $green = $bitmap.GetPixel(870, 220)
        $blue = $bitmap.GetPixel(990, 220)
        Add-Check 'RGB channel order' ($red.R -gt 180 -and $red.G -lt 20 -and $red.B -lt 20 -and
            $green.G -gt 180 -and $green.R -lt 20 -and $green.B -lt 20 -and
            $blue.B -gt 180 -and $blue.R -lt 20 -and $blue.G -lt 20) 'unlit red/green/blue swatch interiors'
        $shadow = Get-LuminanceRange $bitmap 250 559 25 15
        Add-Check 'cast shadows remain enabled' ($shadow.minimum -lt ($floor.minimum * 0.6)) ($shadow | ConvertTo-Json -Compress)
    }
} finally { $bitmap.Dispose() }
Add-Check 'repeatable static GPU capture' ($captures[0].sha256 -eq $captures[1].sha256) 'two independent process launches'

$galleryCaptures = @()
if ($IncludeGallery) {
    $probe = Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/RelWithDebInfo/VulkanNativeValidationProbe.exe'
    if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) { throw "Build VulkanNativeValidationProbe first: $probe" }
    # The extended shader can require substantial driver compilation time/memory.
    # Keep it explicit instead of making ordinary gallery validation pay that cost.
    $modes = @('direct', 'hybrid')
    if ($IncludeExtendedPost) { $modes += 'extended' }
    foreach ($mode in $modes) {
        $imagePath = Join-Path $runRoot "luma-$mode.bmp"
        $logPath = Join-Path $runRoot "luma-$mode.log"
        $launchArguments = @($imagePath, '--luma-curve')
        if ($mode -ne 'direct') { $launchArguments += '--hybrid-hdr' }
        if ($mode -eq 'extended') { $launchArguments += '--extended-post' }
        & $probe @launchArguments *> $logPath
        if ($LASTEXITCODE -ne 0) { throw "Luma probe failed: $logPath" }
        $bitmap = [Drawing.Bitmap]::new($imagePath)
        try {
            $dark = Get-LuminanceRange $bitmap 151 95 16 10
            $control = $bitmap.GetPixel(205, 100)
            Add-Check "luma curve preserves black ($mode)" ($dark.maximum -le 2 -and $control.B -gt 20) `
                "black ROI max=$($dark.maximum); blue control=$($control.B)"
        } finally { $bitmap.Dispose() }
        $galleryCaptures += [ordered]@{mode=$mode; image=$imagePath; log=$logPath; arguments=$launchArguments;
            executable=$probe; executableSha256=(Get-FileHash $probe -Algorithm SHA256).Hash;
            sha256=(Get-FileHash $imagePath -Algorithm SHA256).Hash}
    }
    $imagePath = Join-Path $runRoot 'normal-room.bmp'
    $logPath = Join-Path $runRoot 'normal-room.log'
    $launchArguments = @("--workspace-root=$workspace", '--background', '--start-room=normals',
        '--width=1280', '--height=720', "--capture-native=$imagePath")
    & $executable @launchArguments *> $logPath
    if ($LASTEXITCODE -ne 0) { throw "Normal-room capture failed: $logPath" }
    $bitmap = [Drawing.Bitmap]::new($imagePath)
    try {
        $sky = $bitmap.GetPixel(100, 100)
        $skyChannels = @([int]$sky.R, [int]$sky.G, [int]$sky.B) | Measure-Object -Minimum -Maximum
        Add-Check 'normal-room sky is not enclosed by coffee' `
            ($skyChannels.Minimum -gt 30 -and $skyChannels.Maximum -lt 245 -and $skyChannels.Maximum-$skyChannels.Minimum -lt 40) `
            "RGB=$($sky.R),$($sky.G),$($sky.B)"
        $floor = Get-LuminanceRange $bitmap 1100 630 60 35
        Add-Check 'normal-room floor is lit without white clipping' ($floor.minimum -gt 30 -and $floor.maximum -lt 240) `
            ($floor | ConvertTo-Json -Compress)
    } finally { $bitmap.Dispose() }
    $galleryCaptures += [ordered]@{mode='normal-room direct'; image=$imagePath; log=$logPath; arguments=$launchArguments;
        sha256=(Get-FileHash $imagePath -Algorithm SHA256).Hash}
}

$passed = @($checks | Where-Object { -not $_.passed }).Count -eq 0
$report = [ordered]@{
    timestamp=(Get-Date).ToString('o'); passed=$passed; configuration='RelWithDebInfo'; renderer='direct native Vulkan';
    executable=$executable; executableSha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash;
    shaderBinaries=@(Get-ChildItem -LiteralPath $shaderRoot -Filter 'Native*.spv' | Sort-Object Name | ForEach-Object {
        [ordered]@{path=$_.FullName; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    });
    displayDrivers=@(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion);
    captures=$captures; galleryCaptures=$galleryCaptures; checks=$checks
}
$reportPath = Join-Path $runRoot 'report.json'
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding utf8
$checks | ForEach-Object { Write-Output "$($_.passed): $($_.name) — $($_.detail)" }
Write-Output "Evidence: $reportPath"
if (-not $passed) { exit 1 }
