@echo off
rem JOGAR.bat - dois cliques e o jogo abre. Nao compila nada: o Sandbox.exe ja vem pronto em jogo\.
rem A arte (20 GB) vem da sua pasta C:\EmberEngine\build; nada nela e mudado.
setlocal EnableExtensions
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
set "ART=C:\EmberEngine\build"
set "EXE=%HERE%\jogo\Sandbox.exe"
cd /d "%HERE%"

if not exist "%EXE%" (
  echo Falta %EXE%
  goto :fim
)

set "MAP=C:\Engine2\maps\ember\aldeia015.json"
set "GROUND=%ART%\ground\ground-v2.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground.pak"
set "SCENE=%ART%\engine2-scenes\aldeia015.scene.json"
set "HEROES="
if exist "%ART%\engine2-scenes\roster.json" set "HEROES=--heroes %ART%\engine2-scenes\roster.json"

for %%P in ("%MAP%" "%GROUND%" "%SCENE%" "%ART%\sprites\battle.pak" "%ART%\meshes\props.pak") do (
  if not exist %%P (
    echo Falta o arquivo %%~P
    echo Tire uma foto desta tela e mande para o Claude.
    goto :fim
  )
)

echo Abrindo o Ember...
"%EXE%" --map "%MAP%" --ground "%GROUND%" --scene "%SCENE%" --sprites "%ART%\sprites\battle.pak" --meshes "%ART%\meshes\props.pak" %HEROES% --battle-rules content\engine2\battles\aldeia015.battle.json > "%HERE%\JOGAR-log.txt" 2>&1
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
