@echo off
rem 用法: deploy.bat <exe 绝对路径> <Qt 目录>
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

if exist "%ROOT%\dist" rmdir /s /q "%ROOT%\dist"
mkdir "%ROOT%\dist" || exit /b 4
copy /Y "%TARGET%" "%ROOT%\dist\rml.exe" >nul || exit /b 5
xcopy /E /I /Y "%ROOT%\src\qml" "%ROOT%\dist\qml" >nul || exit /b 6
xcopy /E /I /Y "%ROOT%\src\json" "%ROOT%\dist\json" >nul || exit /b 7

"%QTDIR%\bin\windeployqt.exe" --qmldir "%ROOT%\src\qml" --no-translations --compiler-runtime "%ROOT%\dist\rml.exe"
if errorlevel 1 (
  echo [deploy] windeployqt failed, run it manually:
  echo   "%QTDIR%\bin\windeployqt.exe" --qmldir "%ROOT%\src\qml" "%ROOT%\dist\rml.exe"
  exit /b 8
)

echo [deploy] done: %ROOT%\dist
endlocal
