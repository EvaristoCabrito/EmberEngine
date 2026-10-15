@echo off
rem Build EmberEngine: build.bat [debug|release]
rem Acha sozinho o Visual Studio (qualquer edicao 2022/2026), o CMake e o Vulkan SDK; diz o que falta.
setlocal EnableExtensions
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
cd /d "%~dp0"

rem Vulkan SDK: usa o da variavel, senao o mais novo em C:\VulkanSDK.
if not defined VULKAN_SDK for /f "delims=" %%V in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do if not defined VULKAN_SDK set "VULKAN_SDK=C:\VulkanSDK\%%V"
if not defined VULKAN_SDK (
  echo [ERRO] Nao achei o Vulkan SDK ^(C:\VulkanSDK^). Instale em https://vulkan.lunarg.com
  exit /b 1
)
echo Vulkan SDK: %VULKAN_SDK%

rem Visual Studio: pergunta ao vswhere; se nao der, tenta os caminhos comuns.
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
  echo [ERRO] Nao achei o Visual Studio com C++ ^(vcvars64.bat^).
  exit /b 1
)
if not exist "%VCVARS%" (
  echo [ERRO] Nao existe: "%VCVARS%"
  exit /b 1
)
echo Visual Studio: %VCVARS%
call "%VCVARS%" >nul
if errorlevel 1 (
  echo [ERRO] vcvars64.bat falhou.
  exit /b 1
)

set "PATH=%PATH%;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links"
where cmake >nul 2>&1 || (echo [ERRO] Nao achei o cmake. & exit /b 1)
where ninja >nul 2>&1 || (echo [ERRO] Nao achei o ninja ^(vem com o componente CMake do Visual Studio^). & exit /b 1)

echo Configurando %CONFIG%...
cmake --preset %CONFIG%
if errorlevel 1 (
  echo [ERRO] cmake --preset %CONFIG% falhou.
  exit /b 1
)
echo Compilando %CONFIG%...
cmake --build --preset %CONFIG%
if errorlevel 1 (
  echo [ERRO] a compilacao falhou ^(erros acima, procure por "error"^).
  exit /b 1
)
exit /b 0
