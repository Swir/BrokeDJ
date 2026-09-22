# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [string]$OutputDirectory = "ui-snapshots",

    [ValidateRange(5, 120)]
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedExe = (Resolve-Path -LiteralPath $ExePath).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class BrokeDJNativeWindowCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
}
"@

function Save-BrokeDJWindowPng {
    param(
        [Parameter(Mandatory = $true)] [IntPtr]$Handle,
        [Parameter(Mandatory = $true)] [string]$Path
    )

    $rect = New-Object BrokeDJNativeWindowCapture+RECT
    if (-not [BrokeDJNativeWindowCapture]::GetWindowRect($Handle, [ref]$rect)) {
        return $null
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        return $null
    }

    foreach ($flag in @(2, 0)) {
        $bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $hdc = [IntPtr]::Zero
        $ok = $false
        try {
            $hdc = $graphics.GetHdc()
            $ok = [BrokeDJNativeWindowCapture]::PrintWindow($Handle, $hdc, [uint32]$flag)
        }
        finally {
            if ($hdc -ne [IntPtr]::Zero) {
                $graphics.ReleaseHdc($hdc)
            }
            $graphics.Dispose()
        }

        try {
            if ($ok) {
                $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
            }
        }
        finally {
            $bitmap.Dispose()
        }

        if ($ok -and (Test-Path -LiteralPath $Path)) {
            $file = Get-Item -LiteralPath $Path
            if ($file.Length -ge 4096) {
                return [pscustomobject]@{
                    path = $file.FullName
                    width = $width
                    height = $height
                    bytes = $file.Length
                    print_window_flag = $flag
                }
            }
            Remove-Item -LiteralPath $Path -ErrorAction SilentlyContinue
        }
    }

    return $null
}

$compactPath = Join-Path $resolvedOutput 'BrokeDJ-ui-compact-1050x800.png'
$workstationPath = Join-Path $resolvedOutput 'BrokeDJ-ui-workstation-class.png'
$candidatePath = Join-Path $resolvedOutput 'BrokeDJ-ui-workstation-candidate.png'
$manifestPath = Join-Path $resolvedOutput 'BrokeDJ-ui-visual-witness.json'
Remove-Item $compactPath, $workstationPath, $candidatePath, $manifestPath -ErrorAction SilentlyContinue
Remove-Item (Join-Path (Get-Location) 'BrokeDJ-gui-smoke.json') -ErrorAction SilentlyContinue

