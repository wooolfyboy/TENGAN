@echo off
chcp 65001 >nul
cd /d "%~dp0"
title Radio Radar - PC monitor

rem ---- 1. ищем Python ----
set "PY="
py -3 -c "print(1)" >nul 2>nul && set "PY=py -3"
if not defined PY (
  python -c "print(1)" >nul 2>nul && set "PY=python"
)
if not defined PY (
  echo.
  echo [!!] Python не найден.
  echo      Скачай его с python.org, при установке поставь галочку "Add python.exe to PATH",
  echo      потом снова запусти этот файл.
  echo.
  pause
  exit /b 1
)

rem ---- 2. первый запуск: ставим две библиотеки ----
%PY% -c "import psutil, serial" >nul 2>nul
if errorlevel 1 (
  echo Первый запуск: ставлю библиотеки psutil и pyserial. Это один раз, около минуты...
  %PY% -m pip install --user --disable-pip-version-check psutil pyserial
  %PY% -c "import psutil, serial" >nul 2>nul
  if errorlevel 1 (
    echo.
    echo [!!] Не получилось поставить библиотеки. Сфотографируй это окно и пришли.
    echo.
    pause
    exit /b 1
  )
  echo Готово!
)

rem ---- 2б. по желанию: pycaw — чтобы палка видела громкость в процентах ----
%PY% -c "import pycaw" >nul 2>nul
if errorlevel 1 (
  echo Ставлю библиотеку pycaw ^(громкость в процентах на палке^)...
  %PY% -m pip install --user --disable-pip-version-check pycaw >nul 2>nul
)

rem ---- 2в. по желанию: winrt — чтобы палка видела, что сейчас играет ----
%PY% -c "import winrt.windows.media.control" >nul 2>nul
if errorlevel 1 (
  echo Ставлю библиотеки winrt ^(название трека на палке^)...
  %PY% -m pip install --user --disable-pip-version-check winrt-runtime winrt-Windows.Media.Control winrt-Windows.Foundation winrt-Windows.Foundation.Collections >nul 2>nul
)

rem ---- 2г. по желанию: numpy + PyAudioWPatch — звук ноутбука на палке (раздел Sound) ----
%PY% -c "import numpy, pyaudiowpatch" >nul 2>nul
if errorlevel 1 (
  echo Ставлю библиотеки для звука ноутбука ^(numpy, PyAudioWPatch^), около минуты...
  %PY% -m pip install --user --disable-pip-version-check numpy PyAudioWPatch >nul 2>nul
)

rem ---- 3. запускаем ----
%PY% pc_monitor.py
echo.
pause
