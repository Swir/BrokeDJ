# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$AppPath,

    [string]$EvidencePath = (Join-Path (Get-Location) 'BrokeDJ-M1-Hardware-Witness.json'),

    [string]$ProbePath = (Join-Path (Get-Location) 'BrokeDJ-device-probe.json'),

    [switch]$ValidateExisting
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string]$Message) {
    Write-Host "[BrokeDJ M1] $Message"
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
    if ($env:OS -ne 'Windows_NT') {
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
    if ($item.Name -ne 'BrokeDJ.exe') { throw 'AppPath must point to BrokeDJ.exe.' }
    return $item
}

function Get-AppFingerprint([System.IO.FileInfo]$App) {
    $version = $App.VersionInfo.FileVersion
    if ([string]::IsNullOrWhiteSpace($version)) { $version = 'unknown' }
    return [ordered]@{
        fileName = $App.Name
        fileVersion = $version
        sha256 = (Get-FileHash -LiteralPath $App.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Get-RequiredProperty([object]$Object, [string]$Name, [string]$Context) {
    if ($null -eq $Object) { throw "Missing $Context object." }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { throw "Missing $Context.$Name." }
    return $property.Value
}

function Assert-ExactProperties([object]$Object, [string[]]$Expected, [string]$Context) {
    if ($null -eq $Object) { throw "Missing $Context object." }
    $actual = @($Object.PSObject.Properties | ForEach-Object { $_.Name })
    foreach ($name in $Expected) {
        if ($actual -notcontains $name) { throw "Missing $Context.$name." }
    }
    $unexpected = @($actual | Where-Object { $Expected -notcontains $_ })
    if ($unexpected.Count -gt 0) {
        throw ("Unexpected $Context properties: " + ($unexpected -join ', '))
    }
}

function Assert-IntegerProperty([object]$Object, [string]$Name, [long]$Minimum, [string]$Context) {
    $value = Get-RequiredProperty -Object $Object -Name $Name -Context $Context
    if ($value -isnot [int] -and $value -isnot [long]) {
        throw "$Context.$Name must be a JSON integer."
    }
    $numeric = [long]$value
    if ($numeric -lt $Minimum) {
        throw "$Context.$Name must be at least $Minimum."
    }
    return $numeric
}

function Assert-StringProperty([object]$Object, [string]$Name, [string]$Context) {
    $value = Get-RequiredProperty -Object $Object -Name $Name -Context $Context
    if ($value -isnot [string]) {
        throw "$Context.$Name must be a JSON string."
    }
    return [string]$value
}

function Assert-BooleanProperty([object]$Object, [string]$Name, [bool]$Expected, [string]$Context) {
    $value = Get-RequiredProperty -Object $Object -Name $Name -Context $Context
    if ($value -isnot [bool]) {
        throw "$Context.$Name must be a JSON boolean."
    }
    if ($value -ne $Expected) {
        throw "$Context.$Name must be $($Expected.ToString().ToLowerInvariant())."
    }
}

function Resolve-Probe([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $item = Get-Item -LiteralPath $resolved
    if ($item.PSIsContainer) { throw 'ProbePath must point to BrokeDJ-device-probe.json.' }
    if ($item.Name -ne 'BrokeDJ-device-probe.json') {
        throw 'ProbePath must point to BrokeDJ-device-probe.json.'
    }
    return $item
}

function Validate-Probe([System.IO.FileInfo]$Probe) {
    $data = Get-Content -LiteralPath $Probe.FullName -Raw | ConvertFrom-Json

    $schema = Get-RequiredProperty -Object $data -Name 'schema_version' -Context 'probe'
    if ($schema -isnot [int] -and $schema -isnot [long]) { throw 'probe.schema_version must be a JSON integer.' }
    if ([long]$schema -ne 2) { throw 'Unsupported device-probe schema; expected schema_version=2.' }

    $mode = Get-RequiredProperty -Object $data -Name 'mode' -Context 'probe'
    if ($mode -isnot [string] -or $mode -ne 'silent-capability') {
        throw 'M1 witness requires a full silent-capability probe, not CI enumeration.'
    }

    Assert-BooleanProperty -Object $data -Name 'plays_audio' -Expected $false -Context 'probe'
    Assert-BooleanProperty -Object $data -Name 'calls_device_open' -Expected $false -Context 'probe'
    Assert-BooleanProperty -Object $data -Name 'starts_audio_callback' -Expected $false -Context 'probe'
    Assert-BooleanProperty -Object $data -Name 'creates_device_descriptors' -Expected $true -Context 'probe'
    Assert-BooleanProperty -Object $data -Name 'safety_invariants_ok' -Expected $true -Context 'probe'

    $unexpectedOpen = Get-RequiredProperty -Object $data -Name 'unexpected_open_state_count' -Context 'probe'
    if ($unexpectedOpen -isnot [int] -and $unexpectedOpen -isnot [long]) {
        throw 'probe.unexpected_open_state_count must be a JSON integer.'
    }
    if ([long]$unexpectedOpen -ne 0) { throw 'Device probe observed an unexpected open device state.' }

    $backendCount = Get-RequiredProperty -Object $data -Name 'backend_count' -Context 'probe'
    $outputCount = Get-RequiredProperty -Object $data -Name 'output_device_count' -Context 'probe'
    $fourOutputCount = Get-RequiredProperty -Object $data -Name 'four_output_candidate_count' -Context 'probe'
    foreach ($entry in @(
        @{ Name = 'backend_count'; Value = $backendCount },
        @{ Name = 'output_device_count'; Value = $outputCount },
        @{ Name = 'four_output_candidate_count'; Value = $fourOutputCount }
    )) {
        if ($entry.Value -isnot [int] -and $entry.Value -isnot [long]) {
            throw "probe.$($entry.Name) must be a JSON integer."
        }
        if ([long]$entry.Value -lt 0) { throw "probe.$($entry.Name) cannot be negative." }
    }

    if ([long]$backendCount -lt 1) { throw 'No audio backends were reported by the M1 device probe.' }
    if ([long]$outputCount -lt 1) { throw 'No output devices were reported by the M1 device probe.' }
    if ([long]$fourOutputCount -lt 1) {
        throw 'No four-output candidate was reported; M1 four-output cue qualification cannot be completed from this probe.'
    }

    $backends = @(Get-RequiredProperty -Object $data -Name 'backends' -Context 'probe')
    if ($backends.Count -ne [long]$backendCount) {
        throw 'Device-probe backend_count does not match the serialized backend array.'
    }

    return [ordered]@{
        fileName = $Probe.Name
        schemaVersion = [long]$schema
        sha256 = (Get-FileHash -LiteralPath $Probe.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        outputDeviceCount = [long]$outputCount
        fourOutputCandidateCount = [long]$fourOutputCount
        safetyInvariantsOk = $true
    }
}

function Validate-Evidence([string]$Path, [System.IO.FileInfo]$ExpectedApp, [System.IO.FileInfo]$ExpectedProbe) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $data = Get-Content -LiteralPath $resolved -Raw | ConvertFrom-Json

    Assert-ExactProperties -Object $data -Expected @(
        'schema', 'project', 'scope', 'generatedUtc', 'environment', 'app', 'deviceProbe', 'checks', 'privacy'
    ) -Context 'evidence'

    $schema = Assert-IntegerProperty -Object $data -Name 'schema' -Minimum 1 -Context 'evidence'
    if ($schema -ne 1) { throw 'Unsupported M1 witness schema.' }
    $project = Assert-StringProperty -Object $data -Name 'project' -Context 'evidence'
    $scope = Assert-StringProperty -Object $data -Name 'scope' -Context 'evidence'
    if ($project -ne 'BrokeDJ' -or $scope -ne 'M1-windows-hardware') {
        throw 'Evidence file is not a BrokeDJ M1 hardware witness.'
    }

    $generatedUtc = Get-RequiredProperty -Object $data -Name 'generatedUtc' -Context 'evidence'
    $parsedGeneratedUtc = [DateTimeOffset]::MinValue
    if ($generatedUtc -is [DateTime]) {
        $parsedGeneratedUtc = [DateTimeOffset]$generatedUtc
    } elseif ($generatedUtc -is [DateTimeOffset]) {
        $parsedGeneratedUtc = $generatedUtc
    } elseif ($generatedUtc -is [string]) {
        if (-not [DateTimeOffset]::TryParse($generatedUtc, [ref]$parsedGeneratedUtc)) {
            throw 'evidence.generatedUtc is not a valid timestamp.'
        }
    } else {
        throw 'evidence.generatedUtc must be an ISO-8601 timestamp.'
    }

    Assert-ExactProperties -Object $data.environment -Expected @(
        'windowsBuild', 'architecture', 'processArchitecture'
    ) -Context 'environment'
    $windowsBuild = Assert-IntegerProperty -Object $data.environment -Name 'windowsBuild' -Minimum 22000 -Context 'environment'
    $architecture = Assert-StringProperty -Object $data.environment -Name 'architecture' -Context 'environment'
    $processArchitecture = Assert-StringProperty -Object $data.environment -Name 'processArchitecture' -Context 'environment'
    if ($architecture -ne 'X64' -or $processArchitecture -ne 'X64') {
        throw 'M1 witness evidence must come from an x64 OS and x64 PowerShell process.'
    }

    Assert-ExactProperties -Object $data.app -Expected @('fileName', 'fileVersion', 'sha256') -Context 'app'
    $expectedFingerprint = Get-AppFingerprint -App $ExpectedApp
    $evidenceFileName = Assert-StringProperty -Object $data.app -Name 'fileName' -Context 'app'
    $evidenceVersion = Assert-StringProperty -Object $data.app -Name 'fileVersion' -Context 'app'
    $evidenceHash = Assert-StringProperty -Object $data.app -Name 'sha256' -Context 'app'
    if ($evidenceHash -notmatch '^[0-9a-fA-F]{64}$') { throw 'app.sha256 is not a valid SHA-256 digest.' }
    if ($evidenceFileName -ne $expectedFingerprint.fileName) { throw 'Evidence executable filename does not match AppPath.' }
    if ($evidenceVersion -ne $expectedFingerprint.fileVersion) { throw 'Evidence executable version does not match AppPath.' }
    if ($evidenceHash.ToLowerInvariant() -ne $expectedFingerprint.sha256) { throw 'Evidence executable SHA-256 does not match AppPath.' }

    $expectedProbeFingerprint = Validate-Probe -Probe $ExpectedProbe
    Assert-ExactProperties -Object $data.deviceProbe -Expected @(
        'fileName', 'schemaVersion', 'sha256', 'outputDeviceCount', 'fourOutputCandidateCount', 'safetyInvariantsOk'
    ) -Context 'deviceProbe'
    $probeFileName = Assert-StringProperty -Object $data.deviceProbe -Name 'fileName' -Context 'deviceProbe'
    $probeSchema = Assert-IntegerProperty -Object $data.deviceProbe -Name 'schemaVersion' -Minimum 1 -Context 'deviceProbe'
    $probeHash = Assert-StringProperty -Object $data.deviceProbe -Name 'sha256' -Context 'deviceProbe'
    $probeOutputCount = Assert-IntegerProperty -Object $data.deviceProbe -Name 'outputDeviceCount' -Minimum 1 -Context 'deviceProbe'
    $probeFourOutputCount = Assert-IntegerProperty -Object $data.deviceProbe -Name 'fourOutputCandidateCount' -Minimum 1 -Context 'deviceProbe'
    Assert-BooleanProperty -Object $data.deviceProbe -Name 'safetyInvariantsOk' -Expected $true -Context 'deviceProbe'
    if ($probeHash -notmatch '^[0-9a-fA-F]{64}$') { throw 'deviceProbe.sha256 is not a valid SHA-256 digest.' }
    if ($probeFileName -ne $expectedProbeFingerprint.fileName -or
        $probeSchema -ne $expectedProbeFingerprint.schemaVersion -or
        $probeOutputCount -ne $expectedProbeFingerprint.outputDeviceCount -or
        $probeFourOutputCount -ne $expectedProbeFingerprint.fourOutputCandidateCount -or
        $probeHash.ToLowerInvariant() -ne $expectedProbeFingerprint.sha256) {
        throw 'Evidence device-probe identity or counts do not match ProbePath.'
    }

    $requiredChecks = @(
        'cleanLaunch',
        'resizeAndHiDpi',
        'importAndPlayback',
        'deviceSwitchRecovery',
        'fourOutputCueIsolation',
        'runtimeErrorReview'
    )
    Assert-ExactProperties -Object $data.checks -Expected $requiredChecks -Context 'checks'
    $failed = @()
    foreach ($name in $requiredChecks) {
        try {
            Assert-BooleanProperty -Object $data.checks -Name $name -Expected $true -Context 'checks'
        } catch {
            $failed += $name
        }
    }
    if ($failed.Count -gt 0) {
        throw ('Witness is incomplete or malformed. Failed checks: ' + ($failed -join ', '))
    }

    $privacyProperties = @('containsDeviceNames', 'containsTrackPaths', 'containsTrackNames', 'containsSourceMusic')
    Assert-ExactProperties -Object $data.privacy -Expected $privacyProperties -Context 'privacy'
    foreach ($name in $privacyProperties) {
        Assert-BooleanProperty -Object $data.privacy -Name $name -Expected $false -Context 'privacy'
    }

    Write-Step "Witness is complete and bound to AppPath/ProbePath: $resolved"
    Write-Step "App SHA-256: $($expectedFingerprint.sha256)"
    Write-Step "Probe SHA-256: $($expectedProbeFingerprint.sha256)"
    Write-Step "Windows build: $windowsBuild"
}

$app = Resolve-App -Path $AppPath

if ($ValidateExisting) {
    $probe = Resolve-Probe -Path $ProbePath
    Validate-Evidence -Path $EvidencePath -ExpectedApp $app -ExpectedProbe $probe
    exit 0
}

$windowsBuild = Assert-Windows11
$osArchitecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$processArchitecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
if ($osArchitecture -ne 'X64' -or $processArchitecture -ne 'X64') {
    throw 'M1 witness requires an x64 Windows OS and x64 PowerShell process.'
}

$probeParent = Split-Path -Parent $ProbePath
if ([string]::IsNullOrWhiteSpace($probeParent)) { $probeParent = (Get-Location).Path }
if (-not (Test-Path -LiteralPath $probeParent)) {
    New-Item -ItemType Directory -Path $probeParent -Force | Out-Null
}
$probeParent = (Resolve-Path -LiteralPath $probeParent).Path
$expectedProbePath = Join-Path $probeParent 'BrokeDJ-device-probe.json'
if ([System.IO.Path]::GetFullPath($ProbePath) -ne [System.IO.Path]::GetFullPath($expectedProbePath)) {
    throw 'ProbePath filename must be BrokeDJ-device-probe.json because BrokeDJ writes that fixed diagnostic filename.'
}

Remove-Item -LiteralPath $expectedProbePath -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $probeParent 'BrokeDJ-device-probe.txt') -ErrorAction SilentlyContinue
Write-Step 'Running the silent device capability probe. It does not open an audio device or start an audio callback.'
$probeProcess = Start-Process -FilePath $app.FullName -ArgumentList '--device-probe' -WorkingDirectory $probeParent -PassThru -Wait
if ($probeProcess.ExitCode -ne 0) {
    throw "BrokeDJ silent device probe failed with exit code $($probeProcess.ExitCode)."
}
$probe = Resolve-Probe -Path $expectedProbePath
$probeFingerprint = Validate-Probe -Probe $probe
$appFingerprint = Get-AppFingerprint -App $app

Write-Step 'The generated witness JSON stores no device names, track names, track paths or source music.'
Write-Step 'The separate BrokeDJ-device-probe.json can contain local backend/device names; keep it private unless reviewed.'
Write-Step 'The script does not automate playback or loud tests. Keep hardware volume low and perform the documented steps manually.'
Write-Host ''

$checks = [ordered]@{}
$checks.cleanLaunch = Read-YesNo 'The staged BrokeDJ build launched cleanly on Windows 11 x64 without a crash?'
$checks.resizeAndHiDpi = Read-YesNo 'The UI remained usable at 1050x800 and a normal desktop size/qualified display scale without overlapping critical controls?'
$checks.importAndPlayback = Read-YesNo 'A supported local test track imported and ordinary playback worked through the intended real output device at safe volume?'
$checks.deviceSwitchRecovery = Read-YesNo 'Switching to another intended audio device/backend and back recovered without a crash, stale routing or unusable playback?'
$checks.fourOutputCueIsolation = Read-YesNo 'On a real four-output interface, master stayed on 1/2 and private CUE stayed isolated on 3/4 for at least two decks and after reopening/switching device settings?'
$checks.runtimeErrorReview = Read-YesNo 'Available driver/runtime/xrun diagnostics were reviewed and no unresolved M1-blocking error remained?'

$evidence = [ordered]@{
    schema = 1
    project = 'BrokeDJ'
    scope = 'M1-windows-hardware'
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    environment = [ordered]@{
        windowsBuild = $windowsBuild
        architecture = $osArchitecture
        processArchitecture = $processArchitecture
    }
    app = $appFingerprint
    deviceProbe = $probeFingerprint
    checks = $checks
    privacy = [ordered]@{
        containsDeviceNames = $false
        containsTrackPaths = $false
        containsTrackNames = $false
        containsSourceMusic = $false
    }
}

$evidenceParent = Split-Path -Parent $EvidencePath
if (-not [string]::IsNullOrWhiteSpace($evidenceParent) -and -not (Test-Path -LiteralPath $evidenceParent)) {
    New-Item -ItemType Directory -Path $evidenceParent -Force | Out-Null
}
$evidence | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $EvidencePath -Encoding utf8
Write-Step "Evidence written to: $EvidencePath"

try {
    Validate-Evidence -Path $EvidencePath -ExpectedApp $app -ExpectedProbe $probe
} catch {
    Write-Error $_
    exit 2
}
