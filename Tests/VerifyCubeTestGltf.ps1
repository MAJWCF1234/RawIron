param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$Workspace,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Read-Glb([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    Require ($bytes.Length -ge 28) "GLB is truncated: $Path"
    Require ([BitConverter]::ToUInt32($bytes,0) -eq 0x46546c67 -and [BitConverter]::ToUInt32($bytes,4) -eq 2) "Invalid GLB header: $Path"
    Require ([BitConverter]::ToUInt32($bytes,8) -eq $bytes.Length) "Incorrect GLB byte length: $Path"
    $length = [BitConverter]::ToInt32($bytes,12)
    Require ($length -gt 0 -and ($length % 4) -eq 0 -and 20+$length+8 -le $bytes.Length -and
        [BitConverter]::ToUInt32($bytes,16) -eq 0x4e4f534a) "Invalid GLB JSON chunk: $Path"
    $json = [Text.Encoding]::UTF8.GetString($bytes,20,$length) | ConvertFrom-Json
    $binaryOffset = 20+$length+8
    Require ([BitConverter]::ToUInt32($bytes,20+$length+4) -eq 0x004e4942 -and
        [BitConverter]::ToUInt32($bytes,20+$length)+$binaryOffset -eq $bytes.Length) "Invalid GLB BIN chunk: $Path"
    return [pscustomobject]@{json=$json; bytes=$bytes; binaryOffset=$binaryOffset; path=$Path}
}
function Local-Dependency([string]$Root, [string]$Uri) {
    Require (-not [string]::IsNullOrWhiteSpace($Uri) -and $Uri -notmatch '^[a-zA-Z]+:|^[/\\]|\\') "Nonportable dependency URI: $Uri"
    $resolved = [IO.Path]::GetFullPath((Join-Path $Root ([Uri]::UnescapeDataString($Uri))))
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    Require ($resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) "Dependency escapes export folder: $Uri"
    Require (Test-Path -LiteralPath $resolved -PathType Leaf) "Missing exported dependency: $Uri"
    return $resolved
}
function Meshes-Below($Document, [int]$Node) {
    $entry = $Document.nodes[$Node]
    if ($null -ne $entry.mesh) { $entry.mesh }
    foreach ($child in $entry.children) { Meshes-Below $Document $child }
}

$Workspace = [IO.Path]::GetFullPath($Workspace)
$Executable = [IO.Path]::GetFullPath($Executable)
$run = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $run -Force | Out-Null
$reference = Join-Path $Workspace 'Games/CubeTest/assets/reference/threejs-r185'
$sources = @((Read-Glb (Join-Path $reference 'models/gltf/ShaderBall.glb')),
    (Read-Glb (Join-Path $reference 'models/gltf/coffeemat.glb')))
$sourceRecords = @()
foreach ($source in $sources) {
    $document = $source.json
    Require ($document.extensionsUsed -contains 'KHR_mesh_quantization') 'Fixture no longer exercises quantization'
    $primitives = @($document.meshes | ForEach-Object { $_.primitives })
    Require (@($primitives | Where-Object { $document.accessors[$_.attributes.POSITION].componentType -ne 5126 }).Count -gt 0) 'No quantized POSITION accessor found'
    $compressed = @($document.bufferViews | Where-Object { $null -ne $_.extensions.EXT_meshopt_compression })
    if ($source.path.EndsWith('coffeemat.glb')) {
        Require ($document.extensionsUsed -contains 'EXT_meshopt_compression' -and $compressed.Count -gt 0) 'Coffee must contain actual meshopt buffer views'
        foreach ($view in $compressed) {
            $extension = $view.extensions.EXT_meshopt_compression
            Require ($extension.buffer -eq 0 -and $extension.count -gt 0 -and $extension.byteStride -gt 0 -and
                $extension.byteLength -gt 0 -and $extension.byteOffset+$extension.byteLength -le $document.buffers[0].byteLength) 'Compressed source range is invalid'
        }
    }
    $images = @()
    for ($i=0; $i -lt $document.images.Count; $i++) {
        $image = $document.images[$i]
        $view = $document.bufferViews[$image.bufferView]
        Require ($view.buffer -eq 0 -and $view.byteLength -gt 0 -and $source.binaryOffset+$view.byteOffset+$view.byteLength -le $source.bytes.Length) 'Embedded image range is invalid'
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash($source.bytes, ($source.binaryOffset+$view.byteOffset), $view.byteLength)).Replace('-','') }
        finally { $sha.Dispose() }
        $images += [ordered]@{index=$i; mimeType=$image.mimeType; byteOffset=$view.byteOffset; byteLength=$view.byteLength; sha256=$hash; status='unsupported embedded KTX2; explicit diagnostic/marker required'}
    }
    $sourceRecords += [ordered]@{path=$source.path; sha256=(Get-FileHash -LiteralPath $source.path -Algorithm SHA256).Hash;
        primitives=$primitives.Count; vertices=($primitives | ForEach-Object { $document.accessors[$_.attributes.POSITION].count } | Measure-Object -Sum).Sum;
        compressedViews=$compressed.Count; embeddedImages=$images}
}

