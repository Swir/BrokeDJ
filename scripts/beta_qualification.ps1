# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [string]$AppPath = '.\BrokeDJ.exe',
    [string]$ProbePath = '.\BrokeDJ-device-probe.json',
    [string]$M1EvidencePath = '.\BrokeDJ-M1-Hardware-Witness.json',
    [string]$M2EvidencePath = '.\BrokeDJ-M2-keylock-listening.json',
    [string]$M3EvidencePath = '.\BrokeDJ-M3-mixer-recording.json',
    [string]$M4EvidencePath = '.\BrokeDJ-M4-Library-Witness.json',
    [string]$QualificationPath = '.\BrokeDJ-Beta-Qualification.json',
    [switch]$ValidateExisting
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string]$Message) {
    Write-Host "[BrokeDJ Beta] $Message"
}

function Test-TruthyEnvironmentValue([object]$Value) {
    if ($null -eq $Value) { return $false }
    switch (([string]$Value).Trim().ToLowerInvariant()) {
        '1' { return $true }
        'true' { return $true }
        'yes' { return $true }
        'on' { return $true }
        default { return $false }
    }
}

function Assert-NotCi {
    if ((Test-TruthyEnvironmentValue $env:GITHUB_ACTIONS) -or
        (Test-TruthyEnvironmentValue $env:CI)) {
        throw 'Beta qualification evidence is human-controlled and cannot be generated in CI.'
    }
}

function Assert-Windows11X64 {
    if ($env:OS -ne 'Windows_NT') { throw 'Beta qualification must run on Windows 11.' }
    if ([Environment]::OSVersion.Version.Build -lt 22000) {
        throw 'Beta qualification requires Windows 11 build 22000 or newer.'
    }
    $osArch = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    $processArch = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
    if ($osArch -ne 'X64' -or $processArch -ne 'X64') {
        throw 'Beta qualification requires an x64 Windows OS and x64 PowerShell process.'
    }
}

function Resolve-Leaf([string]$Path, [string]$ExpectedName) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $item = Get-Item -LiteralPath $resolved
    if ($item.PSIsContainer) { throw "$ExpectedName must be a file." }
    if ($item.Name -ne $ExpectedName) { throw "Expected $ExpectedName, got $($item.Name)." }
    return $item
}

function Resolve-WitnessScript([string]$PackagedName, [string]$RepositoryName) {
    $root = $PSScriptRoot
    if ([string]::IsNullOrWhiteSpace($root)) { $root = (Get-Location).Path }
    foreach ($name in @($PackagedName, $RepositoryName)) {
        $candidate = Join-Path $root $name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Required witness validator is missing: $PackagedName / $RepositoryName"
}

function Get-PowerShellHostPath {
    $hostName = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh.exe' } else { 'powershell.exe' }
    $candidate = Join-Path $PSHOME $hostName
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Unable to locate the current PowerShell host: $candidate"
    }
    return (Resolve-Path -LiteralPath $candidate).Path
}

function Get-Sha256([System.IO.FileInfo]$File) {
    return (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-AppFingerprint([System.IO.FileInfo]$App) {
    $version = $App.VersionInfo.FileVersion
    if ([string]::IsNullOrWhiteSpace($version)) { $version = 'unknown' }
    return [ordered]@{
        fileName = $App.Name
        fileVersion = $version
        sha256 = Get-Sha256 -File $App
    }
}

function Get-SourceCommit([System.IO.FileInfo]$App) {
    $sourceFile = Join-Path $App.Directory.FullName 'SOURCE-COMMIT.txt'
    if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
        throw 'SOURCE-COMMIT.txt is missing beside BrokeDJ.exe.'
    }
    $value = (Get-Content -LiteralPath $sourceFile -Raw).Trim().ToLowerInvariant()
    if ($value -notmatch '^[0-9a-f]{40}$') {
        throw 'SOURCE-COMMIT.txt does not contain a full Git commit SHA.'
    }
    return $value
}

function Invoke-WitnessValidator(
    [string]$ScriptPath,
    [System.IO.FileInfo]$App,
    [System.IO.FileInfo]$Evidence,
    [System.IO.FileInfo]$Probe = $null
) {
    $arguments = @('-NoProfile', '-File', $ScriptPath,
                   '-AppPath', $App.FullName,
                   '-EvidencePath', $Evidence.FullName,
                   '-ValidateExisting')
    if ($null -ne $Probe) {
        $arguments += @('-ProbePath', $Probe.FullName)
    }

    $hostExecutable = Get-PowerShellHostPath
    & $hostExecutable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Witness validation failed for $($Evidence.Name) with exit code $LASTEXITCODE."
    }
}

function Assert-ExactProperties([object]$Object, [string[]]$Expected, [string]$Context) {
    if ($null -eq $Object) { throw "$Context is missing." }
    $actual = @($Object.PSObject.Properties.Name | Sort-Object)
    $wanted = @($Expected | Sort-Object)
    if ($actual.Count -ne $wanted.Count) { throw "$Context has an unexpected property count." }
    for ($i = 0; $i -lt $actual.Count; ++$i) {
        if ($actual[$i] -cne $wanted[$i]) { throw "$Context contains unexpected or missing properties." }
    }
}

