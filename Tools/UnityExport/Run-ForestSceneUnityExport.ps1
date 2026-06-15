#Requires -Version 5.1
$ErrorActionPreference = "Stop"

$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$UnityProject = Join-Path $WorkspaceRoot "Assets\Source\Forest Scene"
$ExportOutput = Join-Path $WorkspaceRoot "Games\WildernessRuins\Assets\Generated\ForestScene\Meshes"
$PreferredUnityVersion = "2019.2.9f1"
$ExecuteMethod = "RawIronForestExport.ExportForRawIron.Run"
$LogFile = Join-Path $UnityProject "Logs\rawiron-forest-export.log"
$YamlExporter = Join-Path $WorkspaceRoot "Tools\UnityExport\export_botd_unity_yaml_meshes.py"

function Find-UnityEditorExe {
    param([string]$Version)

    if ($env:UNITY_EXE -and (Test-Path -LiteralPath $env:UNITY_EXE)) {
        return (Resolve-Path -LiteralPath $env:UNITY_EXE).Path
    }

    $hubCandidates = @(
        "$env:ProgramFiles\Unity\Hub\Editor\$Version\Editor\Unity.exe",
        "${env:ProgramFiles(x86)}\Unity\Hub\Editor\$Version\Editor\Unity.exe",
        "$env:LOCALAPPDATA\Programs\Unity\Hub\Editor\$Version\Editor\Unity.exe"
    )
    foreach ($candidate in $hubCandidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $searchRoots = @(
        "$env:ProgramFiles\Unity\Hub\Editor",
        "$env:LOCALAPPDATA\Programs\Unity\Hub\Editor",
        "C:\Unity",
        "D:\Unity"
    )
    $matches = @()
    foreach ($root in $searchRoots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $matches += Get-ChildItem -LiteralPath $root -Recurse -Filter "Unity.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match [regex]::Escape("Editor\Unity.exe") }
    }
    if ($matches.Count -gt 0) {
        return ($matches | Sort-Object FullName -Descending | Select-Object -First 1).FullName
    }

    return $null
}

if (-not (Test-Path -LiteralPath $UnityProject)) {
    throw "Unity project not found: $UnityProject"
}

$unityExe = Find-UnityEditorExe -Version $PreferredUnityVersion
if (-not $unityExe) {
    Write-Host ""
    Write-Host "Unity $PreferredUnityVersion was not found. Falling back to YAML mesh export."
    Write-Host "For prefab hierarchy exports, set UNITY_EXE to your Unity Editor and re-run."
    Write-Host ""
    if (-not (Test-Path -LiteralPath $YamlExporter)) {
        throw "Missing YAML exporter: $YamlExporter"
    }
    py -3 $YamlExporter
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $objCount = (Get-ChildItem -LiteralPath $ExportOutput -Filter "*.obj" -ErrorAction SilentlyContinue).Count
    Write-Host "YAML export complete. OBJ files: $objCount"
    Write-Host "Folder: $ExportOutput"
    exit 0
}

New-Item -ItemType Directory -Force -Path (Split-Path $LogFile) | Out-Null
New-Item -ItemType Directory -Force -Path $ExportOutput | Out-Null

$env:RAWIRON_FOREST_EXPORT_DIR = $ExportOutput

Write-Host "Unity:   $unityExe"
Write-Host "Project: $UnityProject"
Write-Host "Output:  $ExportOutput"
Write-Host "Log:     $LogFile"
Write-Host ""

$unityArgs = @(
    "-batchmode",
    "-nographics",
    "-quit",
    "-projectPath", $UnityProject,
    "-executeMethod", $ExecuteMethod,
    "-logFile", $LogFile
)

& $unityExe @unityArgs
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Host "Unity export failed (exit $exitCode). See log: $LogFile"
    exit $exitCode
}

$objCount = (Get-ChildItem -LiteralPath $ExportOutput -Filter "*.obj" -ErrorAction SilentlyContinue).Count
Write-Host "Export complete. OBJ files: $objCount"
Write-Host "Folder: $ExportOutput"
exit 0
