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

rem ---- 3. запускаем ----
%PY% pc_monitor.py
echo.
pause
