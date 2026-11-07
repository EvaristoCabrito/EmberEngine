@echo off
rem Builds one or more targets of the release preset with the same toolchain as build.bat (Action Points work, 2026-11-06).
rem Use: work\ap-build.bat <target> [target...]
setlocal EnableExtensions
cd /d "%~dp0.."
if not defined VULKAN_SDK for /f "delims=" %%V in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do if not defined VULKAN_SDK set "VULKAN_SDK=C:\VulkanSDK\%%V"
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%I in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
set "PF86=%ProgramFiles(x86)%"
set "PF=%ProgramFiles%"
for %%E in (BuildTools Community Professional Enterprise) do (
  if not defined VCVARS if exist "%PF86%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%PF86%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
  if not defined VCVARS if exist "%PF%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%PF%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
  echo [ERRO] vcvars64.bat nao encontrado
  exit /b 1
)
call "%VCVARS%" >nul
set "PATH=%PATH%;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links"
set "TARGETS="
:next
if "%~1"=="" goto build
set "TARGETS=%TARGETS% --target %~1"
shift
goto next
:build
cmake --build build\release %TARGETS%
exit /b %errorlevel%
