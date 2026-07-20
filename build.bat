@echo off
setlocal

set "CMAKE_EXE=cmake"
where cmake >nul 2>nul
if errorlevel 1 set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"

if not exist "%CMAKE_EXE%" if /i not "%CMAKE_EXE%"=="cmake" (
    echo CMake was not found.
    echo.
    echo Install CMake, then run this file again.
    echo Expected path: C:\Program Files\CMake\bin\cmake.exe
    echo.
    pause
    exit /b 1
)

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if defined VSINSTALL if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
    call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
)

if not exist build mkdir build
"%CMAKE_EXE%" -S . -B build
if errorlevel 1 (
    echo.
    echo CMake configuration failed.
    pause
    exit /b 1
)

"%CMAKE_EXE%" --build build --config Release
if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Built executable:
echo build\Release\Pr0mt_X Text.exe
echo.
pause
