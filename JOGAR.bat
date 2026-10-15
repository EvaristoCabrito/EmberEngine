@echo off
rem JOGAR.bat - dois cliques: junta este codigo com a sua pasta C:\EmberEngine (onde estao a arte e os builds),
rem compila e abre o mapa. Nada da sua pasta e apagado: antes de copiar, o que ela tem e guardado num commit.
rem Tudo o que acontece fica em C:\EmberEngine\JOGAR-log.txt e a janela nunca fecha sozinha.
setlocal EnableExtensions
set "GAME=C:\EmberEngine"
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
set "LOG=%GAME%\JOGAR-log.txt"

if not exist "%GAME%\build" (
  echo Nao achei a pasta do jogo com a arte: %GAME%\build
  echo Este arquivo precisa da sua pasta C:\EmberEngine de 20 GB.
  goto :fim
)

echo JOGAR.bat %DATE% %TIME% > "%LOG%"
echo veio de: %HERE% >> "%LOG%"

rem 1. Se este JOGAR.bat veio de outra pasta (o ZIP baixado), traz o codigo novo para dentro de C:\EmberEngine.
if /i not "%HERE%"=="%GAME%" (
  echo Guardando o estado atual de %GAME% num commit antes de copiar...
  git -C "%GAME%" add -A >> "%LOG%" 2>&1
  git -C "%GAME%" commit -q -m "Antes do JOGAR.bat: estado da pasta guardado" >> "%LOG%" 2>&1
  echo Copiando o codigo novo para %GAME% ^(nada e apagado^)...
  for %%D in (apps engine shaders tools content cmake docs looks) do (
    if exist "%HERE%\%%D" robocopy "%HERE%\%%D" "%GAME%\%%D" /E /NFL /NDL /NJH /NJS /NP >nul
  )
  for %%F in (CLAUDE.md CMakeLists.txt CMakePresets.json build.bat) do (
    if exist "%HERE%\%%F" copy /Y "%HERE%\%%F" "%GAME%\%%F" >nul
  )
  if exist "%HERE%\JOGAR.bat" copy /Y "%HERE%\JOGAR.bat" "%GAME%\JOGAR-novo.bat" >nul
)

cd /d "%GAME%"

rem 2. Compila (release). A saida vai para a tela e para o log.
echo.
echo Compilando... ^(a primeira vez demora varios minutos^)
echo ==== build.bat release ==== >> "%LOG%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "cmd /c 'call build.bat release 2>&1' | Tee-Object -FilePath '%LOG%' -Append; exit $LASTEXITCODE"
set "BUILDERR=%ERRORLEVEL%"

set "EXE=build\release\bin\Sandbox.exe"
if not "%BUILDERR%"=="0" goto :falhou
if not exist "%EXE%" goto :falhou
goto :arte

:falhou
echo.
echo ================================================================
echo  A COMPILACAO FALHOU - o Sandbox.exe nao foi gerado.
echo  Os erros:
echo ================================================================
findstr /i /c:"error" /c:"[ERRO]" /c:"fatal" "%LOG%" > "%GAME%\JOGAR-erros.txt"
powershell -NoProfile -Command "Get-Content '%GAME%\JOGAR-erros.txt' | Select-Object -First 30"
echo ================================================================
echo  Tire uma foto desta tela e mande para o Claude,
echo  ou mande o arquivo %GAME%\JOGAR-log.txt
echo ================================================================
goto :fim

:arte
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
    echo Falta o arquivo %%~P >> "%LOG%"
    echo Tire uma foto desta tela e mande para o Claude.
    goto :fim
  )
)

echo.
echo Abrindo o jogo... ^(esta janela fica aberta; se o jogo fechar, o motivo aparece aqui^)
echo ==== Sandbox ==== >> "%LOG%"
"%EXE%" --map "%MAP%" --ground "%GROUND%" --scene "%SCENE%" --sprites build\sprites\battle.pak --meshes build\meshes\props.pak %HEROES% %BATTLE%
set "RUNERR=%ERRORLEVEL%"
echo Sandbox saiu com codigo %RUNERR% >> "%LOG%"
echo.
echo O jogo fechou ^(codigo %RUNERR%^). Se fechou sozinho, tire uma foto desta tela e mande para o Claude.

:fim
echo.
pause
exit /b 0