$gltfPath = Join-Path $run 'gallery.gltf'
$logPath = Join-Path $run 'export.log'
& $Executable "--workspace-root=$Workspace" "--export-gltf=$gltfPath" *> $logPath
Require ($LASTEXITCODE -eq 0) "Cube Test export failed; see $logPath"
$exported = Get-Content -LiteralPath $gltfPath -Raw | ConvertFrom-Json
Require ($exported.asset.version -eq '2.0') 'Export must be glTF 2.0'
$buffers = @($exported.buffers | ForEach-Object {
    $file = Local-Dependency $run $_.uri
    Require ((Get-Item -LiteralPath $file).Length -eq $_.byteLength) 'Exported buffer byte count differs from JSON'
    [pscustomobject]@{bytes=[IO.File]::ReadAllBytes($file)}
})
foreach ($view in $exported.bufferViews) {
    Require ($view.buffer -ge 0 -and $view.buffer -lt $buffers.Count -and $view.byteOffset -ge 0 -and
        $view.byteLength -gt 0 -and $view.byteOffset+$view.byteLength -le $buffers[$view.buffer].bytes.Length) 'Exported bufferView is out of bounds'
}

# Independent byte-level checks, not a second call into Raw Iron/cgltf.
Add-Type -TypeDefinition @'
using System;
public static class CubeGltfBytes {
    public static void Check(byte[] data, int offset, int count, int width, int stride, bool floats, int vertexLimit) {
        if (count <= 0 || offset < 0 || stride < width*4 || (long)offset+(long)(count-1)*stride+width*4 > data.Length)
            throw new Exception("Invalid accessor byte range");
        for (int i=0;i<count;i++) for (int c=0;c<width;c++) {
            int at=offset+i*stride+c*4;
            if (floats) { float value=BitConverter.ToSingle(data,at); if (float.IsNaN(value)||float.IsInfinity(value)) throw new Exception("Non-finite geometry"); }
            else if (BitConverter.ToUInt32(data,at)>=vertexLimit) throw new Exception("Index outside vertex range");
        }
    }
}
'@
foreach ($mesh in $exported.meshes) {
    foreach ($primitive in $mesh.primitives) {
        $position = $exported.accessors[$primitive.attributes.POSITION]
        foreach ($pair in @(@($primitive.attributes.POSITION,3),@($primitive.attributes.NORMAL,3),@($primitive.attributes.TEXCOORD_0,2),@($primitive.indices,1))) {
            Require ($null -ne $pair[0]) 'Export must retain positions, normals, UVs and indices'
            $accessor = $exported.accessors[$pair[0]]
            $view = $exported.bufferViews[$accessor.bufferView]
            $floats = $pair[1] -ne 1
            Require ($accessor.componentType -eq $(if($floats){5126}else{5125})) 'Unexpected export accessor component type'
            $stride = $pair[1]*4
            if ($view.byteStride) { $stride = $view.byteStride }
            Require ($accessor.byteOffset + ($accessor.count-1)*$stride + $pair[1]*4 -le $view.byteLength) 'Accessor escapes its bufferView'
            [CubeGltfBytes]::Check($buffers[$view.buffer].bytes, ($view.byteOffset+$accessor.byteOffset), $accessor.count, $pair[1], $stride, $floats, $position.count)
            if ($floats) { Require ($accessor.count -eq $position.count) 'Export vertex streams differ in length' }
        }
    }
}
for ($s=0; $s -lt $sources.Count; $s++) {
    $wrapper = @('CubeTest_ThreeJsShaderBall','CubeTest_ThreeJsCompressedCoffee')[$s]
    $node = -1
    for ($i=0; $i -lt $exported.nodes.Count; $i++) { if ($exported.nodes[$i].name -eq $wrapper) { $node=$i; break } }
    Require ($node -ge 0) "Missing imported model wrapper: $wrapper"
    $meshIds = @(Meshes-Below $exported $node | Select-Object -Unique)
    $primitives = @($meshIds | ForEach-Object { $exported.meshes[$_].primitives })
    $vertices = ($primitives | ForEach-Object { $exported.accessors[$_.attributes.POSITION].count } | Measure-Object -Sum).Sum
    Require ($primitives.Count -eq $sourceRecords[$s].primitives -and $vertices -eq $sourceRecords[$s].vertices) "Compressed import lost geometry: $wrapper"
}

