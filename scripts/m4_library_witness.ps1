# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$AppPath,

    [string]$EvidencePath = (Join-Path (Get-Location) 'BrokeDJ-M4-Library-Witness.json'),

    [switch]$ValidateExisting,

    [switch]$FixtureSelfTest
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
        throw 'M4 witness evidence is human-controlled and cannot be generated in CI.'
    }
}

function Assert-Windows11X64 {
    if ($env:OS -ne 'Windows_NT') {
        throw 'This witness must run on Windows.'
    }
    $build = [Environment]::OSVersion.Version.Build
    if ($build -lt 22000) {
        throw "Windows 11 is required for this witness (detected build $build)."
    }
    $osArch = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    $processArch = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
    if ($osArch -ne 'X64' -or $processArch -ne 'X64') {
        throw "M4 witness generation requires x64 Windows and x64 PowerShell (OS=$osArch, process=$processArch)."
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

function New-WitnessFixtureWorkspace {
    $root = Join-Path ([System.IO.Path]::GetTempPath()) ("BrokeDJ-M4-fixture-" + [Guid]::NewGuid().ToString('N'))
    $importDirectory = Join-Path $root 'import'
    $duplicateDirectory = Join-Path $root 'duplicate'
    $relocatedDirectory = Join-Path $root 'relocated'
    New-Item -ItemType Directory -Path $importDirectory,$duplicateDirectory,$relocatedDirectory -Force | Out-Null

    $sourcePath = Join-Path $importDirectory 'BrokeDJ-M4-Fixture.wav'
    $duplicatePath = Join-Path $duplicateDirectory 'BrokeDJ-M4-Fixture-copy.wav'

    $sampleRate = 48000
    $channels = 2
    $bitsPerSample = 16
    $frameCount = 48000
    $bytesPerSample = [int]($bitsPerSample / 8)
    $blockAlign = $channels * $bytesPerSample
    $dataSize = $frameCount * $blockAlign
    $byteRate = $sampleRate * $blockAlign
    $stream = [System.IO.File]::Open($sourcePath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $writer = [System.IO.BinaryWriter]::new($stream)

    try {
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([int](36 + $dataSize))
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('WAVE'))
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('fmt '))
        $writer.Write([int]16)
        $writer.Write([int16]1)
        $writer.Write([int16]$channels)
        $writer.Write([int]$sampleRate)
        $writer.Write([int]$byteRate)
        $writer.Write([int16]$blockAlign)
        $writer.Write([int16]$bitsPerSample)
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('data'))
        $writer.Write([int]$dataSize)

        $fadeFrames = 480
        for ($frame = 0; $frame -lt $frameCount; ++$frame) {
            $fadeIn = [Math]::Min(1.0, [double]$frame / [double]$fadeFrames)
            $fadeOut = [Math]::Min(1.0, [double]($frameCount - 1 - $frame) / [double]$fadeFrames)
            $fade = [Math]::Min($fadeIn, $fadeOut)
            $angle = 2.0 * [Math]::PI * 440.0 * [double]$frame / [double]$sampleRate
            $sample = [int16][Math]::Round([Math]::Sin($angle) * 4096.0 * $fade)
            $writer.Write($sample)
            $writer.Write($sample)
        }
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }

    Copy-Item -LiteralPath $sourcePath -Destination $duplicatePath

    $readmePath = Join-Path $root 'README.txt'
    @(
        'BrokeDJ M4 disposable fixture workspace',
        '',
        '1. Import the WAV under import\ into BrokeDJ.',
        '2. Import the byte-identical WAV under duplicate\ to exercise duplicate review.',
        '3. For the relocate check, move the primary WAV from import\ into relocated\ while BrokeDJ is running, refresh missing-file review, then relocate the existing library record to the moved copy.',
        '4. The witness script removes this entire workspace when the run ends.',
        '',
        'These generated files contain no user music or private library metadata.'
    ) | Set-Content -LiteralPath $readmePath -Encoding utf8

    return [pscustomobject]@{
        Root = $root
        Source = $sourcePath
        Duplicate = $duplicatePath
        RelocatedDirectory = $relocatedDirectory
        Readme = $readmePath
    }
}

