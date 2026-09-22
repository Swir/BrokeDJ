# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$WitnessDirectory,

    [ValidateRange(1, 16)]
    [int]$SampleStep = 5
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path -LiteralPath $WitnessDirectory).Path
$manifestPath = Join-Path $root 'BrokeDJ-ui-visual-witness.json'
$reportPath = Join-Path $root 'BrokeDJ-ui-quality.json'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "UI witness manifest missing: $manifestPath"
}

Add-Type -AssemblyName System.Drawing
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 2 -or $manifest.mode -ne 'windows-ui-visual-witness') {
    throw 'Unsupported BrokeDJ UI witness manifest.'
}
if ($manifest.plays_audio -ne $false -or $manifest.opens_audio_device -ne $false) {
    throw 'UI witness unexpectedly claims audio/device activity.'
}

function Measure-BrokeDJImage {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [int]$Step
    )

    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $width = $bitmap.Width
        $height = $bitmap.Height
        if ($width -lt 640 -or $height -lt 480) {
            throw "UI witness image is unexpectedly small: ${width}x${height}"
        }

        $startY = [Math]::Min(36, [Math]::Max(0, $height - 1))
        $endX = [Math]::Max(1, $width - 4)
        $endY = [Math]::Max($startY + 1, $height - 4)
        [int64]$samples = 0
        [double]$luminanceSum = 0.0
        [double]$luminanceSquaredSum = 0.0
        [int64]$nearBlack = 0
        [int64]$bright = 0
        [int64]$accent = 0
        [int64]$nonBlack = 0
        $colours = New-Object 'System.Collections.Generic.HashSet[int]'

        for ($y = $startY; $y -lt $endY; $y += $Step) {
            for ($x = 4; $x -lt $endX; $x += $Step) {
                $pixel = $bitmap.GetPixel($x, $y)
                $r = [int]$pixel.R
                $g = [int]$pixel.G
                $b = [int]$pixel.B
                $luminance = 0.2126 * $r + 0.7152 * $g + 0.0722 * $b
                $luminanceSum += $luminance
                $luminanceSquaredSum += $luminance * $luminance
                ++$samples

                if ([Math]::Max($r, [Math]::Max($g, $b)) -lt 10) { ++$nearBlack }
                else { ++$nonBlack }
                if ($luminance -gt 120.0) { ++$bright }

                $blueCyan = ($b -ge 80 -and ($b - $r) -ge 30 -and ($g - $r) -ge 10)
                $brightCyan = ($g -gt 150 -and $b -gt 150 -and $r -lt 140)
                if ($blueCyan -or $brightCyan) { ++$accent }

                $key = ([int]($r / 16) -shl 8) -bor ([int]($g / 16) -shl 4) -bor [int]($b / 16)
                [void]$colours.Add($key)
            }
        }

        if ($samples -le 0) { throw 'UI witness produced no sampled pixels.' }
        $mean = $luminanceSum / $samples
        $variance = [Math]::Max(0.0, ($luminanceSquaredSum / $samples) - ($mean * $mean))
        $stddev = [Math]::Sqrt($variance)

        return [ordered]@{
            width = $width
            height = $height
            sample_step = $Step
            sample_count = $samples
            mean_luminance = [Math]::Round($mean, 3)
            luminance_stddev = [Math]::Round($stddev, 3)
            near_black_fraction = [Math]::Round($nearBlack / [double]$samples, 5)
            non_black_fraction = [Math]::Round($nonBlack / [double]$samples, 5)
            bright_fraction = [Math]::Round($bright / [double]$samples, 5)
            blue_cyan_fraction = [Math]::Round($accent / [double]$samples, 5)
            quantized_colour_count = $colours.Count
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Test-BrokeDJImageMetrics {
    param(
        [Parameter(Mandatory = $true)] $Metrics,
        [Parameter(Mandatory = $true)] [string]$Label
    )

    $failures = New-Object 'System.Collections.Generic.List[string]'
    if ($Metrics.mean_luminance -lt 8.0 -or $Metrics.mean_luminance -gt 95.0) {
        $failures.Add("$Label mean luminance is outside the dark-workstation envelope: $($Metrics.mean_luminance)")
    }
    if ($Metrics.luminance_stddev -lt 12.0) {
        $failures.Add("$Label has too little luminance variation and may be blank/unpainted: $($Metrics.luminance_stddev)")
    }
    if ($Metrics.bright_fraction -lt 0.004 -or $Metrics.bright_fraction -gt 0.20) {
        $failures.Add("$Label bright-detail fraction is outside the expected envelope: $($Metrics.bright_fraction)")
    }
    if ($Metrics.blue_cyan_fraction -lt 0.008 -or $Metrics.blue_cyan_fraction -gt 0.25) {
        $failures.Add("$Label blue/cyan UI-accent fraction is outside the expected envelope: $($Metrics.blue_cyan_fraction)")
    }
    if ($Metrics.quantized_colour_count -lt 32) {
        $failures.Add("$Label colour diversity is too low and may indicate a blank/corrupt capture: $($Metrics.quantized_colour_count)")
    }
    return $failures
}

$compactPath = Join-Path $root ([string]$manifest.compact.file)
$workstationPath = Join-Path $root ([string]$manifest.workstation.file)
foreach ($path in @($compactPath, $workstationPath)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "UI witness image missing: $path" }
}

