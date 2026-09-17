param(
    [string]$BuildDirectory = 'build/dev-msvc',
    [string]$OutputRoot = 'Saved/visual_checks/structural-expansion'
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = [IO.Path]::GetFullPath((Join-Path $workspace $BuildDirectory))
$exe = Join-Path $build 'Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Build Cube Test first: $exe" }
$run = Join-Path (Join-Path $workspace $OutputRoot) (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $run -Force | Out-Null
Add-Type -AssemblyName System.Drawing
$captures = @()
foreach ($room in @('knots','helices','lathe-poles','extrusion','rounded','superellipsoids','hulls','heightfields')) {
    $bmp = Join-Path $run "$room.bmp"
    $log = Join-Path $run "$room.log"
    $stderr = Join-Path $run "$room.stderr.log"
    $arguments = @("--workspace-root=$workspace", "--start-room=$room", '--background', '--offline',
        '--no-hybrid-hdr', '--width=1280', '--height=720', "--capture-native=$bmp")
    $quoted = ($arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' '
    $process = Start-Process -FilePath $exe -ArgumentList $quoted -WorkingDirectory $workspace -WindowStyle Hidden `
        -RedirectStandardOutput $log -RedirectStandardError $stderr -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddMinutes(3)
        while (-not $process.WaitForExit(1000)) {
            if ([DateTime]::UtcNow -gt $deadline) { $process.Kill(); throw "Capture timeout: $room; see $log" }
        }
        if ($process.ExitCode -ne 0) { throw "Capture failed: $room exit=$($process.ExitCode); see $log and $stderr" }
    } finally { $process.Dispose() }
    if (-not (Test-Path -LiteralPath $bmp -PathType Leaf)) { throw "Missing capture: $bmp" }
    $bitmap = [Drawing.Bitmap]::new($bmp)
    try {
        if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720) { throw "Wrong capture dimensions: $bmp" }
        $png = Join-Path $run "$room.png"
        $bitmap.Save($png, [Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
    $captures += [ordered]@{room=$room; image=$png; nativeImage=$bmp; sha256=(Get-FileHash $bmp).Hash;
        log=$log; stderr=$stderr; arguments=$arguments}
    Write-Output "Captured $room; visual review still required"
}
[ordered]@{timestamp=(Get-Date).ToString('o'); renderer='direct native Vulkan'; size='1280x720';
    executable=$exe; executableSha256=(Get-FileHash $exe).Hash;
    drivers=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion);
    shaders=@(Get-ChildItem (Join-Path $build 'Source/RawIron.Render.Vulkan/shaders') -Filter 'Native*.spv' | ForEach-Object {
        [ordered]@{name=$_.Name; sha256=(Get-FileHash $_.FullName).Hash}});
    captures=$captures; scope='Eight native GPU room starts. Requires visual inspection; not upstream parity or headset certification.'
} | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $run 'report.json') -Encoding utf8
Write-Output "Evidence: $run"
