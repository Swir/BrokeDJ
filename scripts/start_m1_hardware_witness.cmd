@echo off
setlocal
set "ROOT=%~dp0"
set "APP=%ROOT%BrokeDJ.exe"
set "WITNESS=%ROOT%M1-HARDWARE-WITNESS.ps1"
set "EVIDENCE_DIR=%USERPROFILE%\Documents\BrokeDJ-M1-Evidence"
set "EVIDENCE=%EVIDENCE_DIR%\BrokeDJ-M1-Hardware-Witness.json"
set "PROBE=%EVIDENCE_DIR%\BrokeDJ-device-probe.json"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

if not exist "%POWERSHELL%" (
  echo [BrokeDJ M1] Windows PowerShell was not found at the expected system path.
  pause
  exit /b 2
)
if not exist "%APP%" (
  echo [BrokeDJ M1] Missing BrokeDJ.exe next to this launcher.
  pause
  exit /b 2
)
if not exist "%WITNESS%" (
  echo [BrokeDJ M1] Missing M1-HARDWARE-WITNESS.ps1 next to this launcher.
  pause
  exit /b 2
)
if not exist "%EVIDENCE_DIR%" mkdir "%EVIDENCE_DIR%"
if errorlevel 1 (
  echo [BrokeDJ M1] Could not create evidence directory:
  echo %EVIDENCE_DIR%
  pause
  exit /b 3
)

echo [BrokeDJ M1] Candidate: %APP%
echo [BrokeDJ M1] Evidence:  %EVIDENCE_DIR%
echo [BrokeDJ M1] Real Windows 11 hardware is required.
echo [BrokeDJ M1] The launcher never auto-starts playback, switches devices, disconnects hardware, or invents listening evidence.
echo [BrokeDJ M1] Keep hardware volume conservative before pressing PLAY yourself.
echo.
cd /d "%ROOT%"
"%POWERSHELL%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%WITNESS%" -AppPath "%APP%" -EvidencePath "%EVIDENCE%" -ProbePath "%PROBE%"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [BrokeDJ M1] Qualification stopped with exit code %RC%.
  echo [BrokeDJ M1] Fix the observed blocker and rerun; do not edit failed evidence into success.
  echo [BrokeDJ M1] Evidence directory:
  echo %EVIDENCE_DIR%
  pause
  exit /b %RC%
)

echo [BrokeDJ M1] M1 hardware witness completed for this exact candidate.
echo [BrokeDJ M1] This does not by itself close M1 until the repository acceptance gate is reviewed.
echo [BrokeDJ M1] Evidence: %EVIDENCE_DIR%
pause
exit /b 0
