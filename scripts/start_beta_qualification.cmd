@echo off
setlocal
set "ROOT=%~dp0"
set "APP=%ROOT%BrokeDJ.exe"
set "RUNNER=%ROOT%BETA-WITNESS-RUNNER.ps1"
set "M1_WITNESS=%ROOT%M1-HARDWARE-WITNESS.ps1"
set "BETA_EVIDENCE=%USERPROFILE%\Documents\BrokeDJ-Beta-Evidence"
set "M1_EVIDENCE=%USERPROFILE%\Documents\BrokeDJ-M1-Evidence"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

if not exist "%POWERSHELL%" (
  echo [BrokeDJ] Windows PowerShell was not found at the expected system path.
  pause
  exit /b 2
)
if not exist "%APP%" (
  echo [BrokeDJ] Missing BrokeDJ.exe next to this launcher.
  pause
  exit /b 2
)

if /I "%~1"=="M1" goto m1_only
if not "%~1"=="" (
  echo [BrokeDJ] Unknown mode "%~1". Use no argument for full Beta qualification or M1 for the M1-only witness.
  pause
  exit /b 2
)

if not exist "%RUNNER%" (
  echo [BrokeDJ Beta] Missing BETA-WITNESS-RUNNER.ps1 next to this launcher.
  pause
  exit /b 2
)
if not exist "%BETA_EVIDENCE%" mkdir "%BETA_EVIDENCE%"
if errorlevel 1 (
  echo [BrokeDJ Beta] Could not create evidence directory:
  echo %BETA_EVIDENCE%
  pause
  exit /b 3
)

echo [BrokeDJ Beta] Candidate: %APP%
echo [BrokeDJ Beta] Evidence:  %BETA_EVIDENCE%
echo [BrokeDJ Beta] This guided run never auto-starts ordinary playback, microphone input or recording.
echo.
"%POWERSHELL%" -NoLogo -NoProfile -File "%RUNNER%" -AppPath "%APP%" -EvidenceDirectory "%BETA_EVIDENCE%"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [BrokeDJ Beta] Qualification stopped with exit code %RC%.
  echo [BrokeDJ Beta] Any previously accepted evidence remains in:
  echo %BETA_EVIDENCE%
  pause
  exit /b %RC%
)

echo [BrokeDJ Beta] Manual M1-M4 qualification completed for this exact candidate.
echo [BrokeDJ Beta] Public Beta publication still requires the repository release gate.
echo [BrokeDJ Beta] Evidence: %BETA_EVIDENCE%
pause
exit /b 0

:m1_only
if not exist "%M1_WITNESS%" (
  echo [BrokeDJ M1] Missing M1-HARDWARE-WITNESS.ps1 next to this launcher.
  pause
  exit /b 2
)
if not exist "%M1_EVIDENCE%" mkdir "%M1_EVIDENCE%"
if errorlevel 1 (
  echo [BrokeDJ M1] Could not create evidence directory:
  echo %M1_EVIDENCE%
  pause
  exit /b 3
)

echo [BrokeDJ M1] Candidate: %APP%
echo [BrokeDJ M1] Evidence:  %M1_EVIDENCE%
echo [BrokeDJ M1] Real Windows 11 hardware is required.
echo [BrokeDJ M1] This launcher never starts playback, switches devices, disconnects hardware or invents listening evidence.
echo.
"%POWERSHELL%" -NoLogo -NoProfile -File "%M1_WITNESS%" -AppPath "%APP%" -EvidencePath "%M1_EVIDENCE%\BrokeDJ-M1-Hardware-Witness.json" -ProbePath "%M1_EVIDENCE%\BrokeDJ-device-probe.json"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [BrokeDJ M1] Qualification stopped with exit code %RC%.
  echo [BrokeDJ M1] Fix the observed blocker and rerun; do not edit failed evidence into success.
  echo [BrokeDJ M1] Evidence: %M1_EVIDENCE%
  pause
  exit /b %RC%
)

echo [BrokeDJ M1] M1 hardware witness completed for this exact candidate.
echo [BrokeDJ M1] This does not by itself close M1 until repository acceptance is reviewed.
echo [BrokeDJ M1] Evidence: %M1_EVIDENCE%
pause
exit /b 0
