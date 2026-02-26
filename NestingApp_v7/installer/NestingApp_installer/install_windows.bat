@echo off
chcp 65001 >nul
title NestingApp — Установка зависимостей и сборка

echo.
echo ╔══════════════════════════════════════════════════════╗
echo ║        NestingApp v2.0 — Установщик Windows         ║
echo ║       Раскладка деталей из листового металла        ║
echo ╚══════════════════════════════════════════════════════╝
echo.

:: ─── Проверка прав администратора ────────────────────────────────────────────
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Требуются права администратора.
    echo     Нажмите правой кнопкой → «Запуск от имени администратора»
    pause
    exit /b 1
)

:: ─── Определяем архитектуру ───────────────────────────────────────────────────
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCH=x64
) else (
    set ARCH=x86
    echo [!] Внимание: программа оптимизирована для 64-битных систем.
)

echo [1/6] Проверка наличия менеджера пакетов winget...
where winget >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] winget не найден.
    echo     Скачайте вручную App Installer из Microsoft Store.
    echo     Или используйте ручную установку (см. ниже).
    goto MANUAL_INSTALL
)

:: ─── Установка Visual C++ Redistributable ────────────────────────────────────
echo.
echo [2/6] Установка Visual C++ 2022 Redistributable...
winget install Microsoft.VCRedist.2022.x64 --silent --accept-source-agreements --accept-package-agreements
if %errorLevel% neq 0 (
    echo [~] VC++ Redist уже установлен или ошибка — продолжаем...
)

:: ─── Установка CMake ──────────────────────────────────────────────────────────
echo.
echo [3/6] Проверка CMake...
where cmake >nul 2>&1
if %errorLevel% neq 0 (
    echo     Установка CMake...
    winget install Kitware.CMake --silent --accept-source-agreements --accept-package-agreements
    if %errorLevel% neq 0 (
        echo [!] Не удалось установить CMake автоматически.
        echo     Скачайте вручную: https://cmake.org/download/
        goto MANUAL_INSTALL
    )
    :: Обновляем PATH
    set "PATH=%PATH%;C:\Program Files\CMake\bin"
) else (
    echo     CMake уже установлен.
)

:: ─── Установка Qt6 через winget ───────────────────────────────────────────────
echo.
echo [4/6] Проверка Qt6...
if exist "C:\Qt\6.7.0\msvc2022_64\bin\qmake.exe" (
    set QT_DIR=C:\Qt\6.7.0\msvc2022_64
    echo     Qt6 найден: %QT_DIR%
    goto QT_FOUND
)
if exist "C:\Qt\6.6.0\msvc2022_64\bin\qmake.exe" (
    set QT_DIR=C:\Qt\6.6.0\msvc2022_64
    echo     Qt6 найден: %QT_DIR%
    goto QT_FOUND
)
if exist "C:\Qt\6.5.0\msvc2022_64\bin\qmake.exe" (
    set QT_DIR=C:\Qt\6.5.0\msvc2022_64
    echo     Qt6 найден: %QT_DIR%
    goto QT_FOUND
)

echo     Qt6 не найден. Запускаем установщик Qt...
echo.
echo     ВАЖНО: Требуется Qt 6.5+ (MSVC 2022, x64)
echo     1. Зайдите на https://www.qt.io/download-qt-installer
echo     2. Скачайте Qt Online Installer
echo     3. При установке выберите: Qt 6.x → MSVC 2022 64-bit
echo     4. После установки перезапустите этот скрипт
echo.
echo     Примерный путь Qt после установки:
echo     C:\Qt\6.x.x\msvc2022_64\
echo.

:: Пробуем установить через winget (если доступно)
winget search "Qt 6" --accept-source-agreements 2>nul | find "qt.qt6" >nul 2>&1
if %errorLevel% equ 0 (
    winget install qt.qt6.65.msvc2022_64 --silent --accept-source-agreements 2>nul
    if exist "C:\Qt\6.5.0\msvc2022_64\bin\qmake.exe" (
        set QT_DIR=C:\Qt\6.5.0\msvc2022_64
        goto QT_FOUND
    )
)