function Test-WitnessFixtureWorkspace([object]$Workspace) {
    foreach ($path in @($Workspace.Source, $Workspace.Duplicate, $Workspace.Readme)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Fixture workspace is missing required file: $path"
        }
    }
    if (-not (Test-Path -LiteralPath $Workspace.RelocatedDirectory -PathType Container)) {
        throw 'Fixture workspace is missing the relocation directory.'
    }

    $sourceHash = (Get-FileHash -LiteralPath $Workspace.Source -Algorithm SHA256).Hash
    $duplicateHash = (Get-FileHash -LiteralPath $Workspace.Duplicate -Algorithm SHA256).Hash
    if ($sourceHash -ne $duplicateHash) {
        throw 'Fixture duplicate is not byte-identical to the primary WAV.'
    }

    $bytes = [System.IO.File]::ReadAllBytes($Workspace.Source)
    if ($bytes.Length -ne 192044) {
        throw "Fixture WAV length is unexpected: $($bytes.Length) bytes."
    }
    if ([System.Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'RIFF' -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4) -ne 'WAVE' -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 12, 4) -ne 'fmt ' -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 36, 4) -ne 'data') {
        throw 'Fixture WAV RIFF/WAVE structure is invalid.'
    }

    $formatTag = [BitConverter]::ToInt16($bytes, 20)
    $channels = [BitConverter]::ToInt16($bytes, 22)
    $sampleRate = [BitConverter]::ToInt32($bytes, 24)
    $bitsPerSample = [BitConverter]::ToInt16($bytes, 34)
    $dataSize = [BitConverter]::ToInt32($bytes, 40)
    if ($formatTag -ne 1 -or $channels -ne 2 -or $sampleRate -ne 48000 -or
        $bitsPerSample -ne 16 -or $dataSize -ne 192000) {
        throw 'Fixture WAV format is not the expected 48 kHz stereo 16-bit PCM contract.'
    }
}

