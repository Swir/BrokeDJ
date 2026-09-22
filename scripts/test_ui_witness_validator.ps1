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
        [Parameter(Mandatory = $true)] [bool]$Blank
    )

    $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 2, 5, 10))
        if (-not $Blank) {
            # Deterministic dark workstation fixture with enough genuine colour and
            # luminance variation to exercise every success threshold. It is not a
            # screenshot mock and is never used as product evidence.
            $stripeCount = 64
            $stripeWidth = [Math]::Max(2, [int]($Width / ($stripeCount * 2)))
            for ($i = 0; $i -lt $stripeCount; ++$i) {
                $r = 4 + ($i % 10)
                $g = [Math]::Min(210, 42 + $i * 3)
                $b = [Math]::Min(245, 92 + $i * 3)
                $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, $r, $g, $b))
                try {
                    $x = 18 + $i * [Math]::Max(5, [int](($Width - 36) / $stripeCount))
                    $graphics.FillRectangle($brush, $x, 70, $stripeWidth, $Height - 130)
                }
                finally { $brush.Dispose() }
            }

            $panelBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 7, 17, 28))
            $linePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 98, 229, 255), 2.0)
            $brightBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 244, 250, 255))
            try {
                for ($row = 0; $row -lt 6; ++$row) {
                    $y = 52 + $row * [Math]::Max(72, [int](($Height - 120) / 6))
                    $graphics.FillRectangle($panelBrush, 34, $y, $Width - 68, 46)
                    $graphics.DrawRectangle($linePen, 34, $y, $Width - 68, 46)
                }
                for ($i = 0; $i -lt 18; ++$i) {
                    $x = 42 + ($i % 9) * [Math]::Max(70, [int](($Width - 96) / 9))
                    $y = 64 + [int]($i / 9) * 300
                    $graphics.FillRectangle($brightBrush, $x, $y, 26, 10)
                }
            }
            finally {
                $panelBrush.Dispose()
                $linePen.Dispose()
                $brightBrush.Dispose()
            }
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
        compact = [ordered]@{ file = 'compact.png'; width = 1066; height = 839; bytes = 8192; print_window_flag = 2 }
        workstation = [ordered]@{ file = 'workstation.png'; width = 1616; height = 939; bytes = 8192; print_window_flag = 2; capture_class = 'requested-1600-class' }
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
    if ($positive.success -ne $true) {
        throw 'UI witness validator positive self-test did not report success=true.'
    }

    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'compact.png') -Width 1066 -Height 839 -Blank $true
    New-BrokeDJSyntheticWitness -Path (Join-Path $root 'workstation.png') -Width 1616 -Height 939 -Blank $true

    & pwsh -NoProfile -File $resolvedValidator -WitnessDirectory $root -SampleStep 6 *> $null
    if ($LASTEXITCODE -eq 0) {
        throw 'UI witness validator accepted the deterministic all-black negative fixture.'
    }
    $negative = Get-Content -LiteralPath (Join-Path $root 'BrokeDJ-ui-quality.json') -Raw | ConvertFrom-Json
    if ($negative.success -ne $false -or @($negative.failures).Count -lt 1) {
        throw 'UI witness validator negative self-test did not preserve failure diagnostics.'
    }

    Write-Host 'BrokeDJ UI witness validator self-test passed (positive accepted, blank negative rejected).'
}
finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
