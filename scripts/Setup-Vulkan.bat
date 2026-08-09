@echo off
rem Launcher for Setup-Vulkan.ps1: installs/verifies the pinned Vulkan SDK (which ships Slang).
rem Runs unelevated first -- verifying an existing SDK needs no rights. If the script reports
rem exit code 2 ("install needed, not elevated"), relaunch once through UAC.
rem Usage: Setup-Vulkan.bat [-Force] [-AllowNewer] [-PinnedVersion 1.4.350.0]
setlocal

set "PS1=%~dp0Setup-Vulkan.ps1"
if not exist "%PS1%" (
    echo Setup-Vulkan: cannot find "%PS1%"
    exit /b 1
)

rem Prefer PowerShell 7 when present; the script also runs fine on Windows PowerShell 5.1.
set "PSEXE=powershell"
where pwsh >nul 2>&1 && set "PSEXE=pwsh"

%PSEXE% -NoProfile -ExecutionPolicy Bypass -File "%PS1%" %*
set "EXITCODE=%ERRORLEVEL%"

if not "%EXITCODE%"=="2" goto :done
if "%TF_SETUP_ELEVATED%"=="1" (
    echo Setup-Vulkan: still not elevated after a UAC relaunch; giving up.
    set "EXITCODE=1"
    goto :done
)

echo.
echo Requesting administrator privileges...
set "TF_SETUP_ELEVATED=1"
set "ARGS=%*"
if defined ARGS (
    %PSEXE% -NoProfile -Command "$p = Start-Process -FilePath '%~f0' -ArgumentList '%ARGS%' -Verb RunAs -Wait -PassThru; exit $p.ExitCode"
) else (
    %PSEXE% -NoProfile -Command "$p = Start-Process -FilePath '%~f0' -Verb RunAs -Wait -PassThru; exit $p.ExitCode"
)
set "EXITCODE=%ERRORLEVEL%"

:done
rem Leave a double-clicked window up long enough to read. Deliberately `timeout`, not `pause`:
rem it exits immediately when stdin is redirected, so it can never hang a non-interactive shell
rem the way Win-GenProjects.bat's PAUSE does. Set TF_NO_PAUSE=1 to skip it.
if not defined TF_NO_PAUSE timeout /t 15 >nul 2>&1

exit /b %EXITCODE%