$process = Start-Process -FilePath $resolvedExe -ArgumentList '--smoke-test' -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$compact = $null
$workstation = $null
$candidate = $null
$candidateArea = 0L
$observations = @()
$seenSizes = @{}

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $process.Refresh()
        if ($process.HasExited) { break }

        $handle = $process.MainWindowHandle
        if ($handle -ne [IntPtr]::Zero) {
            $rect = New-Object BrokeDJNativeWindowCapture+RECT
            if ([BrokeDJNativeWindowCapture]::GetWindowRect($handle, [ref]$rect)) {
                $width = $rect.Right - $rect.Left
                $height = $rect.Bottom - $rect.Top
                $sizeKey = "${width}x${height}"
                if (-not $seenSizes.ContainsKey($sizeKey)) {
                    $seenSizes[$sizeKey] = $true
                    Write-Host "Observed BrokeDJ window: $sizeKey"
                    $observations += [pscustomobject]@{ width = $width; height = $height }
                }

                if ($null -eq $compact -and $width -ge 1040 -and $width -le 1160 -and $height -ge 760) {
                    $compact = Save-BrokeDJWindowPng -Handle $handle -Path $compactPath
                }

                if ($null -eq $workstation -and $width -ge 1500 -and $height -ge 780) {
                    $workstation = Save-BrokeDJWindowPng -Handle $handle -Path $workstationPath
                }

                $area = [int64]$width * [int64]$height
                if ($width -ge 1240 -and $height -ge 760 -and $area -gt $candidateArea) {
                    $newCandidate = Save-BrokeDJWindowPng -Handle $handle -Path $candidatePath
                    if ($null -ne $newCandidate) {
                        $candidate = $newCandidate
                        $candidateArea = $area
                    }
                }
            }
        }

        Start-Sleep -Milliseconds 12
    }

    if (-not $process.HasExited) {
        $process.Kill()
        throw "BrokeDJ GUI smoke exceeded ${TimeoutSeconds}s while collecting visual witness."
    }
    if ($process.ExitCode -ne 0) {
        throw "BrokeDJ GUI smoke failed with exit code $($process.ExitCode)."
    }

    $smokePath = Join-Path (Get-Location) 'BrokeDJ-gui-smoke.json'
    if (-not (Test-Path -LiteralPath $smokePath)) {
        throw 'BrokeDJ-gui-smoke.json was not produced.'
    }
    $smoke = Get-Content -LiteralPath $smokePath -Raw | ConvertFrom-Json
    if ($smoke.schema_version -ne 1 -or $smoke.mode -ne 'native-resize-lifecycle' -or
        $smoke.plays_audio -ne $false -or $smoke.opens_audio_device -ne $false -or
        $smoke.success -ne $true -or $smoke.step_count -ne 4) {
        throw 'GUI smoke JSON contract failed while collecting visual witness.'
    }

    $requestedSizes = @($smoke.steps | ForEach-Object { "$($_.requested_width)x$($_.requested_height)" })
    if ($requestedSizes -notcontains '1050x800' -or $requestedSizes -notcontains '1600x900') {
        throw 'GUI smoke did not exercise both compact 1050x800 and workstation 1600x900 requested states.'
    }

    if ($null -eq $compact -or -not (Test-Path -LiteralPath $compactPath)) {
        throw ('Compact 1050x800-class window was not captured. Observed: ' + (($seenSizes.Keys | Sort-Object) -join ', '))
    }

    $workstationCaptureClass = 'requested-1600-class'
    if ($null -eq $workstation -or -not (Test-Path -LiteralPath $workstationPath)) {
        if ($null -eq $candidate -or -not (Test-Path -LiteralPath $candidatePath)) {
            throw ('Workstation-class window was not captured. Observed: ' + (($seenSizes.Keys | Sort-Object) -join ', '))
        }
        Move-Item -LiteralPath $candidatePath -Destination $workstationPath -Force
        $workstation = [pscustomobject]@{
            path = $workstationPath
            width = $candidate.width
            height = $candidate.height
            bytes = (Get-Item -LiteralPath $workstationPath).Length
            print_window_flag = $candidate.print_window_flag
        }
        $workstationCaptureClass = 'largest-observed-window-fallback'
    }
    else {
        Remove-Item -LiteralPath $candidatePath -ErrorAction SilentlyContinue
    }

    $manifest = [ordered]@{
        schema_version = 2
        mode = 'windows-ui-visual-witness'
        plays_audio = $false
        opens_audio_device = $false
        smoke_success = $true
        smoke_exercised_requested_1600x900 = $true
        compact = [ordered]@{
            file = [System.IO.Path]::GetFileName($compact.path)
            width = $compact.width
            height = $compact.height
            bytes = $compact.bytes
            print_window_flag = $compact.print_window_flag
        }
        workstation = [ordered]@{
            file = [System.IO.Path]::GetFileName($workstation.path)
            width = $workstation.width
            height = $workstation.height
            bytes = $workstation.bytes
            print_window_flag = $workstation.print_window_flag
            capture_class = $workstationCaptureClass
        }
        observed_window_sizes = @($observations)
        qualification_note = 'Windows Server runner pixel witness for layout regression review only. The smoke also requests 1600x900 even when the hosted display clamps native window bounds. This does not certify Windows 11 HiDPI quality, manual usability, audio behavior, controller behavior, or live readiness.'
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8

    Write-Host "Captured compact witness: $($compact.width)x$($compact.height), $($compact.bytes) bytes"
    Write-Host "Captured workstation witness: $($workstation.width)x$($workstation.height), $($workstation.bytes) bytes ($workstationCaptureClass)"
}
finally {
    if ($process -and -not $process.HasExited) {
        $process.Kill()
    }
}
