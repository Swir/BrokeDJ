# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [string]$ValidatorPath = (Join-Path $PSScriptRoot 'validate_ui_witness.ps1')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedValidator = (Resolve-Path -LiteralPath $ValidatorPath).Path
Add-Type -AssemblyName System.Drawing

$root = Join-Path ([System.IO.Path]::GetTempPath()) ("BrokeDJ-ui-validator-selftest-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $root | Out-Null

function New-BrokeDJSyntheticWitness {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [int]$Width,
        [Parameter(Mandatory = $true)] [int]$Height,
        [Parameter(Mandatory = $true)] [bool]$Blank,
        [switch]$DeadRightQuarter
    )

    $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 2, 5, 10))
        if (-not $Blank) {
            # Deterministic synthetic fixture used only to test the validator. Keep
            # accent coverage well below the production upper bound while adding
            # independent colour swatches so the diversity check is meaningful.
            $stripeCount = 64
            $stripeWidth = [Math]::Max(2, [int]($Width / ($stripeCount * 6)))
            for ($i = 0; $i -lt $stripeCount; ++$i) {
                $r = 4 + ($i % 10)
                $g = [Math]::Min(190, 38 + $i * 2)
                $b = [Math]::Min(230, 88 + $i * 2)
                $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, $r, $g, $b))
                try {
                    $x = 18 + $i * [Math]::Max(5, [int](($Width - 36) / $stripeCount))
                    $graphics.FillRectangle($brush, $x, 70, $stripeWidth, $Height - 130)
                }
                finally { $brush.Dispose() }
            }

            for ($i = 0; $i -lt 48; ++$i) {
                $r = 20 + (($i * 37) % 140)
                $g = 18 + (($i * 53) % 130)
                $b = 16 + (($i * 71) % 120)
                $swatch = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, $r, $g, $b))
                try {
                    $x = 24 + ($i % 16) * [Math]::Max(20, [int](($Width - 48) / 16))
                    $y = 45 + [int]($i / 16) * 22
                    $graphics.FillRectangle($swatch, $x, $y, 18, 14)
                }
                finally { $swatch.Dispose() }
            }

            $linePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 98, 229, 255), 2.0)
            $brightBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 244, 250, 255))
            try {
                for ($row = 0; $row -lt 6; ++$row) {
                    $y = 130 + $row * [Math]::Max(72, [int](($Height - 190) / 6))
                    $graphics.DrawRectangle($linePen, 34, $y, $Width - 68, 40)
                }
                for ($i = 0; $i -lt 18; ++$i) {
                    $x = 42 + ($i % 9) * [Math]::Max(70, [int](($Width - 96) / 9))
                    $y = 150 + [int]($i / 9) * 300
                    $graphics.FillRectangle($brightBrush, $x, $y, 26, 10)
                }
            }
            finally {
                $linePen.Dispose()
                $brightBrush.Dispose()
            }
        }

        if ($DeadRightQuarter) {
            $dead = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Black)
            try {
                $x = [int][Math]::Floor($Width * 0.74)
                $graphics.FillRectangle($dead, $x, 36, $Width - $x, $Height - 36)
            }
            finally { $dead.Dispose() }
        }

        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Write-BrokeDJManifest {
    param([Parameter(Mandatory = $true)] [string]$Directory)
    $manifest = [ordered]@{
        schema_version = 2
        mode = 'windows-ui-visual-witness'
        plays_audio = $false
        opens_audio_device = $false
        smoke_success = $true
        smoke_exercised_requested_1600x900 = $true
        capture_settle_milliseconds = 90
        compact = [ordered]@{ file = 'compact.png'; width = 1066; height = 839; bytes = 8192; print_window_flag = 2; stable_frame = $true }
        workstation = [ordered]@{ file = 'workstation.png'; width = 1616; height = 939; bytes = 8192; print_window_flag = 2; capture_class = 'requested-1600-class'; stable_frame = $true }
        observed_window_sizes = @()
        qualification_note = 'Synthetic validator self-test fixture only.'
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Directory 'BrokeDJ-ui-visual-witness.json') -Encoding utf8
}

try {
    Write-BrokeDJManifest -Directory $root
    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'compact.png') -Width 1066 -Height 839 -Blank $false
    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'workstation.png') -Width 1616 -Height 939 -Blank $false

    & pwsh -NoProfile -File $resolvedValidator -WitnessDirectory $root -SampleStep 6
    if ($LASTEXITCODE -ne 0) {
        throw "UI witness validator rejected its deterministic positive self-test fixture: $LASTEXITCODE"
    }
    $positive = Get-Content -LiteralPath (Join-Path $root 'BrokeDJ-ui-quality.json') -Raw | ConvertFrom-Json
    if ($positive.success -ne $true -or $positive.schema_version -ne 2) {
        throw 'UI witness validator positive self-test did not report schema-2 success=true.'
    }

    # Reproduce the exact class of regression that motivated the new gate: a
    # native 1600-class window containing an otherwise plausible old-size frame
    # plus a dead black strip on the right. Global colour metrics alone used to
    # accept this capture; the independent right-quarter coverage must reject it.
    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'workstation.png') -Width 1616 -Height 939 -Blank $false -DeadRightQuarter
    & pwsh -NoProfile -File $resolvedValidator -WitnessDirectory $root -SampleStep 6 *> $null
    if ($LASTEXITCODE -eq 0) {
        throw 'UI witness validator accepted the deterministic partial-width workstation negative fixture.'
    }
    $partial = Get-Content -LiteralPath (Join-Path $root 'BrokeDJ-ui-quality.json') -Raw | ConvertFrom-Json
    if ($partial.success -ne $false -or @($partial.failures).Count -lt 1 -or
        [double]$partial.workstation.right_quarter_non_black_fraction -ge 0.80) {
        throw 'UI witness validator partial-width negative did not preserve the full-surface failure evidence.'
    }

    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'compact.png') -Width 1066 -Height 839 -Blank $true
    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'workstation.png') -Width 1616 -Height 939 -Blank $true

    & pwsh -NoProfile -File $resolvedValidator -WitnessDirectory $root -SampleStep 6 *> $null
    if ($LASTEXITCODE -eq 0) {
        throw 'UI witness validator accepted the deterministic all-dark negative fixture.'
    }
    $negative = Get-Content -LiteralPath (Join-Path $root 'BrokeDJ-ui-quality.json') -Raw | ConvertFrom-Json
    if ($negative.success -ne $false -or @($negative.failures).Count -lt 1) {
        throw 'UI witness validator all-dark negative self-test did not preserve failure diagnostics.'
    }

    Write-Host 'BrokeDJ UI witness validator self-test passed (positive accepted, partial-width and all-dark negatives rejected).'
}
finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
