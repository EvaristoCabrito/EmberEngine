@echo off
rem JOGAR.bat - dois cliques e o jogo abre com o build local do editor. A arte vem de C:\EmberEngine\build.
setlocal EnableExtensions
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
set "ART=C:\EmberEngine\build"
set "EXE=%ART%\release\bin\Sandbox.exe"
cd /d "%HERE%"

if not exist "%EXE%" (
  echo Falta %EXE%
  goto :fim
)

set "GROUND=%ART%\ground\ground-v60.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v59.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v56.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v55.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v54.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v53.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v52.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v51.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v50.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v49.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v48.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v47.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v46.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v45.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v44.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v43.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v42.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v41.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v40.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v38.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v37.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v36.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v35.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v34.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v33.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v32.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v31.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v30.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v29.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v28.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v27.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v26.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v25.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v24.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v23.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v22.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v21.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v20.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v19.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v18.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v17.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v16.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v15.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v14.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v13.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v12.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v11.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v10.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v9.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v8.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v7.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v6.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v5.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v4.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v3.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground-v2.pak"
if not exist "%GROUND%" set "GROUND=%ART%\ground\ground.pak"
set "MESHES=%ART%\meshes\props-v36.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v35.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v34.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v33.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v32.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v17.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v16.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v15.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v14.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v13.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v12.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v11.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v10.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v9.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v8.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v6.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v5.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v4.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v3.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props-v2.pak"
if not exist "%MESHES%" set "MESHES=%ART%\meshes\props.pak"
rem battle.pak = o pacote de sprites atual (cada mudanca de sprite e cozinhada nele); battle-v2.pak (19/10) so se faltar
rem battle-new.pak = um pacote cozinhado com o jogo aberto (o battle.pak preso): entra no lugar dele aqui, o anterior guardado com a data
if not exist "%ART%\sprites\battle-new.pak" goto :semnovo
for /f %%D in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmm"') do set "STAMP=%%D"
move /y "%ART%\sprites\battle.pak" "%ART%\sprites\battle-%STAMP%-antes.pak" >nul
move /y "%ART%\sprites\battle-new.pak" "%ART%\sprites\battle.pak" >nul
:semnovo
set "SPRITES=%ART%\sprites\battle.pak"
if not exist "%SPRITES%" set "SPRITES=%ART%\sprites\battle-v2.pak"
set "HEROES="
if exist "%ART%\scenes\roster.json" set "HEROES=--heroes %ART%\scenes\roster.json"

for %%P in ("%GROUND%" "%SPRITES%" "%MESHES%" "%ART%\scenes" "%HERE%\content\maps\ember" "%HERE%\content\assets" "%HERE%\content\data\battle-rules.json") do (
  if not exist %%P (
    echo Falta %%~P
    echo Tire uma foto desta tela e mande para o Claude.
    goto :fim
  )
)

echo Abrindo o Ember...
"%EXE%" --title --ground "%GROUND%" --sprites "%SPRITES%" --meshes "%MESHES%" --scenes "%ART%\scenes" --maps "%HERE%\content\maps\ember" --game-assets "%HERE%\content\assets" %HEROES% > "%HERE%\JOGAR-log.txt" 2>&1
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
