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
if ($null -eq $manifest.capture_settle_milliseconds -or [int]$manifest.capture_settle_milliseconds -lt 40) {
    throw 'UI witness is missing the settled-frame dwell contract.'
}
if ($manifest.compact.stable_frame -ne $true -or $manifest.workstation.stable_frame -ne $true) {
    throw 'UI witness images are not marked as settled frames.'
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
        $rightStart = [Math]::Max(4, [int][Math]::Floor($width * 0.75))
        $bottomStart = [Math]::Max($startY, [int][Math]::Floor($height * 0.75))
        [int64]$samples = 0
        [double]$luminanceSum = 0.0
        [double]$luminanceSquaredSum = 0.0
        [int64]$nearBlack = 0
        [int64]$bright = 0
        [int64]$accent = 0
        [int64]$nonBlack = 0
        [int64]$rightSamples = 0
        [int64]$rightNonBlack = 0
        [int64]$bottomSamples = 0
        [int64]$bottomNonBlack = 0
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

                $isNearBlack = [Math]::Max($r, [Math]::Max($g, $b)) -lt 10
                if ($isNearBlack) { ++$nearBlack }
                else { ++$nonBlack }
                if ($luminance -gt 120.0) { ++$bright }

                if ($x -ge $rightStart) {
                    ++$rightSamples
                    if (-not $isNearBlack) { ++$rightNonBlack }
                }
                if ($y -ge $bottomStart) {
                    ++$bottomSamples
                    if (-not $isNearBlack) { ++$bottomNonBlack }
                }

                $blueCyan = ($b -ge 80 -and ($b - $r) -ge 30 -and ($g - $r) -ge 10)
                $brightCyan = ($g -gt 150 -and $b -gt 150 -and $r -lt 140)
                if ($blueCyan -or $brightCyan) { ++$accent }

                $key = ([int]($r / 16) -shl 8) -bor ([int]($g / 16) -shl 4) -bor [int]($b / 16)
                [void]$colours.Add($key)
            }
        }

        if ($samples -le 0 -or $rightSamples -le 0 -or $bottomSamples -le 0) {
            throw 'UI witness produced no sampled pixels for a required coverage region.'
        }
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
            right_quarter_non_black_fraction = [Math]::Round($rightNonBlack / [double]$rightSamples, 5)
            bottom_quarter_non_black_fraction = [Math]::Round($bottomNonBlack / [double]$bottomSamples, 5)
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

# The Beta workstation must paint the complete native content surface after a resize.
# A previous witness could accept the old-size 1280-wide frame inside a 1600-wide
# window, leaving a large black strip on the right. Global palette statistics were
# too forgiving, so both the right and bottom quarters are now independently gated.
if ($compact.non_black_fraction -lt 0.90) {
    $failures.Add("compact painted-content fraction is too low: $($compact.non_black_fraction)")
}
if ($workstation.non_black_fraction -lt 0.90) {
    $failures.Add("workstation painted-content fraction is too low: $($workstation.non_black_fraction)")
}
foreach ($edge in @(
    [pscustomobject]@{ label = 'compact right quarter'; value = [double]$compact.right_quarter_non_black_fraction },
    [pscustomobject]@{ label = 'compact bottom quarter'; value = [double]$compact.bottom_quarter_non_black_fraction },
    [pscustomobject]@{ label = 'workstation right quarter'; value = [double]$workstation.right_quarter_non_black_fraction },
    [pscustomobject]@{ label = 'workstation bottom quarter'; value = [double]$workstation.bottom_quarter_non_black_fraction }
)) {
    if ($edge.value -lt 0.80) {
        $failures.Add("$($edge.label) is not fully presented; non-black fraction=$($edge.value)")
    }
}

$report = [ordered]@{
    schema_version = 2
    mode = 'windows-ui-pixel-quality-gate'
    source_manifest_schema = [int]$manifest.schema_version
    plays_audio = $false
    opens_audio_device = $false
    sample_step = $SampleStep
    settled_capture_milliseconds = [int]$manifest.capture_settle_milliseconds
    thresholds = [ordered]@{
        mean_luminance = [ordered]@{ min = 8.0; max = 95.0 }
        luminance_stddev_min = 12.0
        bright_fraction = [ordered]@{ min = 0.004; max = 0.20 }
        blue_cyan_fraction = [ordered]@{ min = 0.008; max = 0.25 }
        quantized_colour_count_min = 32
        non_black_fraction_min = 0.90
        edge_quarter_non_black_fraction_min = 0.80
    }
    compact = $compact
    workstation = $workstation
    success = ($failures.Count -eq 0)
    failures = @($failures)
    qualification_note = 'Automated settled-frame pixel gate for blank/corrupt/theme/full-surface presentation regressions only. It does not judge aesthetic quality, HiDPI usability, accessibility, audio behavior, controller behavior or live readiness.'
}
$report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding utf8

Write-Host "Compact UI metrics: mean=$($compact.mean_luminance) stddev=$($compact.luminance_stddev) full=$($compact.non_black_fraction) right=$($compact.right_quarter_non_black_fraction) bottom=$($compact.bottom_quarter_non_black_fraction)"
Write-Host "Workstation UI metrics: mean=$($workstation.mean_luminance) stddev=$($workstation.luminance_stddev) full=$($workstation.non_black_fraction) right=$($workstation.right_quarter_non_black_fraction) bottom=$($workstation.bottom_quarter_non_black_fraction)"

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) { Write-Error $failure -ErrorAction Continue }
    throw "BrokeDJ UI pixel quality gate failed with $($failures.Count) issue(s)."
}

Write-Host 'BrokeDJ UI pixel quality gate passed.'
