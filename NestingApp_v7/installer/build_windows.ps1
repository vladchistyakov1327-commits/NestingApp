# ============================================================
#  build_windows.ps1  —  Сборка NestingApp для Windows
#
#  Запуск: правая кнопка → "Выполнить с PowerShell"
#  ИЛИ из Developer PowerShell:  .\build_windows.ps1
#
#  Требования:
#    - Visual Studio 2022 (с компонентом "Разработка классических приложений C++")
#    - Qt6 (6.5+)  →  https://www.qt.io/download-qt-installer
#    - CMake 3.20+ →  https://cmake.org/download/
#    - Inno Setup 6 → https://jrsoftware.org/isinfo.php  (для создания Setup.exe)
# ============================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Настройки ────────────────────────────────────────────────────────────────
$QtDir     = "C:\Qt\6.5.3\msvc2019_64"   # <-- поменяй под свой путь Qt
$BuildDir  = "$PSScriptRoot\..\build_release"
$DistDir   = "$PSScriptRoot\..\dist"
$InnoSetup = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

# ── Функции ──────────────────────────────────────────────────────────────────
function Write-Step($msg) {
    Write-Host "`n==> $msg" -ForegroundColor Cyan
}

function Assert-Exists($path, $hint) {
    if (-not (Test-Path $path)) {
        Write-Host "ОШИБКА: не найдено: $path" -ForegroundColor Red
        Write-Host "Подсказка: $hint" -ForegroundColor Yellow
        exit 1
    }
}

# ── Проверки ─────────────────────────────────────────────────────────────────
Write-Step "Проверка окружения"

Assert-Exists $QtDir         "Установи Qt6 с https://www.qt.io/download-qt-installer и укажи путь в \$QtDir"
Assert-Exists (Get-Command cmake -ErrorAction SilentlyContinue).Source `
              "Установи CMake с https://cmake.org/download/"

Write-Host "Qt6:   $QtDir" -ForegroundColor Green
Write-Host "CMake: $(cmake --version | head -1)" -ForegroundColor Green

# ── Сборка ───────────────────────────────────────────────────────────────────
Write-Step "Конфигурация CMake"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

cmake -B $BuildDir -S "$PSScriptRoot\.." `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="$QtDir" `
      -G "Visual Studio 17 2022" -A x64

Write-Step "Компиляция (Release)"
cmake --build $BuildDir --config Release --parallel

$ExePath = "$BuildDir\Release\NestingApp.exe"
Assert-Exists $ExePath "Сборка завершилась с ошибкой"
Write-Host "Собран: $ExePath" -ForegroundColor Green

# ── Деплой Qt DLL ─────────────────────────────────────────────────────────────
Write-Step "Деплой Qt библиотек (windeployqt)"

$WinDeploy = "$QtDir\bin\windeployqt.exe"
Assert-Exists $WinDeploy "Не найден windeployqt.exe в $QtDir\bin"

& $WinDeploy --release --no-compiler-runtime --no-translations "$ExePath"

# Копируем всё в отдельную папку deploy для установщика
$DeployDir = "$BuildDir\deploy"
New-Item -ItemType Directory -Force -Path $DeployDir | Out-Null
Copy-Item "$BuildDir\Release\*" -Destination $DeployDir -Recurse -Force

Write-Host "Qt DLL задеплоены в: $DeployDir" -ForegroundColor Green

# ── Скачиваем VC++ Redist (если нет) ─────────────────────────────────────────
Write-Step "Проверка VC++ Redistributable"

$RedistDir  = "$PSScriptRoot\redist"
$RedistFile = "$RedistDir\vc_redist.x64.exe"
New-Item -ItemType Directory -Force -Path $RedistDir | Out-Null

if (-not (Test-Path $RedistFile)) {
    Write-Host "Скачиваем vc_redist.x64.exe..." -ForegroundColor Yellow
    $RedistUrl = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
    Invoke-WebRequest -Uri $RedistUrl -OutFile $RedistFile -UseBasicParsing
    Write-Host "Скачан: $RedistFile" -ForegroundColor Green
} else {
    Write-Host "Уже есть: $RedistFile" -ForegroundColor Green
}

# ── Создание установщика ─────────────────────────────────────────────────────
Write-Step "Создание Setup.exe (Inno Setup)"

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

if (Test-Path $InnoSetup) {
    # Подставляем правильный путь к deploy папке в .iss
    $IssContent = Get-Content "$PSScriptRoot\NestingApp.iss" -Raw
    $IssContent = $IssContent -replace '#define AppDir.*', "#define AppDir   `"$DeployDir`""
    $TmpIss = "$env:TEMP\NestingApp_build.iss"
    $IssContent | Set-Content $TmpIss -Encoding UTF8

    & $InnoSetup $TmpIss
    Write-Host "`n✔ Установщик создан: $DistDir\NestingApp_v2_Setup.exe" -ForegroundColor Green
} else {
    Write-Host "Inno Setup не найден — пропускаем создание Setup.exe" -ForegroundColor Yellow
    Write-Host "Установи с: https://jrsoftware.org/isinfo.php" -ForegroundColor Yellow

    # Создаём ZIP как альтернативу
    $ZipPath = "$DistDir\NestingApp_v2_portable.zip"
    Compress-Archive -Path "$DeployDir\*" -DestinationPath $ZipPath -Force
    Write-Host "✔ Portable ZIP создан: $ZipPath" -ForegroundColor Green
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " СБОРКА ЗАВЕРШЕНА УСПЕШНО!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Установщик: $DistDir\NestingApp_v2_Setup.exe"
Write-Host " Portable:   $DeployDir\"
