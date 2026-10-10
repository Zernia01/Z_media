@echo off
chcp 65001 >nul
setlocal
set "CONFIG=%~1"
set "FRESH="
set "CLEAN="
if "%~2"=="clean" set "FRESH=--fresh"
if "%~2"=="clean" set "CLEAN=--clean-first"
if "%CONFIG%"=="" set "CONFIG=Release"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS exit /b 1
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
set VSLANG=1033
set "CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
"%CMAKE%" %FRESH% -S "%~dp0.." -B "%~dp0..\build\%CONFIG%" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%~dp0..\build\%CONFIG%" --parallel %CLEAN%
if errorlevel 1 exit /b 1
"%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir "%~dp0..\build\%CONFIG%" --output-on-failure
exit /b %errorlevel%
