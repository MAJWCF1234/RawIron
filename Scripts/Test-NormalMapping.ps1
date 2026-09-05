param(
    [string]$BuildDirectory = 'build/dev-msvc',
    [string]$OutputRoot = 'Saved/visual_checks/normal-mapping',
    [switch]$IncludeDemo
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = [IO.Path]::GetFullPath((Join-Path $workspace $BuildDirectory))
$probe = Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/RelWithDebInfo/VulkanNativeValidationProbe.exe'
$runRoot = Join-Path ([IO.Path]::GetFullPath((Join-Path $workspace $OutputRoot))) (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
# Constant linear tangent-space normal; analytic controls use the same decoded vector.
$texture = Join-Path $runRoot 'normal.tga'
[IO.File]::WriteAllBytes($texture, [byte[]]@(0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,24,32,230,191,166))
Add-Type -AssemblyName System.Drawing
$checks = @()
foreach ($width in @(320,1280)) { foreach ($tier in @(0,2)) { foreach ($mode in @('standard','mirror-u','mirror-v','degenerate')) {
    $caseName = "$mode-$width-tier$tier"
    foreach ($variant in @('sampled','analytic')) {
        $capture = Join-Path $runRoot "$caseName-$variant.bmp"
        $launchArguments = @($capture, '--normal-frame', $mode, $texture)
        $launchArguments += $(if ($variant -eq 'analytic') { '--analytic' } else { '--sampled' })
        $launchArguments += @([string]$width,[string]$tier)
        & $probe @launchArguments *> (Join-Path $runRoot "$caseName-$variant.log")
        if ($LASTEXITCODE -ne 0) { throw "Normal probe failed: $mode-$variant" }
    }
    $sampled = [Drawing.Bitmap]::new((Join-Path $runRoot "$caseName-sampled.bmp"))
    $analytic = [Drawing.Bitmap]::new((Join-Path $runRoot "$caseName-analytic.bmp"))
    try {
        $difference = 0.0
        $control = 0.0
        for ($y=$width*5/16-15; $y -lt $width*5/16+15; $y++) { for ($x=$width/2-15; $x -lt $width/2+15; $x++) {
            $a=$sampled.GetPixel($x,$y); $b=$analytic.GetPixel($x,$y)
            $difference += [Math]::Abs([int]$a.R-[int]$b.R)+[Math]::Abs([int]$a.G-[int]$b.G)+[Math]::Abs([int]$a.B-[int]$b.B)
            $control += [int]$b.R
        } }
        $difference /= 2700
        $control /= 900
        $checks += [ordered]@{ mode=$mode; width=$width; tier=$tier; meanAbsoluteError=$difference; controlMean=$control; passed=($difference -le 2 -and $control -gt 20 -and $control -lt 235) }
    } finally { $sampled.Dispose(); $analytic.Dispose() }
} } }
$demo = $null
if ($IncludeDemo) {
    $game = Join-Path $buildRoot 'Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
    $capture = Join-Path $runRoot 'comparison.bmp'
    $launchArguments = @("--workspace-root=$workspace", '--normal-comparison', '--background',
        '--width=1280', '--height=720', "--capture-native=$capture")
    & $game @launchArguments *> (Join-Path $runRoot 'comparison.log')
    if ($LASTEXITCODE -ne 0) { throw 'Native comparison demo failed' }
    $bitmap = [Drawing.Bitmap]::new($capture)
    try {
        $bitmap.Save((Join-Path $runRoot 'comparison.png'),[Drawing.Imaging.ImageFormat]::Png)
        foreach ($centerY in @(216,504)) {
            $convertedError=0.0; $wrongError=0.0
            # Matched interior bump regions exclude the different text embedded in each source map.
            for ($dy=-45; $dy -lt 45; $dy++) { for ($dx=-45; $dx -lt 45; $dx++) {
                $a=$bitmap.GetPixel(352+$dx,$centerY+$dy)
                $b=$bitmap.GetPixel(640+$dx,$centerY+$dy)
                $c=$bitmap.GetPixel(928+$dx,$centerY+$dy)
                $convertedError += [Math]::Abs([int]$a.R-[int]$b.R)
                $wrongError += [Math]::Abs([int]$a.R-[int]$c.R)
            } }
            $convertedError/=8100; $wrongError/=8100
            $top = [int]($bitmap.GetPixel(352,$centerY-50).R)
            $bottom = [int]($bitmap.GetPixel(352,$centerY+50).R)
            $checks += [ordered]@{mode="reference-row-$centerY"; width=1280; tier=1;
                meanAbsoluteError=$convertedError; wrongConventionError=$wrongError; extrusionTop=$top; extrusionBottom=$bottom;
                passed=($convertedError -le 2 -and $wrongError -ge 5 -and $top -gt $bottom+5)}
        }
        $demo=[ordered]@{executable=$game; executableSha256=(Get-FileHash $game).Hash;
            arguments=$launchArguments; image=$capture; imageSha256=(Get-FileHash $capture).Hash}
    } finally { $bitmap.Dispose() }
}
$passed = @($checks | Where-Object { -not $_.passed }).Count -eq 0
$report = [ordered]@{
    passed=$passed; timestamp=(Get-Date).ToString('o'); checks=$checks; demo=$demo
    executable=$probe; executableSha256=(Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash
    fragmentShaderSha256=(Get-FileHash (Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/shaders/NativeScenePreview.frag.spv')).Hash
    vertexShaderSha256=(Get-FileHash (Join-Path $buildRoot 'Source/RawIron.Render.Vulkan/shaders/NativeScenePreview.vert.spv')).Hash
    displayDrivers=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runRoot 'report.json') -Encoding utf8
$checks | ForEach-Object { Write-Output "$($_.passed): $($_.mode) $($_.width) tier$($_.tier) MAE=$($_.meanAbsoluteError)" }
Write-Output "Evidence: $runRoot"
if (-not $passed) { exit 1 }