$compact = Measure-BrokeDJImage -Path $compactPath -Step $SampleStep
$workstation = Measure-BrokeDJImage -Path $workstationPath -Step $SampleStep
$failures = New-Object 'System.Collections.Generic.List[string]'
foreach ($failure in @(Test-BrokeDJImageMetrics -Metrics $compact -Label 'compact')) { $failures.Add($failure) }
foreach ($failure in @(Test-BrokeDJImageMetrics -Metrics $workstation -Label 'workstation')) { $failures.Add($failure) }

# Hosted Windows runners can expose off-screen black pixels when PrintWindow captures a
# window larger than the desktop. Do not mistake that runner limitation for an app defect;
# instead require enough painted content to reject genuinely empty captures.
if ($compact.non_black_fraction -lt 0.35) {
    $failures.Add("compact painted-content fraction is too low: $($compact.non_black_fraction)")
}
if ($workstation.non_black_fraction -lt 0.35) {
    $failures.Add("workstation painted-content fraction is too low: $($workstation.non_black_fraction)")
}

$report = [ordered]@{
    schema_version = 1
    mode = 'windows-ui-pixel-quality-gate'
    source_manifest_schema = [int]$manifest.schema_version
    plays_audio = $false
    opens_audio_device = $false
    sample_step = $SampleStep
    thresholds = [ordered]@{
        mean_luminance = [ordered]@{ min = 8.0; max = 95.0 }
        luminance_stddev_min = 12.0
        bright_fraction = [ordered]@{ min = 0.004; max = 0.20 }
        blue_cyan_fraction = [ordered]@{ min = 0.008; max = 0.25 }
        quantized_colour_count_min = 32
        non_black_fraction_min = 0.35
    }
    compact = $compact
    workstation = $workstation
    success = ($failures.Count -eq 0)
    failures = @($failures)
    qualification_note = 'Automated pixel sanity gate for blank/corrupt/theme-regression detection only. It does not judge aesthetic quality, HiDPI usability, accessibility, audio behavior, controller behavior or live readiness.'
}
$report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding utf8

Write-Host "Compact UI metrics: mean=$($compact.mean_luminance) stddev=$($compact.luminance_stddev) accent=$($compact.blue_cyan_fraction) colours=$($compact.quantized_colour_count)"
Write-Host "Workstation UI metrics: mean=$($workstation.mean_luminance) stddev=$($workstation.luminance_stddev) accent=$($workstation.blue_cyan_fraction) colours=$($workstation.quantized_colour_count)"

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) { Write-Error $failure -ErrorAction Continue }
    throw "BrokeDJ UI pixel quality gate failed with $($failures.Count) issue(s)."
}

Write-Host 'BrokeDJ UI pixel quality gate passed.'
