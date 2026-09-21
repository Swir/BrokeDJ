# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$AppPath,

    [string]$EvidencePath = (Join-Path (Get-Location) 'BrokeDJ-M4-Library-Witness.json'),

    [switch]$ValidateExisting
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string]$Message) {
    Write-Host "[BrokeDJ M4] $Message"
}

function Read-YesNo([string]$Question) {
    while ($true) {
        $answer = (Read-Host "$Question [y/n]").Trim().ToLowerInvariant()
        if ($answer -eq 'y' -or $answer -eq 'yes') { return $true }
        if ($answer -eq 'n' -or $answer -eq 'no') { return $false }
        Write-Host 'Please answer y or n.'
    }
}

function Assert-Windows11 {
    if (-not $IsWindows) {
        throw 'This witness must run on Windows.'
    }
    $build = [Environment]::OSVersion.Version.Build
    if ($build -lt 22000) {
        throw "Windows 11 is required for this witness (detected build $build)."
    }
    return $build
}

function Resolve-App([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $item = Get-Item -LiteralPath $resolved
    if ($item.PSIsContainer) { throw 'AppPath must point to BrokeDJ.exe, not a directory.' }
    if ($item.Extension -ne '.exe') { throw 'AppPath must point to a Windows executable.' }
    return $item
}

function Validate-Evidence([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $data = Get-Content -LiteralPath $resolved -Raw | ConvertFrom-Json
    if ($data.schema -ne 1) { throw 'Unsupported M4 witness schema.' }
    if ($data.project -ne 'BrokeDJ' -or $data.scope -ne 'M4-library-workflow') {
        throw 'Evidence file is not a BrokeDJ M4 library witness.'
    }

    $required = @(
        'launchAndResize',
        'importAndSearch',
        'tagsAndPlaylists',
        'history',
        'duplicateAndMissingReview',
        'relocate',
        'libraryBackupRestore',
        'sessionSaveLoad'
    )
    $failed = @()
    foreach ($name in $required) {
        if (-not [bool]$data.checks.$name) { $failed += $name }
    }
    if ($failed.Count -gt 0) {
        throw ('Witness is incomplete. Failed checks: ' + ($failed -join ', '))
    }

    Write-Step "Witness is complete: $resolved"
    Write-Step "App SHA-256: $($data.app.sha256)"
    Write-Step "Windows build: $($data.environment.windowsBuild)"
}

if ($ValidateExisting) {
    Validate-Evidence -Path $EvidencePath
    exit 0
}

$windowsBuild = Assert-Windows11
$app = Resolve-App -Path $AppPath
$appHash = (Get-FileHash -LiteralPath $app.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$appVersion = $app.VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($appVersion)) { $appVersion = 'unknown' }

Write-Step 'This witness never asks for track names or paths and does not inspect source music.'
Write-Step 'Use disposable copies or non-critical local tracks for manual interaction checks.'
Write-Step 'Do not include screenshots or notes containing private paths in public artifacts.'
Write-Host ''

$checks = [ordered]@{}
$checks.launchAndResize = Read-YesNo 'BrokeDJ launched on Windows 11 and remained usable while resizing without overlapping/hidden critical controls?'
$checks.importAndSearch = Read-YesNo 'A supported local track imported successfully and bounded library search found it?'
$checks.tagsAndPlaylists = Read-YesNo 'Tag editing plus playlist create/add/remove/browse worked and persisted after refresh?'
$checks.history = Read-YesNo 'Starting playback created a bounded local history entry visible in the History view?'
$checks.duplicateAndMissingReview = Read-YesNo 'Duplicate and missing-file review filters behaved non-destructively?'
$checks.relocate = Read-YesNo 'Relocate reconnected a deliberately moved test copy without losing tags/playlist membership?'
$checks.libraryBackupRestore = Read-YesNo 'Library backup succeeded; after a controlled metadata mutation, restore returned the prior library state?'
$checks.sessionSaveLoad = Read-YesNo 'A four-deck/mixer session snapshot saved and loaded with restored controls while decks remained paused until explicit Play?'

$evidence = [ordered]@{
    schema = 1
    project = 'BrokeDJ'
    scope = 'M4-library-workflow'
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    environment = [ordered]@{
        windowsBuild = $windowsBuild
        architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        processArchitecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
    }
    app = [ordered]@{
        fileName = $app.Name
        fileVersion = $appVersion
        sha256 = $appHash
    }
    checks = $checks
    privacy = [ordered]@{
        containsTrackPaths = $false
        containsTrackNames = $false
        containsSourceMusic = $false
    }
}

$parent = Split-Path -Parent $EvidencePath
if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}
$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $EvidencePath -Encoding utf8
Write-Step "Evidence written to: $EvidencePath"

try {
    Validate-Evidence -Path $EvidencePath
} catch {
    Write-Error $_
    exit 2
}
