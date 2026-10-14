@echo off
rem Build EmberEngine: build.bat [debug|release]
setlocal
if "%VULKAN_SDK%"=="" set "VULKAN_SDK=C:\VulkanSDK\1.4.363.0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
cd /d "%~dp0"
cmake --preset %CONFIG% || exit /b 1
cmake --build --preset %CONFIG% || exit /b 1