$surfaceCounts = [ordered]@{
    CubeTest_Procedural_Lathe20=5880; CubeTest_Procedural_Lathe96=28224; CubeTest_Procedural_LatheCutaway=18816
    CubeTest_Procedural_OpenTube=11640; CubeTest_Procedural_ClosedTube6=4608; CubeTest_Procedural_ClosedTube24=18432
    CubeTest_Procedural_Torus=9216; CubeTest_Procedural_Mobius=9216; CubeTest_Procedural_Saddle=9600
}
foreach ($name in $surfaceCounts.Keys) {
    $nodes = @($exported.nodes | Where-Object name -eq $name)
    Require ($nodes.Count -eq 1 -and $null -ne $nodes[0].mesh) "Missing structural surface: $name"
    $primitive = $exported.meshes[$nodes[0].mesh].primitives[0]
    Require ($exported.accessors[$primitive.indices].count -eq $surfaceCounts[$name]) "Structural surface topology changed: $name"
    Require ($exported.materials[$primitive.material].doubleSided) "Open surface must retain double-sided material: $name"
}

$manifest = Get-Content (Join-Path $reference 'asset-manifest.json') -Raw | ConvertFrom-Json
$imageHashes = @($manifest.assets | Where-Object { $_.path -match '\.(png|jpg)$' } | ForEach-Object { $_.sha256.ToUpperInvariant() })
$exportedImages = @()
foreach ($image in $exported.images) {
    $file = Local-Dependency $run $image.uri
    $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    Require ($imageHashes -contains $hash) "Exported texture is not an exact accepted reference copy: $file"
    $exportedImages += [ordered]@{uri=$image.uri; sha256=$hash}
}
Require ($exportedImages.Count -eq 8 -and @($exportedImages.sha256 | Select-Object -Unique).Count -eq 8) 'All eight supported reference images must be exported exactly once'
foreach ($texture in $exported.textures) { Require ($texture.source -ge 0 -and $texture.source -lt $exported.images.Count) 'Texture source index is invalid' }
Require ((Get-Content $logPath -Raw) -match 'glTF texture fallback:.*coffeemat.glb') 'Embedded unsupported texture diagnostics must be visible'
$report = [ordered]@{passed=$true; executable=$Executable; executableSha256=(Get-FileHash $Executable -Algorithm SHA256).Hash;
    sources=$sourceRecords; structuralSurfaceIndexCounts=$surfaceCounts; export=$gltfPath; meshCount=$exported.meshes.Count; images=$exportedImages;
    scope='Geometry/indices/UVs and portable supported-image bytes; embedded KTX2 material rendering remains unsupported'}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'report.json') -Encoding utf8
Write-Output "PASS: source compression, imported geometry counts, finite streams, index bounds, portable URIs, 8 image hashes. Evidence: $run"
