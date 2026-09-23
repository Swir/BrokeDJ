# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$AppPath,

    [string]$EvidencePath = ".\BrokeDJ-M3-mixer-recording.json",

    [switch]$ValidateExisting,

    [ValidateRange(0, 1440)]
    [int]$ContinuousSessionMinutes = 0,

    [ValidateRange(0, 64)]
    [int]$RepresentativeTrackCount = 0,

    [switch]$UserOwnedOrLicensed,
    [switch]$EqListeningReviewed,
    [switch]$GainStagingReviewed,
    [switch]$CrossfaderLawsReviewed,
    [switch]$LimiterBehaviorReviewed,
    [switch]$MicrophoneInputReviewed,
    [switch]$DuckingReviewed,
    [switch]$BoothRoutingReviewed,
    [switch]$CueIsolationPreserved,
    [switch]$RecordingCreated,
    [switch]$RecordingPlaybackReviewed,
    [switch]$ZeroRecordingDropoutsObserved,
    [switch]$RuntimeErrorReview
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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
        throw 'M3 mixer/recording evidence is human-controlled and cannot be generated in CI.'
    }
}

function Assert-ExactProperties {
    param(
        [Parameter(Mandatory = $true)] [object]$Object,
        [Parameter(Mandatory = $true)] [string[]]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Object) { throw "$Context is missing." }
    $actual = @($Object.PSObject.Properties.Name | Sort-Object)
    $expectedSorted = @($Expected | Sort-Object)
    if ($actual.Count -ne $expectedSorted.Count) {
        throw "$Context has an unexpected property count."
    }
    for ($i = 0; $i -lt $actual.Count; ++$i) {
        if ($actual[$i] -cne $expectedSorted[$i]) {
            throw "$Context contains unexpected or missing properties."
        }
    }
}

function Assert-Boolean {
    param([object]$Value, [string]$Context)
    if ($Value -isnot [bool]) { throw "$Context must be a JSON boolean." }
}

function Assert-Integer {
    param([object]$Value, [string]$Context)
    if (($Value -isnot [int]) -and ($Value -isnot [long])) {
        throw "$Context must be a JSON integer."
    }
}

function Assert-String {
    param([object]$Value, [string]$Context)
    if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace($Value)) {
        throw "$Context must be a non-empty JSON string."
    }
}

function Assert-Timestamp {
    param([object]$Value)
    if ($Value -is [string]) {
        try { [void][datetimeoffset]::Parse($Value) }
        catch { throw 'generatedUtc is not a valid timestamp.' }
    } elseif ($Value -isnot [datetime] -and $Value -isnot [datetimeoffset]) {
        throw 'generatedUtc must be an ISO timestamp.'
    }
}

