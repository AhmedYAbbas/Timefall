@echo off
rem Verification-only compile check: builds the solution so Claude can catch
rem compile errors before handing work over. Never used to run binaries.
rem Usage: check-compile.bat [Configuration] [extra msbuild args]  (default: Debug)
setlocal

set "ROOT=%~dp0.."
set "CONFIG=%~1"
if "%CONFIG%"=="" set CONFIG=Debug

set VSWHERE="C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set MSBUILD=
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo check-compile: MSBuild not found via vswhere
    exit /b 1
)

pushd "%ROOT%"
"%MSBUILD%" Timefall.slnx /m /nologo /v:minimal /p:Configuration=%CONFIG% /p:Platform=x64 %2 %3 %4 %5
set EXITCODE=%ERRORLEVEL%
popd
exit /b %EXITCODE%
