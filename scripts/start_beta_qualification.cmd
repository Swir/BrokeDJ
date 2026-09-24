@echo off
setlocal
set "ROOT=%~dp0"
set "APP=%ROOT%BrokeDJ.exe"
set "RUNNER=%ROOT%BETA-WITNESS-RUNNER.ps1"
set "EVIDENCE=%USERPROFILE%\Documents\BrokeDJ-Beta-Evidence"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

if not exist "%POWERSHELL%" (
  echo [BrokeDJ Beta] Windows PowerShell was not found at the expected system path.
  pause
  exit /b 2
)
if not exist "%APP%" (
  echo [BrokeDJ Beta] Missing BrokeDJ.exe next to this launcher.
  pause
  exit /b 2
)
if not exist "%RUNNER%" (
  echo [BrokeDJ Beta] Missing BETA-WITNESS-RUNNER.ps1 next to this launcher.
  pause
  exit /b 2
)
if not exist "%EVIDENCE%" mkdir "%EVIDENCE%"
if errorlevel 1 (
  echo [BrokeDJ Beta] Could not create evidence directory:
  echo %EVIDENCE%
  pause
  exit /b 3
)

echo [BrokeDJ Beta] Candidate: %APP%
echo [BrokeDJ Beta] Evidence:  %EVIDENCE%
echo [BrokeDJ Beta] This guided run never auto-starts ordinary playback, microphone input or recording.
echo.
"%POWERSHELL%" -NoLogo -NoProfile -File "%RUNNER%" -AppPath "%APP%" -EvidenceDirectory "%EVIDENCE%"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [BrokeDJ Beta] Qualification stopped with exit code %RC%.
  echo [BrokeDJ Beta] Any previously accepted evidence remains in:
  echo %EVIDENCE%
  pause
  exit /b %RC%
)

echo [BrokeDJ Beta] Manual M1-M4 qualification completed for this exact candidate.
echo [BrokeDJ Beta] Public Beta publication still requires the repository release gate.
echo [BrokeDJ Beta] Evidence: %EVIDENCE%
pause
exit /b 0
