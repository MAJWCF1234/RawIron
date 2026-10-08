param(
    [string]$BuildDirectory='build/dev-msvc',
    [string]$OutputRoot='Saved/visual_checks/mesh-features'
)
$ErrorActionPreference='Stop'
$featureWorkspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$featureExecutable=Join-Path $featureWorkspace "$BuildDirectory/Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe"
$featureOutput=Join-Path (Join-Path $featureWorkspace $OutputRoot) (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $featureOutput -Force | Out-Null
$featureRows=@()
foreach($featureRoom in @('vertex-colors','uv-transform','morph-targets','clipping')) {
    foreach($featureFrame in @(0,16)) {
        $featureImage=Join-Path $featureOutput "$featureRoom-$featureFrame.bmp"
        $featureLog=Join-Path $featureOutput "$featureRoom-$featureFrame.log"
        & $featureExecutable "--workspace-root=$featureWorkspace" "--start-room=$featureRoom" '--offline' '--background' '--no-hybrid-hdr' "--feature-frame=$featureFrame" "--capture-native=$featureImage" > $featureLog 2>&1
        if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $featureImage)) {throw "Native capture failed: $featureRoom/$featureFrame; $featureLog"}
        $featureRows+=[pscustomobject]@{Room=$featureRoom;Frame=$featureFrame;Image=$featureImage;ImageSha256=(Get-FileHash -LiteralPath $featureImage -Algorithm SHA256).Hash;Log=$featureLog}
    }
}
[pscustomobject]@{Executable=$featureExecutable;ExecutableSha256=(Get-FileHash -LiteralPath $featureExecutable -Algorithm SHA256).Hash;Captures=$featureRows;Scope='Native direct Vulkan captures; visual superiority, full example parity and headset comfort require additional review.'} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $featureOutput 'report.json') -Encoding utf8
Write-Output $featureOutput