function Assert-BooleanTrue([object]$Value, [string]$Context) {
    if ($Value -isnot [bool] -or $Value -ne $true) { throw "$Context must be the JSON boolean true." }
}

function Read-Qualification([System.IO.FileInfo]$App,
                            [System.IO.FileInfo]$Probe,
                            [hashtable]$EvidenceFiles,
                            [string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $data = Get-Content -LiteralPath $resolved -Raw | ConvertFrom-Json
    Assert-ExactProperties -Object $data -Expected @(
        'schema', 'project', 'channel', 'generatedUtc', 'sourceCommit', 'app', 'gates', 'evidence', 'privacy'
    ) -Context 'qualification'
    if ($data.schema -isnot [int] -and $data.schema -isnot [long]) { throw 'qualification.schema must be a JSON integer.' }
    if ([long]$data.schema -ne 1) { throw 'Unsupported beta qualification schema.' }
    if ($data.project -isnot [string] -or $data.project -ne 'BrokeDJ') { throw 'qualification.project must be BrokeDJ.' }
    if ($data.channel -isnot [string] -or $data.channel -ne 'beta') { throw 'qualification.channel must be beta.' }
    $parsed = [DateTimeOffset]::MinValue
    if ($data.generatedUtc -is [string]) {
        if (-not [DateTimeOffset]::TryParse([string]$data.generatedUtc, [ref]$parsed)) {
            throw 'qualification.generatedUtc is invalid.'
        }
    } elseif ($data.generatedUtc -is [DateTime] -or $data.generatedUtc -is [DateTimeOffset]) {
        $parsed = [DateTimeOffset]$data.generatedUtc
    } else {
        throw 'qualification.generatedUtc must be an ISO-8601 timestamp.'
    }

    $expectedCommit = Get-SourceCommit -App $App
    if ($data.sourceCommit -isnot [string] -or $data.sourceCommit.ToLowerInvariant() -ne $expectedCommit) {
        throw 'qualification.sourceCommit does not match SOURCE-COMMIT.txt.'
    }

    Assert-ExactProperties -Object $data.app -Expected @('fileName', 'fileVersion', 'sha256') -Context 'app'
    $fingerprint = Get-AppFingerprint -App $App
    if ($data.app.fileName -ne $fingerprint.fileName -or
        $data.app.fileVersion -ne $fingerprint.fileVersion -or
        $data.app.sha256 -isnot [string] -or
        $data.app.sha256.ToLowerInvariant() -ne $fingerprint.sha256) {
        throw 'qualification.app does not match the exact BrokeDJ.exe.'
    }

    Assert-ExactProperties -Object $data.gates -Expected @('m1', 'm2', 'm3', 'm4') -Context 'gates'
    foreach ($gate in @('m1', 'm2', 'm3', 'm4')) {
        Assert-BooleanTrue -Value $data.gates.$gate -Context "gates.$gate"
    }

    Assert-ExactProperties -Object $data.evidence -Expected @('deviceProbe', 'm1', 'm2', 'm3', 'm4') -Context 'evidence'
    $expectedFiles = @{
        deviceProbe = $Probe
        m1 = $EvidenceFiles['m1']
        m2 = $EvidenceFiles['m2']
        m3 = $EvidenceFiles['m3']
        m4 = $EvidenceFiles['m4']
    }
    foreach ($key in @('deviceProbe', 'm1', 'm2', 'm3', 'm4')) {
        Assert-ExactProperties -Object $data.evidence.$key -Expected @('fileName', 'sha256') -Context "evidence.$key"
        $file = $expectedFiles[$key]
        $expectedHash = Get-Sha256 -File $file
        if ($data.evidence.$key.fileName -ne $file.Name -or
            $data.evidence.$key.sha256 -isnot [string] -or
            $data.evidence.$key.sha256.ToLowerInvariant() -ne $expectedHash) {
            throw "qualification.evidence.$key does not match the supplied file."
        }
    }

    Assert-ExactProperties -Object $data.privacy -Expected @(
        'containsDeviceNames', 'containsTrackNames', 'containsTrackPaths', 'containsRecordingPaths', 'containsSourceMusic', 'containsMicrophoneAudio'
    ) -Context 'privacy'
    foreach ($property in @(
        'containsDeviceNames', 'containsTrackNames', 'containsTrackPaths', 'containsRecordingPaths', 'containsSourceMusic', 'containsMicrophoneAudio'
    )) {
        if ($data.privacy.$property -isnot [bool] -or $data.privacy.$property -ne $false) {
            throw "privacy.$property must be the JSON boolean false."
        }
    }

    Write-Step "Qualification is complete and bound to $($fingerprint.sha256)."
    Write-Step "Source commit: $expectedCommit"
}

if (-not $ValidateExisting) {
    # Reject unattended generation before resolving or hashing any supplied candidate/evidence files.
    Assert-NotCi
    Assert-Windows11X64
}

$app = Resolve-Leaf -Path $AppPath -ExpectedName 'BrokeDJ.exe'
$probe = Resolve-Leaf -Path $ProbePath -ExpectedName 'BrokeDJ-device-probe.json'
$evidenceFiles = @{
    m1 = Resolve-Leaf -Path $M1EvidencePath -ExpectedName 'BrokeDJ-M1-Hardware-Witness.json'
    m2 = Resolve-Leaf -Path $M2EvidencePath -ExpectedName 'BrokeDJ-M2-keylock-listening.json'
    m3 = Resolve-Leaf -Path $M3EvidencePath -ExpectedName 'BrokeDJ-M3-mixer-recording.json'
    m4 = Resolve-Leaf -Path $M4EvidencePath -ExpectedName 'BrokeDJ-M4-Library-Witness.json'
}

$m1 = Resolve-WitnessScript -PackagedName 'M1-HARDWARE-WITNESS.ps1' -RepositoryName 'm1_hardware_witness.ps1'
$m2 = Resolve-WitnessScript -PackagedName 'M2-KEYLOCK-LISTENING-WITNESS.ps1' -RepositoryName 'm2_keylock_listening_witness.ps1'
$m3 = Resolve-WitnessScript -PackagedName 'M3-MIXER-RECORDING-WITNESS.ps1' -RepositoryName 'm3_mixer_recording_witness.ps1'
$m4 = Resolve-WitnessScript -PackagedName 'M4-LIBRARY-WITNESS.ps1' -RepositoryName 'm4_library_witness.ps1'

Invoke-WitnessValidator -ScriptPath $m1 -App $app -Evidence $evidenceFiles.m1 -Probe $probe
Invoke-WitnessValidator -ScriptPath $m2 -App $app -Evidence $evidenceFiles.m2
Invoke-WitnessValidator -ScriptPath $m3 -App $app -Evidence $evidenceFiles.m3
Invoke-WitnessValidator -ScriptPath $m4 -App $app -Evidence $evidenceFiles.m4

if ($ValidateExisting) {
    Read-Qualification -App $app -Probe $probe -EvidenceFiles $evidenceFiles -Path $QualificationPath
    exit 0
}

$qualification = [ordered]@{
    schema = 1
    project = 'BrokeDJ'
    channel = 'beta'
    generatedUtc = [DateTimeOffset]::UtcNow.ToString('o')
    sourceCommit = Get-SourceCommit -App $app
    app = Get-AppFingerprint -App $app
    gates = [ordered]@{
        m1 = $true
        m2 = $true
        m3 = $true
        m4 = $true
    }
    evidence = [ordered]@{
        deviceProbe = [ordered]@{ fileName = $probe.Name; sha256 = Get-Sha256 -File $probe }
        m1 = [ordered]@{ fileName = $evidenceFiles.m1.Name; sha256 = Get-Sha256 -File $evidenceFiles.m1 }
        m2 = [ordered]@{ fileName = $evidenceFiles.m2.Name; sha256 = Get-Sha256 -File $evidenceFiles.m2 }
        m3 = [ordered]@{ fileName = $evidenceFiles.m3.Name; sha256 = Get-Sha256 -File $evidenceFiles.m3 }
        m4 = [ordered]@{ fileName = $evidenceFiles.m4.Name; sha256 = Get-Sha256 -File $evidenceFiles.m4 }
    }
    privacy = [ordered]@{
        containsDeviceNames = $false
        containsTrackNames = $false
        containsTrackPaths = $false
        containsRecordingPaths = $false
        containsSourceMusic = $false
        containsMicrophoneAudio = $false
    }
}

$parent = Split-Path -Parent $QualificationPath
if ([string]::IsNullOrWhiteSpace($parent)) {
    $parent = (Get-Location).Path
} elseif (-not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}
$parent = (Resolve-Path -LiteralPath $parent).Path
$finalQualificationPath = Join-Path $parent ([System.IO.Path]::GetFileName($QualificationPath))
$temporaryQualificationPath = Join-Path $parent (
    '.BrokeDJ-Beta-Qualification-' + [Guid]::NewGuid().ToString('N') + '.tmp'
)

try {
    $qualification | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath $temporaryQualificationPath -Encoding utf8

    # Validate the exact bytes before publishing them. A failed generation leaves any
    # previously valid qualification file untouched.
    Read-Qualification -App $app -Probe $probe -EvidenceFiles $evidenceFiles -Path $temporaryQualificationPath
    Move-Item -LiteralPath $temporaryQualificationPath -Destination $finalQualificationPath -Force
    Read-Qualification -App $app -Probe $probe -EvidenceFiles $evidenceFiles -Path $finalQualificationPath
    Write-Step "Wrote privacy-safe beta qualification summary: $finalQualificationPath"
} catch {
    Write-Error $_
    exit 2
} finally {
    Remove-Item -LiteralPath $temporaryQualificationPath -Force -ErrorAction SilentlyContinue
}