set /p QT_DIR="Введите путь к Qt вручную (например C:\Qt\6.7.0\msvc2022_64): "
if not exist "%QT_DIR%\bin\qmake.exe" (
    echo [!] Неверный путь Qt. Установка прервана.
    pause
    exit /b 1
)

:QT_FOUND
echo     Qt DIR: %QT_DIR%
set "PATH=%QT_DIR%\bin;%PATH%"
set "Qt6_DIR=%QT_DIR%\lib\cmake\Qt6"

:: ─── Сборка NestingApp ────────────────────────────────────────────────────────
echo.
echo [5/6] Сборка NestingApp...

set BUILD_DIR=%~dp0..\build_release
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cmake -B "%BUILD_DIR%" -S "%~dp0.." ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DQt6_DIR="%Qt6_DIR%" ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -G "Visual Studio 17 2022" -A x64

if %errorLevel% neq 0 (
    echo [!] Ошибка конфигурации CMake.
    echo     Проверьте что Visual Studio 2022 установлена с C++ компонентами.
    pause
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release --parallel

if %errorLevel% neq 0 (
    echo [!] Ошибка компиляции.
    pause
    exit /b 1
)

:: ─── Деплой Qt DLL ────────────────────────────────────────────────────────────
echo.
echo [6/6] Копирование Qt библиотек (windeployqt)...
set EXE_PATH=%BUILD_DIR%\Release\NestingApp.exe
if not exist "%EXE_PATH%" (
    set EXE_PATH=%BUILD_DIR%\NestingApp.exe
)

if exist "%EXE_PATH%" (
    "%QT_DIR%\bin\windeployqt.exe" --release --no-compiler-runtime "%EXE_PATH%"
    echo     DLL скопированы.
) else (
    echo [!] Бинарник не найден по пути: %EXE_PATH%
)

:: ─── Создание ярлыка на рабочем столе ────────────────────────────────────────
echo.
echo Создание ярлыка на рабочем столе...
set DESKTOP=%USERPROFILE%\Desktop
set SHORTCUT=%DESKTOP%\NestingApp.lnk
powershell -NoProfile -Command ^
    "$ws = New-Object -ComObject WScript.Shell; ^
     $sc = $ws.CreateShortcut('%SHORTCUT%'); ^
     $sc.TargetPath = '%EXE_PATH%'; ^
     $sc.WorkingDirectory = '%BUILD_DIR%\Release'; ^
     $sc.Description = 'NestingApp - Раскладка листового металла'; ^
     $sc.Save()"

echo.
echo ╔══════════════════════════════════════════════════════╗
echo ║              Установка завершена!                    ║
echo ╠══════════════════════════════════════════════════════╣
echo ║  Исполняемый файл:                                   ║
echo ║  %EXE_PATH%
echo ║                                                      ║
echo ║  Ярлык создан на рабочем столе: NestingApp           ║
echo ╚══════════════════════════════════════════════════════╝
echo.

set /p LAUNCH="Запустить NestingApp сейчас? (Y/N): "
if /i "%LAUNCH%"=="Y" start "" "%EXE_PATH%"

pause
exit /b 0

:MANUAL_INSTALL
echo.
echo ══════════════════════════════════════════════════════
echo  РУЧНАЯ УСТАНОВКА — инструкция
echo ══════════════════════════════════════════════════════
echo.
echo 1. Visual C++ Redistributable 2022 (x64):
echo    https://aka.ms/vs/17/release/vc_redist.x64.exe
echo.
echo 2. CMake 3.20+:
echo    https://cmake.org/download/
echo.
echo 3. Qt 6.5+ (MSVC 2022 x64):
echo    https://www.qt.io/download-qt-installer
echo    Компоненты: Qt 6.x → MSVC 2022 64-bit
echo.
echo 4. Visual Studio 2022 с C++ компонентами:
echo    https://visualstudio.microsoft.com/
echo    Нужен компонент: «Разработка классических приложений на C++»
echo.
echo 5. После установки всех зависимостей:
echo    Запустите этот скрипт снова, или выполните вручную:
echo.
echo    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
echo    cmake --build build --config Release
echo.
pause
exit /b 1