function Remove-WitnessFixtureWorkspace([object]$Workspace) {
    if ($null -ne $Workspace -and -not [string]::IsNullOrWhiteSpace([string]$Workspace.Root)) {
        Remove-Item -LiteralPath $Workspace.Root -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-GuiSmokePreflight([System.IO.FileInfo]$App) {
    $temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BrokeDJ-M4-preflight-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    $process = $null
    try {
        Write-Step 'Running the packaged no-audio GUI/resize preflight before manual M4 checks.'
        $process = Start-Process -FilePath $App.FullName -ArgumentList '--smoke-test' `
            -WorkingDirectory $temporaryRoot -PassThru
        if (-not $process.WaitForExit(30000)) {
            try { $process.Kill() } catch { }
            throw 'BrokeDJ --smoke-test did not finish within 30 seconds.'
        }
        if ($process.ExitCode -ne 0) {
            throw "BrokeDJ --smoke-test failed with exit code $($process.ExitCode)."
        }

        $reportPath = Join-Path $temporaryRoot 'BrokeDJ-gui-smoke.json'
        if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
            throw 'BrokeDJ --smoke-test exited without BrokeDJ-gui-smoke.json.'
        }
        $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
        $schema = Assert-IntegerProperty -Object $report -Name 'schema_version' -Minimum 1 -Context 'guiSmoke'
        if ($schema -ne 1) { throw 'Unsupported GUI smoke schema.' }
        $mode = Assert-StringProperty -Object $report -Name 'mode' -Context 'guiSmoke'
        if ($mode -ne 'native-resize-lifecycle') { throw 'Unexpected GUI smoke mode.' }
        Assert-BooleanProperty -Object $report -Name 'plays_audio' -Expected $false -Context 'guiSmoke'
        Assert-BooleanProperty -Object $report -Name 'opens_audio_device' -Expected $false -Context 'guiSmoke'
        Assert-BooleanProperty -Object $report -Name 'success' -Expected $true -Context 'guiSmoke'
        $workstationSteps = Assert-IntegerProperty -Object $report -Name 'workstation_step_count' -Minimum 1 -Context 'guiSmoke'
        Write-Step "No-audio GUI preflight passed ($workstationSteps workstation resize step(s))."
    } finally {
        if ($null -ne $process -and -not $process.HasExited) {
            try { $process.Kill() } catch { }
        }
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Validate-Evidence([string]$Path, [System.IO.FileInfo]$ExpectedApp) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $data = Get-Content -LiteralPath $resolved -Raw | ConvertFrom-Json

    Assert-ExactProperties -Object $data -Expected @(
        'schema', 'project', 'scope', 'generatedUtc', 'environment', 'app', 'checks', 'privacy'
    ) -Context 'evidence'

    $schema = Assert-IntegerProperty -Object $data -Name 'schema' -Minimum 1 -Context 'evidence'
    if ($schema -ne 1) { throw 'Unsupported M4 witness schema.' }
    $project = Assert-StringProperty -Object $data -Name 'project' -Context 'evidence'
    $scope = Assert-StringProperty -Object $data -Name 'scope' -Context 'evidence'
    if ($project -ne 'BrokeDJ' -or $scope -ne 'M4-library-workflow') {
        throw 'Evidence file is not a BrokeDJ M4 library witness.'
    }

    # PowerShell 7 can materialize ISO-8601 JSON strings as DateTime while
    # Windows PowerShell commonly leaves them as strings. Accept either
    # representation but still require a parseable timestamp value.
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
        throw 'M4 witness evidence must come from an x64 OS and x64 PowerShell process.'
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

    $requiredChecks = @(
        'launchAndResize',
        'importAndSearch',
        'tagsAndPlaylists',
        'history',
        'duplicateAndMissingReview',
        'relocate',
        'libraryBackupRestore',
        'sessionSaveLoad'
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

    $privacyProperties = @('containsTrackPaths', 'containsTrackNames', 'containsSourceMusic')
    Assert-ExactProperties -Object $data.privacy -Expected $privacyProperties -Context 'privacy'
    foreach ($name in $privacyProperties) {
        Assert-BooleanProperty -Object $data.privacy -Name $name -Expected $false -Context 'privacy'
    }

    Write-Step "Witness is complete and bound to AppPath: $resolved"
    Write-Step "App SHA-256: $($expectedFingerprint.sha256)"
    Write-Step "Windows build: $windowsBuild"
}

if ($FixtureSelfTest) {
    $fixture = $null
    $fixtureRoot = $null
    try {
        $fixture = New-WitnessFixtureWorkspace
        $fixtureRoot = $fixture.Root
        Test-WitnessFixtureWorkspace -Workspace $fixture
        Write-Step 'Disposable synthetic WAV fixture contract passed.'
    } finally {
        Remove-WitnessFixtureWorkspace -Workspace $fixture
    }
    if (-not [string]::IsNullOrWhiteSpace([string]$fixtureRoot) -and (Test-Path -LiteralPath $fixtureRoot)) {
        throw 'Fixture self-test left its temporary workspace behind.'
    }
    exit 0
}

if ($ValidateExisting) {
    $app = Resolve-App -Path $AppPath
    Validate-Evidence -Path $EvidencePath -ExpectedApp $app
    exit 0
}

# Reject unattended generation before resolving or launching the supplied app.
Assert-NotCi
$app = Resolve-App -Path $AppPath
$windowsBuild = Assert-Windows11X64
$appFingerprint = Get-AppFingerprint -App $app
Invoke-GuiSmokePreflight -App $app

$fixture = New-WitnessFixtureWorkspace
try {
    Test-WitnessFixtureWorkspace -Workspace $fixture
    Write-Step 'A disposable synthetic WAV fixture workspace was prepared; no personal music is required.'
    Write-Step "Primary import fixture: $($fixture.Source)"
    Write-Step "Byte-identical duplicate fixture: $($fixture.Duplicate)"
    Write-Step "Relocate destination directory: $($fixture.RelocatedDirectory)"
    Write-Step "Local step guide: $($fixture.Readme)"
    Write-Step 'The fixture contains a quiet generated 440 Hz PCM tone and is never played by this script.'
    Write-Step 'Keep system/headphone volume conservative if you choose to start playback for the History check.'
    Write-Step 'This witness never asks for track names or paths and does not inspect source music.'
    Write-Step 'Do not include screenshots or notes containing private paths in public artifacts.'
    Write-Step 'The automated preflight opened no audio device; the manual launch/resize check below is still required for real usability review.'
    Write-Host ''

    $checks = [ordered]@{}
    $checks.launchAndResize = Read-YesNo 'BrokeDJ launched on Windows 11 and remained usable while resizing without overlapping/hidden critical controls?'
    $checks.importAndSearch = Read-YesNo 'The generated primary fixture imported successfully and bounded library search found it?'
    $checks.tagsAndPlaylists = Read-YesNo 'Tag editing plus playlist create/add/remove/browse worked on the fixture and persisted after refresh?'
    $checks.history = Read-YesNo 'Starting the generated fixture once under your control created a bounded local history entry visible in the History view?'
    $checks.duplicateAndMissingReview = Read-YesNo 'The generated duplicate plus a deliberately moved primary exercised duplicate/missing review non-destructively?'
    $checks.relocate = Read-YesNo 'Relocate reconnected the moved primary fixture without losing tags/playlist membership?'
    $checks.libraryBackupRestore = Read-YesNo 'Library backup succeeded; after a controlled metadata mutation, restore returned the prior library state?'
    $checks.sessionSaveLoad = Read-YesNo 'A four-deck/mixer session snapshot saved and loaded with restored controls while decks remained paused until explicit Play?'

    $failedChecks = @($checks.Keys | Where-Object { -not [bool]$checks[$_] })
    if ($failedChecks.Count -gt 0) {
        throw ('M4 witness failed; no evidence was written. Failed checks: ' + ($failedChecks -join ', '))
    }

    $evidence = [ordered]@{
        schema = 1
        project = 'BrokeDJ'
        scope = 'M4-library-workflow'
        generatedUtc = [DateTimeOffset]::UtcNow.ToString('o')
        environment = [ordered]@{
            windowsBuild = $windowsBuild
            architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            processArchitecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        }
        app = $appFingerprint
        checks = $checks
        privacy = [ordered]@{
            containsTrackPaths = $false
            containsTrackNames = $false
            containsSourceMusic = $false
        }
    }

    $parent = Split-Path -Parent $EvidencePath
    if ([string]::IsNullOrWhiteSpace($parent)) {
        $parent = (Get-Location).Path
    } elseif (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    $parent = (Resolve-Path -LiteralPath $parent).Path
    $finalEvidencePath = Join-Path $parent ([System.IO.Path]::GetFileName($EvidencePath))
    $temporaryEvidencePath = Join-Path $parent ('.BrokeDJ-M4-Witness-' + [Guid]::NewGuid().ToString('N') + '.tmp')

    try {
        $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $temporaryEvidencePath -Encoding utf8
        Validate-Evidence -Path $temporaryEvidencePath -ExpectedApp $app
        Move-Item -LiteralPath $temporaryEvidencePath -Destination $finalEvidencePath -Force
        Validate-Evidence -Path $finalEvidencePath -ExpectedApp $app
        Write-Step "Evidence written to: $finalEvidencePath"
    } catch {
        Write-Error $_
        exit 2
    } finally {
        Remove-Item -LiteralPath $temporaryEvidencePath -Force -ErrorAction SilentlyContinue
    }
} finally {
    Remove-WitnessFixtureWorkspace -Workspace $fixture
}
