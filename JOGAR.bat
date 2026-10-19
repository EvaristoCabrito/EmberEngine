@echo off
rem JOGAR.bat - dois cliques e o jogo abre: sempre o ultimo build local (build\release\bin\Sandbox.exe),
rem o mesmo que o Claude compila depois de cada mudanca. A arte vem de C:\EmberEngine\build.
setlocal EnableExtensions
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
set "ART=C:\EmberEngine\build"
set "EXE=C:\EmberEngine\build\release\bin\Sandbox.exe"
cd /d "%HERE%"

if not exist "%EXE%" (
  echo Falta %EXE%
  goto :fim
)

set "GROUND=%ART%\ground\ground-v3.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v2.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground.pak"
set "MESHES=%ART%\meshes\props-v2.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props.pak"
set "HEROES="
if exist "%ART%\engine2-scenes\roster.json" set "HEROES=--heroes %ART%\engine2-scenes\roster.json"

for %%P in ("%GROUND%" "%ART%\sprites\battle.pak" "%MESHES%" "%ART%\engine2-scenes" "C:\Engine2\maps\ember") do (
  if not exist %%P (
    echo Falta %%~P
    echo Tire uma foto desta tela e mande para o Claude.
    goto :fim
  )
)

echo Abrindo o Ember...
"%EXE%" --title --ground "%GROUND%" --sprites "%ART%\sprites\battle.pak" --meshes "%MESHES%" --scenes "%ART%\engine2-scenes" --maps "C:\Engine2\maps\ember" %HEROES% > "%HERE%\JOGAR-log.txt" 2>&1
set "RUNERR=%ERRORLEVEL%"
if not "%RUNERR%"=="0" (
  echo.
  echo O jogo fechou com erro ^(codigo %RUNERR%^). Ultimas linhas:
  powershell -NoProfile -Command "Get-Content '%HERE%\JOGAR-log.txt' -Tail 25"
  echo Tire uma foto desta tela e mande para o Claude.
  goto :fim
)
exit /b 0

:fim
echo.
pause
exit /b 1
