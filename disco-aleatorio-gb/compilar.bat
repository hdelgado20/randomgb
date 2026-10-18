@echo off
cd /d "%~dp0"
if "%GBDK_HOME%"=="" set "GBDK_HOME=C:\gbdk"
if not exist "%GBDK_HOME%\bin\lcc.exe" goto :nogbdk
where python >nul 2>nul
if errorlevel 1 goto :nopy
python tools\mkdata.py collection.txt src\preload.h
:build
"%GBDK_HOME%\bin\lcc.exe" -Wm-yt0x03 -Wm-yo4 -Wm-ya1 -Wm-ynDISCOALEATO -o DiscoAleatorio.gb src\main.c
if errorlevel 1 goto :err
echo.
echo LISTO: DiscoAleatorio.gb
pause
exit /b 0
:nopy
echo No hay Python: se usara el preload.h que ya viene hecho.
goto :build
:nogbdk
echo No encuentro GBDK en %GBDK_HOME%
echo Descargalo de github.com/gbdk-2020/gbdk-2020/releases y extraelo en C:\gbdk
pause
exit /b 1
:err
echo Algo ha fallado. Lee el mensaje de arriba.
pause
exit /b 1
