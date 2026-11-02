@echo off
rem 用法: deploy.bat <exe 绝对路径> <Qt 目录>
rem 正常走 windeployqt；它跑不起来时退回手动复制（结果一样，只是不裁剪）
setlocal
set ROOT=%~dp0..
set TARGET=%~1
set TARGET=%TARGET:/=\\%
set QTDIR=%~2

if not exist "%TARGET%" (
  echo [deploy] cannot find: %TARGET%
  exit /b 2
)
if not exist "%QTDIR%\bin\windeployqt.exe" (
  echo [deploy] cannot find windeployqt: %QTDIR%\bin
  exit /b 3
)

if exist "%ROOT%\dist" (
  if exist "%ROOT%\dist\qml" rmdir /s /q "%ROOT%\dist\qml"
  if exist "%ROOT%\dist\json" rmdir /s /q "%ROOT%\dist\json"
) else (
  mkdir "%ROOT%\dist" || exit /b 4
)

copy /Y "%TARGET%" "%ROOT%\dist\Riel Minecraft Launcher.exe" >nul || exit /b 5
xcopy /E /I /Y "%ROOT%\src\qml" "%ROOT%\dist\qml" >nul || exit /b 6
xcopy /E /I /Y "%ROOT%\src\json" "%ROOT%\dist\json" >nul || exit /b 7

"%QTDIR%\bin\windeployqt.exe" --qmldir "%ROOT%\src\qml" --no-translations --compiler-runtime "%ROOT%\dist\Riel Minecraft Launcher.exe"
if not errorlevel 1 goto deployed

echo [deploy] windeployqt unavailable, copying Qt runtime manually
copy /Y "%QTDIR%\bin\Qt6*.dll" "%ROOT%\dist\" >nul
copy /Y "%QTDIR%\bin\libstdc++-6.dll" "%ROOT%\dist\" >nul
copy /Y "%QTDIR%\bin\libgcc_s_seh-1.dll" "%ROOT%\dist\" >nul
copy /Y "%QTDIR%\bin\libwinpthread-1.dll" "%ROOT%\dist\" >nul

for %%P in (platforms imageformats tls networkinformation iconengines styles qmltooling) do (
  if exist "%QTDIR%\plugins\%%P" xcopy /E /I /Y "%QTDIR%\plugins\%%P" "%ROOT%\dist\%%P" >nul
)

if exist "%QTDIR%\qml" xcopy /E /I /Y "%QTDIR%\qml" "%ROOT%\dist\qml" >nul

if not exist "%ROOT%\dist\platforms\qwindows.dll" (
  echo [deploy] failed: platforms/qwindows.dll missing
  exit /b 8
)
if not exist "%ROOT%\dist\qml\QtQuick\Controls\qmldir" (
  echo [deploy] failed: Qt qml modules missing
  exit /b 8
)

:deployed
echo [deploy] done: %ROOT%\dist
endlocal
