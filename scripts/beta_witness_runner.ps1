# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [string]$AppPath = '.\BrokeDJ.exe',
    [string]$EvidenceDirectory = '.',
    [switch]$StatusOnly,
    [switch]$ValidateExisting,
    [switch]$FixtureSelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string]$Message) {
    Write-Host "[BrokeDJ Beta Runner] $Message"
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

function Assert-HumanGenerationAllowed {
    if ((Test-TruthyEnvironmentValue $env:GITHUB_ACTIONS) -or
        (Test-TruthyEnvironmentValue $env:CI)) {
        throw 'Human-controlled Beta witness generation cannot run in CI.'
    }
}

function Assert-Windows11X64 {
    if ($env:OS -ne 'Windows_NT') {
        throw 'The Beta witness runner requires Windows 11.'
    }
    $build = [Environment]::OSVersion.Version.Build
    if ($build -lt 22000) {
        throw "Windows 11 build 22000 or newer is required (detected $build)."
    }
    $osArch = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    $processArch = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
    if ($osArch -ne 'X64' -or $processArch -ne 'X64') {
        throw "x64 Windows and x64 PowerShell are required (OS=$osArch, process=$processArch)."
    }
}

function Resolve-App([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $item = Get-Item -LiteralPath $resolved
    if ($item.PSIsContainer -or $item.Name -cne 'BrokeDJ.exe') {
        throw 'AppPath must point to BrokeDJ.exe.'
    }
    return $item
}

function Resolve-EvidenceDirectory([string]$Path, [bool]$Create) {
    if (-not (Test-Path -LiteralPath $Path)) {
        if (-not $Create) {
            throw "Evidence directory does not exist: $Path"
        }
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $item = Get-Item -LiteralPath $resolved
    if (-not $item.PSIsContainer) {
        throw 'EvidenceDirectory must be a directory.'
    }
    return $item.FullName
}

function Resolve-ToolScript(
    [string]$AppDirectory,
    [string]$PackagedName,
    [string]$RepositoryName
) {
    $candidates = @(
        (Join-Path $AppDirectory $PackagedName),
        (Join-Path $PSScriptRoot $RepositoryName)
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Required witness tool is missing: $PackagedName / $RepositoryName"
}

function Resolve-Guide(
    [string]$AppDirectory,
    [string]$PackagedName,
    [string]$RepositoryRelativePath
) {
    $packaged = Join-Path $AppDirectory $PackagedName
    if (Test-Path -LiteralPath $packaged -PathType Leaf) {
        return (Resolve-Path -LiteralPath $packaged).Path
    }
    $repositoryRoot = Split-Path -Parent $PSScriptRoot
    $candidate = Join-Path $repositoryRoot $RepositoryRelativePath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }
    return $null
}

function Get-PowerShellHostPath {
    $hostName = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh.exe' } else { 'powershell.exe' }
    $candidate = Join-Path $PSHOME $hostName
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Unable to locate the current PowerShell host: $candidate"
    }
    return (Resolve-Path -LiteralPath $candidate).Path
}

function Invoke-ChildScript([string]$ScriptPath, [string[]]$Arguments) {
    $hostPath = Get-PowerShellHostPath
    $allArguments = @('-NoProfile', '-File', $ScriptPath) + $Arguments
    & $hostPath @allArguments
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "Tool failed with exit code $code: $([System.IO.Path]::GetFileName($ScriptPath))"
    }
}

function Get-EvidencePaths([string]$Directory) {
    return [ordered]@{
        Probe = Join-Path $Directory 'BrokeDJ-device-probe.json'
        M1 = Join-Path $Directory 'BrokeDJ-M1-Hardware-Witness.json'
        M2 = Join-Path $Directory 'BrokeDJ-M2-keylock-listening.json'
        M3 = Join-Path $Directory 'BrokeDJ-M3-mixer-recording.json'
        M4 = Join-Path $Directory 'BrokeDJ-M4-Library-Witness.json'
        Qualification = Join-Path $Directory 'BrokeDJ-Beta-Qualification.json'
    }
}

function Read-YesNo([string]$Question) {
    while ($true) {
        $answer = (Read-Host "$Question [y/n]").Trim().ToLowerInvariant()
        if ($answer -eq 'y' -or $answer -eq 'yes') { return $true }
        if ($answer -eq 'n' -or $answer -eq 'no') { return $false }
        Write-Host 'Please answer y or n.'
    }
}

function Read-IntegerAtLeast([string]$Question, [int]$Minimum) {
    while ($true) {
        $raw = (Read-Host "$Question (minimum $Minimum)").Trim()
        $value = 0
        if ([int]::TryParse($raw, [ref]$value) -and $value -ge $Minimum) {
            return $value
        }
        Write-Host "Enter an integer greater than or equal to $Minimum."
    }
}

function Assert-AllConfirmations([System.Collections.IDictionary]$Answers, [string]$Scope) {
    foreach ($entry in $Answers.GetEnumerator()) {
        if (-not [bool]$entry.Value) {
            throw "$Scope was not attested completely; no new accepted evidence will be published by the underlying witness tool."
        }
    }
}

function Test-ProbeContract([string]$ProbePath) {
    if (-not (Test-Path -LiteralPath $ProbePath -PathType Leaf)) { return $false }
    try {
        $data = Get-Content -LiteralPath $ProbePath -Raw | ConvertFrom-Json
        if ($data.schema_version -ne 2) { return $false }
        if ($data.plays_audio -ne $false) { return $false }
        if ($data.calls_device_open -ne $false) { return $false }
        if ($data.starts_audio_callback -ne $false) { return $false }
        if ($data.safety_invariants_ok -ne $true) { return $false }
        return $true
    } catch {
        return $false
    }
}

function Ensure-SilentDeviceProbe(
    [System.IO.FileInfo]$App,
    [string]$Directory,
    [string]$ProbePath
) {
    if (Test-ProbeContract -ProbePath $ProbePath) {
        Write-Step 'Existing full device probe passes the non-opening safety contract.'
        return
    }

    Write-Step 'Generating the full silent device-capability probe; this does not open an audio device or start playback.'
    $textPath = Join-Path $Directory 'BrokeDJ-device-probe.txt'
    Remove-Item -LiteralPath $ProbePath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $textPath -Force -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $App.FullName -ArgumentList '--device-probe' `
        -WorkingDirectory $Directory -PassThru
    if (-not $process.WaitForExit(30000)) {
        try { $process.Kill() } catch { }
        throw 'BrokeDJ --device-probe did not finish within 30 seconds.'
    }
    if ($process.ExitCode -ne 0) {
        throw "BrokeDJ --device-probe failed with exit code $($process.ExitCode)."
    }
    if (-not (Test-ProbeContract -ProbePath $ProbePath)) {
        throw 'The generated device probe did not satisfy the non-opening safety contract.'
    }
}

function Test-Witness(
    [string]$ScriptPath,
    [System.IO.FileInfo]$App,
    [string]$EvidencePath,
    [string]$ProbePath = ''
) {
    if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) { return $false }
    $arguments = @('-AppPath', $App.FullName, '-EvidencePath', $EvidencePath, '-ValidateExisting')
    if (-not [string]::IsNullOrWhiteSpace($ProbePath)) {
        if (-not (Test-Path -LiteralPath $ProbePath -PathType Leaf)) { return $false }
        $arguments += @('-ProbePath', $ProbePath)
    }
    try {
        Invoke-ChildScript -ScriptPath $ScriptPath -Arguments $arguments
        return $true
    } catch {
        Write-Warning $_.Exception.Message
        return $false
    }
}

function Invoke-M1(
    [string]$ScriptPath,
    [System.IO.FileInfo]$App,
    [System.Collections.IDictionary]$Paths
) {
    if (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M1 -ProbePath $Paths.Probe) {
        Write-Step 'M1 hardware witness is already valid for this executable.'
        return
    }
    Write-Step 'Starting M1 human hardware witness. The existing witness owns all manual questions and keeps audio actions user-controlled.'
    Invoke-ChildScript -ScriptPath $ScriptPath -Arguments @(
        '-AppPath', $App.FullName,
        '-ProbePath', $Paths.Probe,
        '-EvidencePath', $Paths.M1
    )
    if (-not (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M1 -ProbePath $Paths.Probe)) {
        throw 'M1 witness did not validate after generation.'
    }
}

function Invoke-M2(
    [string]$ScriptPath,
    [string]$GuidePath,
    [System.IO.FileInfo]$App,
    [System.Collections.IDictionary]$Paths
) {
    if (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M2) {
        Write-Step 'M2 key-lock listening witness is already valid for this executable.'
        return
    }

    Write-Step 'M2 requires a human listening review. The runner never starts playback or changes volume.'
    if (-not [string]::IsNullOrWhiteSpace($GuidePath)) {
        Write-Host "Guide: $GuidePath"
    }
    Write-Host "Normal baseline command: & '$($App.FullName)'"
    Write-Host "Key-lock research command: & '$($App.FullName)' --key-lock-research"
    [void](Read-Host 'Complete the documented A/B listening procedure with conservative hardware volume, close BrokeDJ, then press Enter')

    $trackCount = Read-IntegerAtLeast -Question 'How many representative user-owned/licensed tracks were reviewed?' -Minimum 3
    $answers = [ordered]@{
        UserOwnedOrLicensed = Read-YesNo 'Were all reviewed tracks user-owned or otherwise licensed for testing?'
        NormalBaselineReviewed = Read-YesNo 'Was normal playback reviewed as the baseline?'
        SlowKeyLockReviewed = Read-YesNo 'Was key lock reviewed at a slower deck rate?'
        FastKeyLockReviewed = Read-YesNo 'Was key lock reviewed at a faster deck rate?'
        TransportFallbackReviewed = Read-YesNo 'Were seek/loop/rate-change fallback and paused restage reviewed?'
        PitchStabilityAcceptable = Read-YesNo 'Was pitch stability acceptable on the reviewed material?'
        NoCriticalArtifactsObserved = Read-YesNo 'Were no critical audible artifacts observed?'
        RuntimeErrorReview = Read-YesNo 'Was the BrokeDJ runtime log reviewed for unexpected key-lock errors?'
    }
    Assert-AllConfirmations -Answers $answers -Scope 'M2 listening review'

    Invoke-ChildScript -ScriptPath $ScriptPath -Arguments @(
        '-AppPath', $App.FullName,
        '-EvidencePath', $Paths.M2,
        '-RepresentativeTrackCount', [string]$trackCount,
        '-UserOwnedOrLicensed',
        '-NormalBaselineReviewed',
        '-SlowKeyLockReviewed',
        '-FastKeyLockReviewed',
        '-TransportFallbackReviewed',
        '-PitchStabilityAcceptable',
        '-NoCriticalArtifactsObserved',
        '-RuntimeErrorReview'
    )
    if (-not (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M2)) {
        throw 'M2 witness did not validate after generation.'
    }
}

function Invoke-M3(
    [string]$ScriptPath,
    [string]$GuidePath,
    [System.IO.FileInfo]$App,
    [System.Collections.IDictionary]$Paths
) {
    if (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M3) {
        Write-Step 'M3 mixer/recording witness is already valid for this executable.'
        return
    }

    Write-Step 'M3 requires a user-controlled mixer/recording/listening session. The runner never starts playback, microphone input or recording.'
    if (-not [string]::IsNullOrWhiteSpace($GuidePath)) {
        Write-Host "Guide: $GuidePath"
    }
    [void](Read-Host 'Complete the documented M3 session with conservative hardware volume, close BrokeDJ, then press Enter')

    $minutes = Read-IntegerAtLeast -Question 'How many continuous minutes were qualified?' -Minimum 60
    $trackCount = Read-IntegerAtLeast -Question 'How many representative user-owned/licensed tracks were used?' -Minimum 3
    $answers = [ordered]@{
        UserOwnedOrLicensed = Read-YesNo 'Were all source tracks user-owned or otherwise licensed for testing?'
        EqListeningReviewed = Read-YesNo 'Was EQ behavior reviewed by listening?'
        GainStagingReviewed = Read-YesNo 'Was gain staging reviewed?'
        CrossfaderLawsReviewed = Read-YesNo 'Were the crossfader laws reviewed?'
        LimiterBehaviorReviewed = Read-YesNo 'Was the output safety/limiter behavior reviewed without calling it transparent?'
        MicrophoneInputReviewed = Read-YesNo 'Was microphone input reviewed on the intended hardware?'
        DuckingReviewed = Read-YesNo 'Was microphone ducking reviewed?'
        BoothRoutingReviewed = Read-YesNo 'Was Booth routing reviewed where supported?'
        CueIsolationPreserved = Read-YesNo 'Was private Cue isolation preserved on the intended outputs?'
        RecordingCreated = Read-YesNo 'Was a set recording created?'
        RecordingPlaybackReviewed = Read-YesNo 'Was the resulting recording played back and reviewed?'
        ZeroRecordingDropoutsObserved = Read-YesNo 'Were zero recording dropouts observed for the qualified session?'
        RuntimeErrorReview = Read-YesNo 'Was the runtime log reviewed for unexpected mixer/recording errors?'
    }
    Assert-AllConfirmations -Answers $answers -Scope 'M3 mixer/recording review'

    Invoke-ChildScript -ScriptPath $ScriptPath -Arguments @(
        '-AppPath', $App.FullName,
        '-EvidencePath', $Paths.M3,
        '-ContinuousSessionMinutes', [string]$minutes,
        '-RepresentativeTrackCount', [string]$trackCount,
        '-UserOwnedOrLicensed',
        '-EqListeningReviewed',
        '-GainStagingReviewed',
        '-CrossfaderLawsReviewed',
        '-LimiterBehaviorReviewed',
        '-MicrophoneInputReviewed',
        '-DuckingReviewed',
        '-BoothRoutingReviewed',
        '-CueIsolationPreserved',
        '-RecordingCreated',
        '-RecordingPlaybackReviewed',
        '-ZeroRecordingDropoutsObserved',
        '-RuntimeErrorReview'
    )
    if (-not (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M3)) {
        throw 'M3 witness did not validate after generation.'
    }
}

function Invoke-M4(
    [string]$ScriptPath,
    [System.IO.FileInfo]$App,
    [System.Collections.IDictionary]$Paths
) {
    if (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M4) {
        Write-Step 'M4 connected library/session witness is already valid for this executable.'
        return
    }
    Write-Step 'Starting the exact-process-bound M4 library/session witness. It launches only the selected BrokeDJ.exe and keeps all interactive actions human-controlled.'
    Invoke-ChildScript -ScriptPath $ScriptPath -Arguments @(
        '-AppPath', $App.FullName,
        '-EvidencePath', $Paths.M4
    )
    if (-not (Test-Witness -ScriptPath $ScriptPath -App $App -EvidencePath $Paths.M4)) {
        throw 'M4 witness did not validate after generation.'
    }
}

function Invoke-Qualification(
    [string]$ScriptPath,
    [System.IO.FileInfo]$App,
    [System.Collections.IDictionary]$Paths,
    [switch]$ExistingOnly
) {
    $arguments = @(
        '-AppPath', $App.FullName,
        '-ProbePath', $Paths.Probe,
        '-M1EvidencePath', $Paths.M1,
        '-M2EvidencePath', $Paths.M2,
        '-M3EvidencePath', $Paths.M3,
        '-M4EvidencePath', $Paths.M4,
        '-QualificationPath', $Paths.Qualification
    )
    if ($ExistingOnly) { $arguments += '-ValidateExisting' }
    Invoke-ChildScript -ScriptPath $ScriptPath -Arguments $arguments
}

function Invoke-FixtureSelfTest {
    if (-not (Test-TruthyEnvironmentValue '1') -or
        -not (Test-TruthyEnvironmentValue 'TRUE') -or
        -not (Test-TruthyEnvironmentValue ' yes ') -or
        -not (Test-TruthyEnvironmentValue 'On') -or
        (Test-TruthyEnvironmentValue '0') -or
        (Test-TruthyEnvironmentValue 'false') -or
        (Test-TruthyEnvironmentValue $null)) {
        throw 'Truthy environment parsing self-test failed.'
    }

    $paths = Get-EvidencePaths -Directory 'C:\BrokeDJ evidence'
    if ([System.IO.Path]::GetFileName($paths.Probe) -cne 'BrokeDJ-device-probe.json' -or
        [System.IO.Path]::GetFileName($paths.M1) -cne 'BrokeDJ-M1-Hardware-Witness.json' -or
        [System.IO.Path]::GetFileName($paths.M2) -cne 'BrokeDJ-M2-keylock-listening.json' -or
        [System.IO.Path]::GetFileName($paths.M3) -cne 'BrokeDJ-M3-mixer-recording.json' -or
        [System.IO.Path]::GetFileName($paths.M4) -cne 'BrokeDJ-M4-Library-Witness.json' -or
        [System.IO.Path]::GetFileName($paths.Qualification) -cne 'BrokeDJ-Beta-Qualification.json') {
        throw 'Canonical evidence file-name self-test failed.'
    }

    $required = @(
        @('M1-HARDWARE-WITNESS.ps1', 'm1_hardware_witness.ps1'),
        @('M2-KEYLOCK-LISTENING-WITNESS.ps1', 'm2_keylock_listening_witness.ps1'),
        @('M3-MIXER-RECORDING-WITNESS.ps1', 'm3_mixer_recording_witness.ps1'),
        @('M4-LIBRARY-WITNESS.ps1', 'm4_library_witness.ps1'),
        @('BETA-QUALIFICATION.ps1', 'beta_qualification.ps1')
    )
    foreach ($pair in $required) {
        $resolved = Resolve-ToolScript -AppDirectory $PSScriptRoot -PackagedName $pair[0] -RepositoryName $pair[1]
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Tool-resolution self-test failed: $($pair[1])"
        }
    }
    Write-Host 'Beta witness runner fixture self-test: PASS'
}

try {
    if ($FixtureSelfTest) {
        Invoke-FixtureSelfTest
        exit 0
    }

    if ($StatusOnly -and $ValidateExisting) {
        throw 'Use either -StatusOnly or -ValidateExisting, not both.'
    }

    # Generation is deliberately rejected before resolving or hashing AppPath.
    if (-not $StatusOnly -and -not $ValidateExisting) {
        Assert-HumanGenerationAllowed
    }
    Assert-Windows11X64

    $app = Resolve-App -Path $AppPath
    $evidenceDir = Resolve-EvidenceDirectory -Path $EvidenceDirectory -Create (-not $StatusOnly -and -not $ValidateExisting)
    $paths = Get-EvidencePaths -Directory $evidenceDir

    $m1Script = Resolve-ToolScript -AppDirectory $app.Directory.FullName -PackagedName 'M1-HARDWARE-WITNESS.ps1' -RepositoryName 'm1_hardware_witness.ps1'
    $m2Script = Resolve-ToolScript -AppDirectory $app.Directory.FullName -PackagedName 'M2-KEYLOCK-LISTENING-WITNESS.ps1' -RepositoryName 'm2_keylock_listening_witness.ps1'
    $m3Script = Resolve-ToolScript -AppDirectory $app.Directory.FullName -PackagedName 'M3-MIXER-RECORDING-WITNESS.ps1' -RepositoryName 'm3_mixer_recording_witness.ps1'
    $m4Script = Resolve-ToolScript -AppDirectory $app.Directory.FullName -PackagedName 'M4-LIBRARY-WITNESS.ps1' -RepositoryName 'm4_library_witness.ps1'
    $qualificationScript = Resolve-ToolScript -AppDirectory $app.Directory.FullName -PackagedName 'BETA-QUALIFICATION.ps1' -RepositoryName 'beta_qualification.ps1'

    $m2Guide = Resolve-Guide -AppDirectory $app.Directory.FullName -PackagedName 'M2-KEYLOCK-LISTENING-WITNESS.md' -RepositoryRelativePath 'docs/M2_KEYLOCK_LISTENING_WITNESS.md'
    $m3Guide = Resolve-Guide -AppDirectory $app.Directory.FullName -PackagedName 'M3-MIXER-RECORDING-WITNESS.md' -RepositoryRelativePath 'docs/M3_MIXER_RECORDING_WITNESS.md'

    if ($ValidateExisting) {
        Invoke-Qualification -ScriptPath $qualificationScript -App $app -Paths $paths -ExistingOnly
        Write-Step 'Existing first-Beta qualification is VALID for the exact executable and evidence set.'
        exit 0
    }

    if ($StatusOnly) {
        $probeValid = Test-ProbeContract -ProbePath $paths.Probe
        $m1Valid = Test-Witness -ScriptPath $m1Script -App $app -EvidencePath $paths.M1 -ProbePath $paths.Probe
        $m2Valid = Test-Witness -ScriptPath $m2Script -App $app -EvidencePath $paths.M2
        $m3Valid = Test-Witness -ScriptPath $m3Script -App $app -EvidencePath $paths.M3
        $m4Valid = Test-Witness -ScriptPath $m4Script -App $app -EvidencePath $paths.M4
        $qualificationValid = $false
        if ($probeValid -and $m1Valid -and $m2Valid -and $m3Valid -and $m4Valid -and
            (Test-Path -LiteralPath $paths.Qualification -PathType Leaf)) {
            try {
                Invoke-Qualification -ScriptPath $qualificationScript -App $app -Paths $paths -ExistingOnly
                $qualificationValid = $true
            } catch {
                Write-Warning $_.Exception.Message
            }
        }
        Write-Host ('Probe={0} M1={1} M2={2} M3={3} M4={4} Qualification={5}' -f `
            $probeValid, $m1Valid, $m2Valid, $m3Valid, $m4Valid, $qualificationValid)
        if ($qualificationValid) { exit 0 }
        exit 3
    }

    Write-Step "Qualification executable: $($app.FullName)"
    Write-Step "Evidence directory: $evidenceDir"
    Ensure-SilentDeviceProbe -App $app -Directory $evidenceDir -ProbePath $paths.Probe
    Invoke-M1 -ScriptPath $m1Script -App $app -Paths $paths
    Invoke-M2 -ScriptPath $m2Script -GuidePath $m2Guide -App $app -Paths $paths
    Invoke-M3 -ScriptPath $m3Script -GuidePath $m3Guide -App $app -Paths $paths
    Invoke-M4 -ScriptPath $m4Script -App $app -Paths $paths

    Write-Step 'All M1-M4 witness files validate against the exact executable. Creating the composed first-Beta qualification summary.'
    Invoke-Qualification -ScriptPath $qualificationScript -App $app -Paths $paths
    Invoke-Qualification -ScriptPath $qualificationScript -App $app -Paths $paths -ExistingOnly
    Write-Step 'FIRST BETA MANUAL QUALIFICATION: COMPLETE for this exact candidate. Public release gates still apply.'
    exit 0
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