function Get-AppIdentity {
    param([string]$Path)

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $file = Get-Item -LiteralPath $resolved.Path -ErrorAction Stop
    if ($file.PSIsContainer -or $file.Name -cne 'BrokeDJ.exe') {
        throw 'The selected application must be BrokeDJ.exe.'
    }

    $version = $file.VersionInfo.FileVersion
    if ([string]::IsNullOrWhiteSpace($version)) { $version = 'unknown' }
    return [ordered]@{
        fileName = $file.Name
        fileVersion = [string]$version
        sha256 = (Get-FileHash -LiteralPath $resolved.Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Get-WindowsEnvironment {
    $isWindowsHost = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
        [System.Runtime.InteropServices.OSPlatform]::Windows)
    if (-not $isWindowsHost) {
        throw 'M3 mixer/recording evidence must be recorded on Windows 11 x64.'
    }

    $architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToUpperInvariant()
    $processArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString().ToUpperInvariant()
    $windowsBuild = [System.Environment]::OSVersion.Version.Build
    if ($windowsBuild -lt 22000 -or $architecture -cne 'X64' -or $processArchitecture -cne 'X64') {
        throw 'M3 mixer/recording evidence requires Windows 11 x64 with x64 PowerShell.'
    }

    return [ordered]@{
        windowsBuild = [int]$windowsBuild
        architecture = $architecture
        processArchitecture = $processArchitecture
    }
}

function Test-Witness {
    param(
        [Parameter(Mandatory = $true)] [object]$Evidence,
        [Parameter(Mandatory = $true)] [object]$ExpectedApp
    )

    Assert-ExactProperties $Evidence @(
        'schema', 'project', 'scope', 'generatedUtc', 'environment',
        'app', 'session', 'checks', 'privacy'
    ) 'evidence'

    Assert-Integer $Evidence.schema 'schema'
    if ([int64]$Evidence.schema -ne 1) { throw 'Unsupported M3 witness schema.' }
    Assert-String $Evidence.project 'project'
    Assert-String $Evidence.scope 'scope'
    if ($Evidence.project -cne 'BrokeDJ' -or $Evidence.scope -cne 'M3-mixer-recording') {
        throw 'Evidence project or scope does not match the M3 mixer/recording witness.'
    }
    Assert-Timestamp $Evidence.generatedUtc

    Assert-ExactProperties $Evidence.environment @(
        'windowsBuild', 'architecture', 'processArchitecture'
    ) 'environment'
    Assert-Integer $Evidence.environment.windowsBuild 'environment.windowsBuild'
    Assert-String $Evidence.environment.architecture 'environment.architecture'
    Assert-String $Evidence.environment.processArchitecture 'environment.processArchitecture'
    if ([int64]$Evidence.environment.windowsBuild -lt 22000) {
        throw 'This witness requires a Windows 11-class build.'
    }
    if ($Evidence.environment.architecture -cne 'X64' -or
        $Evidence.environment.processArchitecture -cne 'X64') {
        throw 'This witness requires Windows x64 and an x64 PowerShell process.'
    }

    Assert-ExactProperties $Evidence.app @('fileName', 'fileVersion', 'sha256') 'app'
    Assert-String $Evidence.app.fileName 'app.fileName'
    Assert-String $Evidence.app.fileVersion 'app.fileVersion'
    Assert-String $Evidence.app.sha256 'app.sha256'
    if ($Evidence.app.sha256 -notmatch '^[0-9a-f]{64}$') {
        throw 'app.sha256 is not a lowercase SHA-256 digest.'
    }
    if ($Evidence.app.fileName -cne $ExpectedApp.fileName -or
        $Evidence.app.fileVersion -cne $ExpectedApp.fileVersion -or
        $Evidence.app.sha256 -cne $ExpectedApp.sha256) {
        throw 'Evidence is not bound to the selected BrokeDJ.exe.'
    }

    Assert-ExactProperties $Evidence.session @(
        'continuousMinutes', 'representativeTrackCount', 'userOwnedOrLicensed'
    ) 'session'
    Assert-Integer $Evidence.session.continuousMinutes 'session.continuousMinutes'
    Assert-Integer $Evidence.session.representativeTrackCount 'session.representativeTrackCount'
    Assert-Boolean $Evidence.session.userOwnedOrLicensed 'session.userOwnedOrLicensed'
    if ([int64]$Evidence.session.continuousMinutes -lt 60) {
        throw 'M3 qualification requires at least 60 continuous minutes.'
    }
    if ([int64]$Evidence.session.representativeTrackCount -lt 3) {
        throw 'At least three representative tracks are required.'
    }
    if ($Evidence.session.userOwnedOrLicensed -ne $true) {
        throw 'Qualification material must be user-owned or otherwise licensed for testing.'
    }

    $checkNames = @(
        'eqListeningReviewed',
        'gainStagingReviewed',
        'crossfaderLawsReviewed',
        'limiterBehaviorReviewed',
        'microphoneInputReviewed',
        'duckingReviewed',
        'boothRoutingReviewed',
        'cueIsolationPreserved',
        'recordingCreated',
        'recordingPlaybackReviewed',
        'zeroRecordingDropoutsObserved',
        'runtimeErrorReview'
    )
    Assert-ExactProperties $Evidence.checks $checkNames 'checks'
    foreach ($name in $checkNames) {
        Assert-Boolean $Evidence.checks.$name "checks.$name"
        if ($Evidence.checks.$name -ne $true) {
            throw "M3 mixer/recording witness is incomplete: checks.$name is false."
        }
    }

    $privacyNames = @(
        'containsDeviceNames',
        'containsTrackPaths',
        'containsTrackNames',
        'containsRecordingPaths',
        'containsSourceMusic',
        'containsMicrophoneAudio'
    )
    Assert-ExactProperties $Evidence.privacy $privacyNames 'privacy'
    foreach ($name in $privacyNames) {
        Assert-Boolean $Evidence.privacy.$name "privacy.$name"
        if ($Evidence.privacy.$name -ne $false) {
            throw 'Privacy-unsafe M3 qualification evidence was rejected.'
        }
    }

    return $true
}

$temporaryEvidencePath = $null
try {
    if ($ValidateExisting) {
        $app = Get-AppIdentity -Path $AppPath
        if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) {
            throw 'Evidence file does not exist.'
        }
        $evidence = Get-Content -LiteralPath $EvidencePath -Raw -ErrorAction Stop | ConvertFrom-Json
        [void](Test-Witness -Evidence $evidence -ExpectedApp $app)
        Write-Host 'M3 mixer/recording evidence: VALID'
        exit 0
    }

    # Refuse unattended generation before resolving or hashing AppPath.
    Assert-NotCi
    $app = Get-AppIdentity -Path $AppPath
    $environment = Get-WindowsEnvironment

    $evidence = [ordered]@{
        schema = 1
        project = 'BrokeDJ'
        scope = 'M3-mixer-recording'
        generatedUtc = [DateTimeOffset]::UtcNow.ToString('o')
        environment = $environment
        app = $app
        session = [ordered]@{
            continuousMinutes = $ContinuousSessionMinutes
            representativeTrackCount = $RepresentativeTrackCount
            userOwnedOrLicensed = [bool]$UserOwnedOrLicensed
        }
        checks = [ordered]@{
            eqListeningReviewed = [bool]$EqListeningReviewed
            gainStagingReviewed = [bool]$GainStagingReviewed
            crossfaderLawsReviewed = [bool]$CrossfaderLawsReviewed
            limiterBehaviorReviewed = [bool]$LimiterBehaviorReviewed
            microphoneInputReviewed = [bool]$MicrophoneInputReviewed
            duckingReviewed = [bool]$DuckingReviewed
            boothRoutingReviewed = [bool]$BoothRoutingReviewed
            cueIsolationPreserved = [bool]$CueIsolationPreserved
            recordingCreated = [bool]$RecordingCreated
            recordingPlaybackReviewed = [bool]$RecordingPlaybackReviewed
            zeroRecordingDropoutsObserved = [bool]$ZeroRecordingDropoutsObserved
            runtimeErrorReview = [bool]$RuntimeErrorReview
        }
        privacy = [ordered]@{
            containsDeviceNames = $false
            containsTrackPaths = $false
            containsTrackNames = $false
            containsRecordingPaths = $false
            containsSourceMusic = $false
            containsMicrophoneAudio = $false
        }
    }

    $evidenceParent = Split-Path -Parent $EvidencePath
    if ([string]::IsNullOrWhiteSpace($evidenceParent)) {
        $evidenceParent = (Get-Location).Path
    } elseif (-not (Test-Path -LiteralPath $evidenceParent)) {
        New-Item -ItemType Directory -Force -Path $evidenceParent | Out-Null
    }
    $evidenceParent = (Resolve-Path -LiteralPath $evidenceParent).Path
    $leaf = [System.IO.Path]::GetFileName($EvidencePath)
    if ([string]::IsNullOrWhiteSpace($leaf)) {
        throw 'EvidencePath must include a file name.'
    }
    $finalEvidencePath = Join-Path $evidenceParent $leaf
    $temporaryEvidencePath = Join-Path $evidenceParent ('.BrokeDJ-M3-Witness-' + [Guid]::NewGuid().ToString('N') + '.tmp')

    $evidence | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporaryEvidencePath -Encoding utf8
    try {
        $candidate = Get-Content -LiteralPath $temporaryEvidencePath -Raw -ErrorAction Stop | ConvertFrom-Json
        [void](Test-Witness -Evidence $candidate -ExpectedApp $app)
    } catch {
        Write-Warning ('M3 mixer/recording witness is incomplete; accepted evidence was not replaced. ' + $_.Exception.Message)
        exit 2
    }

    Move-Item -LiteralPath $temporaryEvidencePath -Destination $finalEvidencePath -Force
    $temporaryEvidencePath = $null
    $published = Get-Content -LiteralPath $finalEvidencePath -Raw -ErrorAction Stop | ConvertFrom-Json
    [void](Test-Witness -Evidence $published -ExpectedApp $app)
    Write-Host "M3 mixer/recording evidence written and complete: $finalEvidencePath"
    exit 0
} catch {
    Write-Error $_.Exception.Message
    exit 1
} finally {
    if (-not [string]::IsNullOrWhiteSpace([string]$temporaryEvidencePath)) {
        Remove-Item -LiteralPath $temporaryEvidencePath -Force -ErrorAction SilentlyContinue
    }
}
