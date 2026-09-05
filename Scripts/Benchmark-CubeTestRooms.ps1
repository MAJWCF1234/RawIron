param(
    [string]$BuildDirectory = 'build/dev-msvc',
    [string]$OutputRoot = 'Saved/benchmarks/cube-test',
    [ValidateRange(1,10)][int]$Repetitions = 2,
    [ValidateRange(1,10000)][int]$WarmupIntervals = 30,
    [ValidateRange(10,10000)][int]$SampleIntervals = 120
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = [IO.Path]::GetFullPath((Join-Path $workspace $BuildDirectory))
$exe = Join-Path $build 'Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Build CubeTest first: $exe" }
$run = Join-Path (Join-Path $workspace $OutputRoot) (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $run -Force | Out-Null
$rows = @()
$frames = $WarmupIntervals + $SampleIntervals + 1
foreach ($room in @('baseline','sprites','normals','exporter','interaction','projectile','teleport','lathe','tubes','surfaces')) {
    for ($repeat=1; $repeat -le $Repetitions; ++$repeat) {
        $csv = Join-Path $run "$room-$repeat.csv"
        $log = Join-Path $run "$room-$repeat.log"
        $stderr = Join-Path $run "$room-$repeat.stderr.log"
        $arguments = @("--workspace-root=$workspace", "--start-room=$room", '--background', '--offline',
            '--width=1280', '--height=720', "--benchmark-frames=$frames", "--frame-times=$csv")
        $quoted = ($arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' '
        $process = Start-Process -FilePath $exe -ArgumentList $quoted -WorkingDirectory $workspace -WindowStyle Hidden `
            -RedirectStandardOutput $log -RedirectStandardError $stderr -PassThru
        try {
            if (-not $process.WaitForExit(120000)) { $process.Kill(); throw "Benchmark timeout: $room/$repeat; see $log" }
            if ($process.ExitCode -ne 0) { throw "Benchmark failed: $room/$repeat exit=$($process.ExitCode); see $log" }
        } finally { $process.Dispose() }
        $samples = @(Import-Csv -LiteralPath $csv)
        if ($samples.Count -ne ($frames-1)) { throw "Expected $($frames-1) present intervals, got $($samples.Count): $csv" }
        $ms = @($samples | Select-Object -Skip $WarmupIntervals | ForEach-Object {
            $value = [double]::Parse($_.cpu_present_ms, [Globalization.CultureInfo]::InvariantCulture)
            if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) { throw "Invalid interval: $csv" }
            $value
        })
        $sorted = @($ms | Sort-Object)
        $mean = ($ms | Measure-Object -Average).Average
        $rows += [ordered]@{room=$room; repetition=$repeat; samples=$ms.Count; meanMs=$mean;
            p50Ms=$sorted[[int][Math]::Ceiling($ms.Count*.5)-1]; p95Ms=$sorted[[int][Math]::Ceiling($ms.Count*.95)-1];
            p99Ms=$sorted[[int][Math]::Ceiling($ms.Count*.99)-1]; maxMs=$sorted[-1]; cadenceFps=1000/$mean;
            csv=$csv; csvSha256=(Get-FileHash $csv).Hash; log=$log; stderr=$stderr; arguments=$arguments}
        Write-Output "$room run $repeat complete ($($ms.Count) measured intervals)"
    }
}
$report = [ordered]@{timestamp=(Get-Date).ToString('o'); metric='CPU wall time between successful Vulkan presents; NOT GPU execution time';
    renderer='direct native Vulkan'; size='1280x720'; hidden=$true; warmupIntervals=$WarmupIntervals;
    sampleIntervals=$SampleIntervals; repetitions=$Repetitions; executable=$exe; executableSha256=(Get-FileHash $exe).Hash;
    drivers=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion);
    shaders=@(Get-ChildItem (Join-Path $build 'Source/RawIron.Render.Vulkan/shaders') -Filter 'Native*.spv' | ForEach-Object {
        [ordered]@{name=$_.Name; sha256=(Get-FileHash $_.FullName).Hash}}); runs=$rows}
$report | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $run 'report.json') -Encoding utf8
$lines = @('# CubeTest room benchmark', '',
    'CPU present cadence, not GPU execution time or physical-display FPS. Hidden 1280x720 direct Vulkan; no readback.',
    "Warmup: $WarmupIntervals intervals. Measured: $SampleIntervals intervals per run. Renderer/present mode and texture diagnostics are in each log.",
    'Static room starts with normal animation/physics; this is not an interactive traversal or headset comfort benchmark.', '',
    '| Room | Run | Mean ms | P50 ms | P95 ms | P99 ms | Max ms | Cadence FPS |',
    '|---|---:|---:|---:|---:|---:|---:|---:|')
foreach ($row in $rows) {
    $lines += '| {0} | {1} | {2:F2} | {3:F2} | {4:F2} | {5:F2} | {6:F2} | {7:F1} |' -f `
        $row.room,$row.repetition,$row.meanMs,$row.p50Ms,$row.p95Ms,$row.p99Ms,$row.maxMs,$row.cadenceFps
}
$lines | Set-Content (Join-Path $run 'report.md') -Encoding utf8
Write-Output "Report: $(Join-Path $run 'report.md')"
