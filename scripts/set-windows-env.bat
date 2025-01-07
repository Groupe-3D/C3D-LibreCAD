@echo off

if "%Qt6_Dir%"=="" goto SetEnv
if "%NSIS_DIR%"=="" goto SetEnv
goto Exit

:SetEnv
set Qt_DIR=C:\Qt\5.15.2
set NSIS_DIR=C:\PROGRA~2\NSIS
set MINGW_VER=mingw81_32

if exist custom-windows.bat call custom-windows.bat
set PATH=%Qt_Dir%\%MINGW_VER%\bin;%Qt_Dir%\..\Tools\%MINGW_VER%\bin;%NSIS_DIR%;%PATH%

:Exit
echo on
