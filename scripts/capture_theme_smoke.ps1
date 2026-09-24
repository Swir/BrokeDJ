# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [ValidateRange(5, 120)]
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedExe = (Resolve-Path -LiteralPath $ExePath).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path
$captureScript = Join-Path $PSScriptRoot 'capture_ui_smoke.ps1'
if (-not (Test-Path -LiteralPath $captureScript)) {
    throw "Base UI capture helper is missing: $captureScript"
}

Add-Type -AssemblyName System.Drawing

function Measure-ThemeAccentHits {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [int]$TargetR,
        [Parameter(Mandatory = $true)] [int]$TargetG,
        [Parameter(Mandatory = $true)] [int]$TargetB,
        [int]$Tolerance = 28,
        [int]$Step = 4
    )

    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        [int64]$hits = 0
        [int64]$samples = 0
        for ($y = 36; $y -lt ($bitmap.Height - 4); $y += $Step) {
            for ($x = 4; $x -lt ($bitmap.Width - 4); $x += $Step) {
                $pixel = $bitmap.GetPixel($x, $y)
                ++$samples
                if ([Math]::Abs([int]$pixel.R - $TargetR) -le $Tolerance -and
                    [Math]::Abs([int]$pixel.G - $TargetG) -le $Tolerance -and
                    [Math]::Abs([int]$pixel.B - $TargetB) -le $Tolerance) {
                    ++$hits
                }
            }
        }
        return [pscustomobject]@{ hits = $hits; samples = $samples }
    }
    finally {
        $bitmap.Dispose()
    }
}

$themes = @(
    [pscustomobject]@{ token = 'electric-blue'; name = 'Electric Blue'; r = 0; g = 136; b = 255 },
    [pscustomobject]@{ token = 'ultraviolet'; name = 'Ultraviolet'; r = 138; g = 92; b = 255 },
    [pscustomobject]@{ token = 'ember'; name = 'Ember'; r = 255; g = 159; b = 28 }
)

$originalOverride = $env:BROKEDJ_UI_THEME_OVERRIDE
$entries = @()
try {
    foreach ($theme in $themes) {
        $env:BROKEDJ_UI_THEME_OVERRIDE = $theme.token
        $tempDirectory = Join-Path $resolvedOutput ('.theme-' + $theme.token)
        Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null

        # capture_ui_smoke.ps1 defines a small Win32 helper type. Run each theme
        # capture in a fresh pwsh process so Add-Type has isolated type state and
        # the native exit code can be checked unambiguously.
        & pwsh -NoProfile -File $captureScript `
            -ExePath $resolvedExe `
            -OutputDirectory $tempDirectory `
            -TimeoutSeconds $TimeoutSeconds
        if ($LASTEXITCODE -ne 0) {
            throw "Theme UI capture failed for $($theme.name): $LASTEXITCODE"
        }

        $source = Join-Path $tempDirectory 'BrokeDJ-ui-workstation-class.png'
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Theme workstation witness missing for $($theme.name)."
        }
        $destination = Join-Path $resolvedOutput ('BrokeDJ-theme-' + $theme.token + '.png')
        Copy-Item -LiteralPath $source -Destination $destination -Force

        $file = Get-Item -LiteralPath $destination
        if ($file.Length -lt 4096) {
            throw "Theme workstation witness is unexpectedly small for $($theme.name): $($file.Length) bytes"
        }

        $accent = Measure-ThemeAccentHits -Path $destination -TargetR $theme.r -TargetG $theme.g -TargetB $theme.b
        if ($accent.hits -lt 12) {
            throw "Theme accent was not visibly painted for $($theme.name): only $($accent.hits) sampled near-colour pixels."
        }

        $hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        $entries += [pscustomobject]@{
            token = $theme.token
            name = $theme.name
            file = [System.IO.Path]::GetFileName($destination)
            bytes = [int64]$file.Length
            sha256 = $hash
            target_rgb = @([int]$theme.r, [int]$theme.g, [int]$theme.b)
            sampled_pixels = [int64]$accent.samples
            accent_hits = [int64]$accent.hits
        }

        Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
    }
}
finally {
    if ($null -eq $originalOverride) {
        Remove-Item Env:BROKEDJ_UI_THEME_OVERRIDE -ErrorAction SilentlyContinue
    } else {
        $env:BROKEDJ_UI_THEME_OVERRIDE = $originalOverride
    }
}

$hashes = @($entries | ForEach-Object { $_.sha256 } | Sort-Object -Unique)
if ($entries.Count -ne 3 -or $hashes.Count -ne 3) {
    throw 'Theme witnesses are missing or visually identical by SHA-256.'
}

$manifest = [ordered]@{
    schema_version = 1
    mode = 'windows-ui-theme-witness'
    plays_audio = $false
    opens_audio_device = $false
    theme_count = $entries.Count
    themes = @($entries)
    success = $true
    qualification_note = 'No-audio Windows runner witness that each selectable BrokeDJ accent theme reaches the native workstation and paints its own accent family. This is visual regression evidence only, not manual usability, HiDPI, accessibility, audio or live-performance qualification.'
}
$manifestPath = Join-Path $resolvedOutput 'BrokeDJ-theme-witness.json'
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Host "Captured and differentiated $($entries.Count) BrokeDJ runtime theme witnesses."
