[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$AppPath,

    [string]$EvidencePath = ".\BrokeDJ-M2-keylock-listening.json",

    [switch]$ValidateExisting,

    [ValidateRange(0, 64)]
    [int]$RepresentativeTrackCount = 0,

    [switch]$NormalBaselineReviewed,
    [switch]$SlowKeyLockReviewed,
    [switch]$FastKeyLockReviewed,
    [switch]$TransportFallbackReviewed,
    [switch]$PitchStabilityAcceptable,
    [switch]$NoCriticalArtifactsObserved,
    [switch]$RuntimeErrorReview,
    [switch]$UserOwnedOrLicensed
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-ExactProperties {
    param(
        [Parameter(Mandatory = $true)] [object]$Object,
        [Parameter(Mandatory = $true)] [string[]]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Object) {
        throw "$Context is missing."
    }

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
    if ($Value -isnot [bool]) {
        throw "$Context must be a JSON boolean."
    }
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

function Get-AppIdentity {
    param([string]$Path)

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $file = Get-Item -LiteralPath $resolved.Path -ErrorAction Stop
    if ($file.Name -cne "BrokeDJ.exe") {
        throw "The selected application must be named BrokeDJ.exe."
    }

    $version = $file.VersionInfo.FileVersion
    if ([string]::IsNullOrWhiteSpace($version)) {
        $version = "unknown"
    }

    return [ordered]@{
        fileName = $file.Name
        fileVersion = [string]$version
        sha256 = (Get-FileHash -LiteralPath $resolved.Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Test-Witness {
    param(
        [Parameter(Mandatory = $true)] [object]$Evidence,
        [Parameter(Mandatory = $true)] [object]$ExpectedApp
    )

    Assert-ExactProperties $Evidence @(
        "schema", "project", "scope", "generatedUtc", "environment",
        "app", "material", "checks", "privacy"
    ) "evidence"

    Assert-Integer $Evidence.schema "schema"
    if ([int64]$Evidence.schema -ne 1) { throw "Unsupported M2 witness schema." }

    Assert-String $Evidence.project "project"
    Assert-String $Evidence.scope "scope"
    if ($Evidence.project -cne "BrokeDJ" -or $Evidence.scope -cne "M2-key-lock-listening") {
        throw "Evidence project or scope does not match the M2 key-lock listening witness."
    }

    if ($Evidence.generatedUtc -is [string]) {
        try {
            [void][datetimeoffset]::Parse($Evidence.generatedUtc)
        } catch {
            throw "generatedUtc is not a valid timestamp."
        }
    } elseif ($Evidence.generatedUtc -isnot [datetime]) {
        throw "generatedUtc must be an ISO timestamp."
    }

    Assert-ExactProperties $Evidence.environment @(
        "windowsBuild", "architecture", "processArchitecture"
    ) "environment"
    Assert-Integer $Evidence.environment.windowsBuild "environment.windowsBuild"
    Assert-String $Evidence.environment.architecture "environment.architecture"
    Assert-String $Evidence.environment.processArchitecture "environment.processArchitecture"
    if ([int64]$Evidence.environment.windowsBuild -lt 22000) {
        throw "This witness requires a Windows 11-class build."
    }
    if ($Evidence.environment.architecture -cne "X64" -or
        $Evidence.environment.processArchitecture -cne "X64") {
        throw "This witness requires Windows x64 and an x64 BrokeDJ process."
    }

    Assert-ExactProperties $Evidence.app @("fileName", "fileVersion", "sha256") "app"
    Assert-String $Evidence.app.fileName "app.fileName"
    Assert-String $Evidence.app.fileVersion "app.fileVersion"
    Assert-String $Evidence.app.sha256 "app.sha256"
    if ($Evidence.app.fileName -cne $ExpectedApp.fileName -or
        $Evidence.app.fileVersion -cne $ExpectedApp.fileVersion -or
        $Evidence.app.sha256 -cne $ExpectedApp.sha256) {
        throw "Evidence is not bound to the selected BrokeDJ.exe."
    }
    if ($Evidence.app.sha256 -notmatch "^[0-9a-f]{64}$") {
        throw "app.sha256 is not a lowercase SHA-256 digest."
    }

    Assert-ExactProperties $Evidence.material @(
        "representativeTrackCount", "userOwnedOrLicensed"
    ) "material"
    Assert-Integer $Evidence.material.representativeTrackCount "material.representativeTrackCount"
    Assert-Boolean $Evidence.material.userOwnedOrLicensed "material.userOwnedOrLicensed"
    if ([int64]$Evidence.material.representativeTrackCount -lt 3) {
        throw "At least three representative tracks are required."
    }
    if ($Evidence.material.userOwnedOrLicensed -ne $true) {
        throw "Listening material must be user-owned or otherwise licensed for testing."
    }

    $checkNames = @(
        "normalBaselineReviewed",
        "slowKeyLockReviewed",
        "fastKeyLockReviewed",
        "transportFallbackReviewed",
        "pitchStabilityAcceptable",
        "noCriticalArtifactsObserved",
        "runtimeErrorReview"
    )
    Assert-ExactProperties $Evidence.checks $checkNames "checks"
    foreach ($name in $checkNames) {
        Assert-Boolean $Evidence.checks.$name "checks.$name"
        if ($Evidence.checks.$name -ne $true) {
            throw "M2 listening witness is incomplete."
        }
    }

    $privacyNames = @(
        "containsDeviceNames",
        "containsTrackPaths",
        "containsTrackNames",
        "containsSourceMusic"
    )
    Assert-ExactProperties $Evidence.privacy $privacyNames "privacy"
    foreach ($name in $privacyNames) {
        Assert-Boolean $Evidence.privacy.$name "privacy.$name"
        if ($Evidence.privacy.$name -ne $false) {
            throw "Privacy-unsafe M2 listening evidence was rejected."
        }
    }

    return $true
}

try {
    $app = Get-AppIdentity -Path $AppPath

    if ($ValidateExisting) {
        if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) {
            throw "Evidence file does not exist."
        }

        $evidence = Get-Content -LiteralPath $EvidencePath -Raw -ErrorAction Stop | ConvertFrom-Json
        [void](Test-Witness -Evidence $evidence -ExpectedApp $app)
        Write-Host "M2 key-lock listening evidence: VALID"
        exit 0
    }

    if ($env:GITHUB_ACTIONS -eq "true" -or $env:CI -eq "true") {
        throw "Generation mode refuses CI. Listening evidence must come from a real user-controlled session."
    }
    $isWindowsHost = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
        [System.Runtime.InteropServices.OSPlatform]::Windows)
    if (-not $isWindowsHost) {
        throw "M2 listening evidence must be recorded on Windows 11 x64."
    }

    $architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToUpperInvariant()
    $processArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString().ToUpperInvariant()
    $windowsBuild = [System.Environment]::OSVersion.Version.Build
    if ($windowsBuild -lt 22000 -or $architecture -cne "X64" -or $processArchitecture -cne "X64") {
        throw "M2 listening evidence requires Windows 11 x64 with an x64 BrokeDJ process."
    }

    $evidence = [ordered]@{
        schema = 1
        project = "BrokeDJ"
        scope = "M2-key-lock-listening"
        generatedUtc = [DateTime]::UtcNow.ToString("o")
        environment = [ordered]@{
            windowsBuild = [int]$windowsBuild
            architecture = $architecture
            processArchitecture = $processArchitecture
        }
        app = $app
        material = [ordered]@{
            representativeTrackCount = $RepresentativeTrackCount
            userOwnedOrLicensed = [bool]$UserOwnedOrLicensed
        }
        checks = [ordered]@{
            normalBaselineReviewed = [bool]$NormalBaselineReviewed
            slowKeyLockReviewed = [bool]$SlowKeyLockReviewed
            fastKeyLockReviewed = [bool]$FastKeyLockReviewed
            transportFallbackReviewed = [bool]$TransportFallbackReviewed
            pitchStabilityAcceptable = [bool]$PitchStabilityAcceptable
            noCriticalArtifactsObserved = [bool]$NoCriticalArtifactsObserved
            runtimeErrorReview = [bool]$RuntimeErrorReview
        }
        privacy = [ordered]@{
            containsDeviceNames = $false
            containsTrackPaths = $false
            containsTrackNames = $false
            containsSourceMusic = $false
        }
    }

    $parent = Split-Path -Parent $EvidencePath
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    $evidence | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $EvidencePath -Encoding utf8

    try {
        [void](Test-Witness -Evidence ($evidence | ConvertTo-Json -Depth 8 | ConvertFrom-Json) -ExpectedApp $app)
        Write-Host "M2 key-lock listening evidence written and complete."
        exit 0
    } catch {
        Write-Warning "Evidence was written but remains incomplete. Complete the documented listening checks and rerun."
        exit 2
    }
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
