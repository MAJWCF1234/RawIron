#Requires -Version 5.1
<#
.SYNOPSIS
  GUI wizard (or silent -NoGui) to download the split full-workspace GitHub release, verify SHA256, and extract.

.EXAMPLE
  .\RawIron.FullWorkspace.Installer.cmd

.EXAMPLE
  .\RawIron.FullWorkspace.Installer.ps1 -NoGui -InstallRoot D:\RawIronWS
#>
param(
    [string] $ReleaseTag = 'full-workspace-msvc-2026-05-02',
    [string] $Repo = 'MAJWCF1234/RawIron',
    [string] $InstallRoot = '',
    [string] $DownloadCache = (Join-Path $env:LOCALAPPDATA 'RawIron\release-downloads'),
    [switch] $SkipDownload,
    [string] $ExpectedSha256 = '966411d7eea09ca664e44fed1f3d51ee5346287496d4fa9715d1735b95b81261',
    [switch] $SkipHashCheck,
    [switch] $WhatIf,
    [switch] $NoGui
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PartNames = @(
    'RawIron_full_release_with_builds.zip.part01',
    'RawIron_full_release_with_builds.zip.part02',
    'RawIron_full_release_with_builds.zip.part03'
)
$script:OutZipName = 'RawIron_full_release_with_builds.zip'

function Get-ReleaseBaseUrl {
    param([string]$Repo, [string]$ReleaseTag)
    return "https://github.com/$Repo/releases/download/$ReleaseTag"
}

function Invoke-DownloadOnePart {
    param(
        [string]$Uri,
        [string]$Dest,
        [string]$Label
    )
    $parent = Split-Path -Parent $Dest
    if (!(Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $usedBits = $false
    try {
        Import-Module BitsTransfer -ErrorAction Stop
        Start-BitsTransfer -Source $Uri -Destination $Dest -DisplayName "RawIron: $Label" -ErrorAction Stop | Out-Null
        $usedBits = $true
    }
    catch {
        Get-BitsTransfer -ErrorAction SilentlyContinue |
            Where-Object { $_.JobState -notin @('Transferred', 'Acknowledged') } |
            ForEach-Object { Complete-BitsTransfer -BitsJob $_ -ErrorAction SilentlyContinue }
    }

    if (!$usedBits) {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $Uri -OutFile $Dest -UseBasicParsing
    }
}

function Invoke-InstallPipeline {
    param(
        [hashtable]$Options,
        [System.ComponentModel.BackgroundWorker]$Worker
    )

    function Rep([int]$pct, [string]$msg) {
        if ($null -ne $Worker) {
            $null = $Worker.ReportProgress([Math]::Min(100, [Math]::Max(0, $pct)), $msg)
        }
        else {
            Write-Host "[RawIron installer] $msg"
        }
    }

    $Repo = $Options.Repo
    $ReleaseTag = $Options.ReleaseTag
    $DownloadCache = $Options.DownloadCache
    $InstallRoot = $Options.InstallRoot
    $SkipDownload = $Options.SkipDownload
    $ExpectedSha256 = $Options.ExpectedSha256
    $SkipHashCheck = $Options.SkipHashCheck

    $baseUrl = Get-ReleaseBaseUrl -Repo $Repo -ReleaseTag $ReleaseTag
    New-Item -ItemType Directory -Force -Path $DownloadCache | Out-Null

    $n = $script:PartNames.Count
    $dlSpan = 70
    $dlBase = 5

    for ($i = 0; $i -lt $n; $i++) {
        $name = $script:PartNames[$i]
        $pctMark = [int]($dlBase + $dlSpan * $i / $n)
        if (!$SkipDownload) {
            $uri = "$baseUrl/$name"
            $dest = Join-Path $DownloadCache $name
            if ((Test-Path -LiteralPath $dest) -and (Get-Item -LiteralPath $dest).Length -gt 0) {
                Rep $pctMark "Using cached file ($($i + 1)/$n): $name"
                continue
            }
            Rep $pctMark "Downloading part $($i + 1) of $n ($name) ..."
            Invoke-DownloadOnePart -Uri $uri -Dest $dest -Label $name
        }
    }

    $assembled = Join-Path $DownloadCache $script:OutZipName
    if (Test-Path -LiteralPath $assembled) {
        Remove-Item -LiteralPath $assembled -Force
    }

    Rep 76 "Joining parts into a single archive ..."
    $outStream = [System.IO.File]::Create($assembled)
    try {
        $buffer = New-Object byte[] (8MB)
        foreach ($name in $script:PartNames) {
            $partPath = Join-Path $DownloadCache $name
            if (!(Test-Path -LiteralPath $partPath)) {
                throw "Missing part file: $partPath"
            }
            $inStream = [System.IO.File]::OpenRead($partPath)
            try {
                $read = 0
                while (($read = $inStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    $outStream.Write($buffer, 0, $read)
                }
            }
            finally {
                $inStream.Dispose()
            }
        }
    }
    finally {
        $outStream.Dispose()
    }

    if (!$SkipHashCheck) {
        Rep 82 "Verifying SHA256 ..."
        $hash = (Get-FileHash -LiteralPath $assembled -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -ne $ExpectedSha256.ToLowerInvariant()) {
            throw "SHA256 mismatch. Expected $ExpectedSha256 got $hash. Clear folder: $DownloadCache or change release tag/hash if you republished."
        }
        Rep 86 "SHA256 verified."
    }
    else {
        Rep 86 "Skipped SHA256 check."
    }

    Rep 90 "Extracting (several minutes for a large archive) ..."
    New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
    Expand-Archive -LiteralPath $assembled -DestinationPath $InstallRoot -Force
    Rep 100 "Finished."
}

function Enter-ConsoleInstall {
    if (-not $InstallRoot) {
        $defaultDest = Join-Path $env:USERPROFILE 'RawIronFullWorkspace'
        $InstallRoot = Read-Host "Extract workspace to path [default: $defaultDest]"
        if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
            $InstallRoot = $defaultDest
        }
    }
    $InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)
    $DownloadCache = [System.IO.Path]::GetFullPath($DownloadCache)

    $opts = @{
        Repo           = $Repo
        ReleaseTag     = $ReleaseTag
        DownloadCache  = $DownloadCache
        InstallRoot    = $InstallRoot
        SkipDownload   = $SkipDownload
        ExpectedSha256 = $ExpectedSha256
        SkipHashCheck  = $SkipHashCheck
    }
    Invoke-InstallPipeline -Options $opts -Worker $null
    Write-Host "[RawIron installer] Done. Workspace: $InstallRoot"
}

function Enter-GuiInstall {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    $form = New-Object System.Windows.Forms.Form
    $form.Text = 'RawIron - Full workspace installer'
    $form.Size = New-Object System.Drawing.Size(640, 540)
    $form.StartPosition = 'CenterScreen'
    $form.FormBorderStyle = 'FixedDialog'
    $form.MaximizeBox = $false

    $lblIntro = New-Object System.Windows.Forms.Label
    $lblIntro.Location = New-Object System.Drawing.Point(16, 12)
    $lblIntro.Size = New-Object System.Drawing.Size(600, 72)
    $lblIntro.Text = @(
        "Downloads all ZIP parts for the selected GitHub release, joins them, verifies SHA256, and extracts."
        "Repository: $Repo"
        "Default SHA256 matches release ``full-workspace-msvc-2026-05-02``; if you change the tag, update the hash from release notes or use Skip verification."
    ) -join "`r`n"
    $form.Controls.Add($lblIntro)

    $lblDest = New-Object System.Windows.Forms.Label
    $lblDest.Location = New-Object System.Drawing.Point(16, 92)
    $lblDest.Size = New-Object System.Drawing.Size(240, 20)
    $lblDest.Text = 'Extract workspace to this folder:'
    $form.Controls.Add($lblDest)

    $txtDest = New-Object System.Windows.Forms.TextBox
    $txtDest.Location = New-Object System.Drawing.Point(16, 114)
    $txtDest.Size = New-Object System.Drawing.Size(480, 24)
    $txtDest.Text = (Join-Path $env:USERPROFILE 'RawIronFullWorkspace')
    $form.Controls.Add($txtDest)

    $btnBrowse = New-Object System.Windows.Forms.Button
    $btnBrowse.Location = New-Object System.Drawing.Point(510, 111)
    $btnBrowse.Size = New-Object System.Drawing.Size(100, 28)
    $btnBrowse.Text = 'Browse...'
    $btnBrowse.Add_Click({
            $d = New-Object System.Windows.Forms.FolderBrowserDialog
            $d.Description = 'Choose folder for the extracted workspace'
            if ($txtDest.Text) {
                $d.SelectedPath = $txtDest.Text
            }
            if ($d.ShowDialog() -eq 'OK') {
                $txtDest.Text = $d.SelectedPath
            }
        })
    $form.Controls.Add($btnBrowse)

    $lblTag = New-Object System.Windows.Forms.Label
    $lblTag.Location = New-Object System.Drawing.Point(16, 152)
    $lblTag.Size = New-Object System.Drawing.Size(90, 20)
    $lblTag.Text = 'Release tag:'
    $form.Controls.Add($lblTag)

    $txtTag = New-Object System.Windows.Forms.TextBox
    $txtTag.Location = New-Object System.Drawing.Point(110, 149)
    $txtTag.Size = New-Object System.Drawing.Size(500, 24)
    $txtTag.Text = $ReleaseTag
    $form.Controls.Add($txtTag)

    $lblSha = New-Object System.Windows.Forms.Label
    $lblSha.Location = New-Object System.Drawing.Point(16, 182)
    $lblSha.Size = New-Object System.Drawing.Size(90, 20)
    $lblSha.Text = 'SHA256 (full zip):'
    $form.Controls.Add($lblSha)

    $txtSha = New-Object System.Windows.Forms.TextBox
    $txtSha.Location = New-Object System.Drawing.Point(110, 179)
    $txtSha.Size = New-Object System.Drawing.Size(500, 24)
    $txtSha.Text = $ExpectedSha256
    $form.Controls.Add($txtSha)

    $chkSkipHash = New-Object System.Windows.Forms.CheckBox
    $chkSkipHash.Location = New-Object System.Drawing.Point(16, 212)
    $chkSkipHash.Size = New-Object System.Drawing.Size(420, 24)
    $chkSkipHash.Text = 'Skip SHA256 verification (only if you trust the download)'
    $form.Controls.Add($chkSkipHash)

    $lblCache = New-Object System.Windows.Forms.Label
    $lblCache.Location = New-Object System.Drawing.Point(16, 242)
    $lblCache.Size = New-Object System.Drawing.Size(600, 36)
    $lblCache.Text = "Download cache (parts + reassembled zip):`r`n$DownloadCache"
    $form.Controls.Add($lblCache)

    $progress = New-Object System.Windows.Forms.ProgressBar
    $progress.Location = New-Object System.Drawing.Point(16, 288)
    $progress.Size = New-Object System.Drawing.Size(594, 22)
    $progress.Minimum = 0
    $progress.Maximum = 100
    $progress.Value = 0
    $form.Controls.Add($progress)

    $lblStatus = New-Object System.Windows.Forms.Label
    $lblStatus.Location = New-Object System.Drawing.Point(16, 318)
    $lblStatus.Size = New-Object System.Drawing.Size(600, 40)
    $lblStatus.Text = 'Ready.'
    $form.Controls.Add($lblStatus)

    $log = New-Object System.Windows.Forms.TextBox
    $log.Multiline = $true
    $log.ScrollBars = 'Vertical'
    $log.ReadOnly = $true
    $log.Location = New-Object System.Drawing.Point(16, 364)
    $log.Size = New-Object System.Drawing.Size(594, 100)
    $log.Font = New-Object System.Drawing.Font('Consolas', 9)
    $form.Controls.Add($log)

    $btnGo = New-Object System.Windows.Forms.Button
    $btnGo.Location = New-Object System.Drawing.Point(380, 474)
    $btnGo.Size = New-Object System.Drawing.Size(120, 32)
    $btnGo.Text = 'Install'
    $form.Controls.Add($btnGo)

    $btnClose = New-Object System.Windows.Forms.Button
    $btnClose.Location = New-Object System.Drawing.Point(510, 474)
    $btnClose.Size = New-Object System.Drawing.Size(100, 32)
    $btnClose.Text = 'Close'
    $btnClose.Add_Click({ $form.Close() })
    $form.Controls.Add($btnClose)

    $worker = New-Object System.ComponentModel.BackgroundWorker
    $worker.WorkerReportsProgress = $true

    $worker.Add_DoWork({
            param($sender, $e)
            Invoke-InstallPipeline -Options $e.Argument -Worker $sender
        })

    $worker.Add_ProgressChanged({
            param($sender, $e)
            $progress.Value = [Math]::Min(100, $e.ProgressPercentage)
            $lblStatus.Text = [string]$e.UserState
            $log.AppendText("$([string]$e.UserState)`r`n")
        })

    $worker.Add_RunWorkerCompleted({
            param($sender, $e)
            $btnGo.Enabled = $true
            if ($e.Error) {
                [System.Windows.Forms.MessageBox]::Show(
                    $e.Error.Exception.Message,
                    'Install failed',
                    'OK',
                    'Error'
                ) | Out-Null
                $lblStatus.Text = 'Failed - see message box.'
            }
            else {
                [System.Windows.Forms.MessageBox]::Show(
                    "Workspace extracted to:`r`n$($txtDest.Text.Trim())`r`n`r`nBinaries: build\dev-msvc (inside that folder). See RELEASE_BUILD_INFO.txt in the extract.",
                    'Install complete',
                    'OK',
                    'Information'
                ) | Out-Null
                $lblStatus.Text = 'Complete.'
            }
        })

    $btnGo.Add_Click({
            $btnGo.Enabled = $false
            $progress.Value = 0
            $log.Clear()
            $lblStatus.Text = 'Starting...'

            try {
                $destPath = [System.IO.Path]::GetFullPath($txtDest.Text.Trim())
            }
            catch {
                [System.Windows.Forms.MessageBox]::Show('Invalid install folder path.', 'Error') | Out-Null
                $btnGo.Enabled = $true
                return
            }

            $sha = $txtSha.Text.Trim()
            if ([string]::IsNullOrWhiteSpace($sha) -and !$chkSkipHash.Checked) {
                [System.Windows.Forms.MessageBox]::Show('Enter SHA256 or enable Skip verification.', 'Error') | Out-Null
                $btnGo.Enabled = $true
                return
            }

            $opts = @{
                Repo           = $Repo
                ReleaseTag     = $txtTag.Text.Trim()
                DownloadCache  = [System.IO.Path]::GetFullPath($DownloadCache)
                InstallRoot    = $destPath
                SkipDownload   = $SkipDownload
                ExpectedSha256 = $sha
                SkipHashCheck  = $chkSkipHash.Checked
            }

            if (!$worker.IsBusy) {
                $worker.RunWorkerAsync($opts)
            }
        })

    [void]$form.ShowDialog()
}

if ($WhatIf) {
    if (!$NoGui) {
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.MessageBox]::Show(
            'Use -WhatIf together with -NoGui (console) for a dry-run message.',
            'RawIron installer'
        ) | Out-Null
        exit 1
    }
    $wiDest = if ($InstallRoot) { $InstallRoot } else { '(prompt at run)' }
    Write-Host "[RawIron installer] WhatIf: tag=$ReleaseTag cache=$DownloadCache install=$wiDest"
    exit 0
}

if ($NoGui) {
    Enter-ConsoleInstall
}
else {
    Enter-GuiInstall
}
