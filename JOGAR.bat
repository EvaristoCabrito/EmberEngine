@echo off
rem JOGAR.bat - dois cliques: junta este codigo com a sua pasta C:\EmberEngine (onde estao a arte e os builds),
rem compila e abre o mapa. Nada da sua pasta e apagado: antes de copiar, o que ela tem e guardado num commit.
setlocal EnableExtensions
set "GAME=C:\EmberEngine"
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

if not exist "%GAME%\build" (
  echo Nao achei a pasta do jogo com a arte: %GAME%\build
  echo Este arquivo precisa da sua pasta C:\EmberEngine de 20 GB.
  pause
  exit /b 1
)

rem 1. Se este JOGAR.bat veio de outra pasta (o ZIP baixado), traz o codigo novo para dentro de C:\EmberEngine.
if /i not "%HERE%"=="%GAME%" (
  echo Guardando o estado atual de %GAME% num commit antes de copiar...
  git -C "%GAME%" add -A >nul 2>&1
  git -C "%GAME%" commit -q -m "Antes do JOGAR.bat: estado da pasta guardado" >nul 2>&1
  echo Copiando o codigo novo para %GAME% ^(nada e apagado^)...
  for %%D in (apps engine shaders tools content cmake docs looks) do (
    if exist "%HERE%\%%D" robocopy "%HERE%\%%D" "%GAME%\%%D" /E /NFL /NDL /NJH /NJS /NP >nul
  )
  for %%F in (CLAUDE.md CMakeLists.txt CMakePresets.json build.bat JOGAR.bat) do (
    if exist "%HERE%\%%F" copy /Y "%HERE%\%%F" "%GAME%\%%F" >nul
  )
)

cd /d "%GAME%"

rem 2. Compila (release).
echo Compilando... ^(a primeira vez demora^)
call build.bat release
if errorlevel 1 (
  echo.
  echo A COMPILACAO FALHOU. Copie as mensagens de erro acima e cole para o Claude.
  pause
  exit /b 1
)

rem 3. Escolhe os arquivos de arte que existem e abre o mapa.
set "MAP=C:\Engine2\maps\ember\aldeia015.json"
set "GROUND=build\ground\ground-v2.pak"
if not exist "%GROUND%" set "GROUND=build\ground\ground.pak"
set "SCENE=build\engine2-scenes\aldeia015.scene.json"
set "HEROES="
if exist "build\engine2-scenes\roster.json" set "HEROES=--heroes build\engine2-scenes\roster.json"
set "BATTLE="
if exist "content\engine2\battles\aldeia015.battle.json" set "BATTLE=--battle-rules content\engine2\battles\aldeia015.battle.json"

for %%P in ("%MAP%" "%GROUND%" "%SCENE%" "build\sprites\battle.pak" "build\meshes\props.pak") do (
  if not exist %%P (
    echo Falta o arquivo %%~P
    echo Cole esta mensagem para o Claude.
    pause
    exit /b 1
  )
)

echo Abrindo o jogo...
start "" "build\release\bin\Sandbox.exe" --map "%MAP%" --ground "%GROUND%" --scene "%SCENE%" --sprites build\sprites\battle.pak --meshes build\meshes\props.pak %HEROES% %BATTLE%
exit /b 0
