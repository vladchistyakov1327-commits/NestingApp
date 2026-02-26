# ══════════════════════════════════════════════════════════════════════════════
# NestingApp v2.0 — PowerShell установщик (Windows)
# Запуск: правая кнопка → «Выполнить с помощью PowerShell»
#         или: powershell -ExecutionPolicy Bypass -File install_windows.ps1
# ══════════════════════════════════════════════════════════════════════════════

#Requires -Version 5.1

param(
    [string]$QtPath = "",
    [switch]$SkipBuild = $false,
    [switch]$Silent = $false
)

$ErrorActionPreference = "Stop"
$Host.UI.RawUI.WindowTitle = "NestingApp — Установщик"

# Цвета
function Write-Step  { param($n, $t) Write-Host "`n[$n] $t" -ForegroundColor Cyan }
function Write-OK    { param($t) Write-Host "  [OK] $t" -ForegroundColor Green }
function Write-Warn  { param($t) Write-Host "  [!]  $t" -ForegroundColor Yellow }
function Write-Fail  { param($t) Write-Host "  [X]  $t" -ForegroundColor Red }

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        NestingApp v2.0 — Установщик Windows         ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir
$BuildDir   = Join-Path $ProjectDir "build_release"

# ─── Шаг 1: Проверка системы ──────────────────────────────────────────────────
Write-Step "1/6" "Проверка системы..."

$WinVer = [System.Environment]::OSVersion.Version
if ($WinVer.Major -lt 10) {
    Write-Fail "Требуется Windows 10 или новее. Текущая версия: $WinVer"
    exit 1
}
Write-OK "Windows $($WinVer.Major).$($WinVer.Minor)"

# Проверяем наличие winget
$HasWinget = (Get-Command winget -ErrorAction SilentlyContinue) -ne $null
if ($HasWinget) {
    Write-OK "winget доступен"
} else {
    Write-Warn "winget не найден. Зависимости нужно установить вручную."
}

# ─── Шаг 2: Visual C++ Redistributable ───────────────────────────────────────
Write-Step "2/6" "Visual C++ Redistributable 2022..."

$VCKey = "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64"
$NeedsVC = $true
if (Test-Path $VCKey) {
    $VCVer = (Get-ItemProperty $VCKey -ErrorAction SilentlyContinue).Version
    if ($VCVer -ge "v14.30.00000") { $NeedsVC = $false }
}

if ($NeedsVC) {
    if ($HasWinget) {
        Write-Host "  Установка VC++ Redist..." -ForegroundColor Gray
        & winget install Microsoft.VCRedist.2022.x64 --silent --accept-source-agreements --accept-package-agreements 2>$null
        Write-OK "VC++ Redistributable установлен"
    } else {
        $VCUrl  = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
        $VCFile = Join-Path $env:TEMP "vc_redist.x64.exe"
        Write-Host "  Скачивание VC++ Redist..." -ForegroundColor Gray
        Invoke-WebRequest -Uri $VCUrl -OutFile $VCFile -UseBasicParsing
        Start-Process $VCFile -ArgumentList "/install /quiet /norestart" -Wait
        Write-OK "VC++ Redistributable установлен"
    }
} else {
    Write-OK "VC++ Redistributable уже установлен"
}

# ─── Шаг 3: CMake ─────────────────────────────────────────────────────────────
Write-Step "3/6" "CMake..."

$CmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $CmakeCmd) {
    if ($HasWinget) {
        Write-Host "  Установка CMake..." -ForegroundColor Gray
        & winget install Kitware.CMake --silent --accept-source-agreements --accept-package-agreements
        # Обновляем PATH
        $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("PATH", "User")
    } else {
        Write-Fail "CMake не найден. Установите с https://cmake.org/download/"
        exit 1
    }
}
$CmakeVer = (cmake --version 2>&1 | Select-Object -First 1)
Write-OK "CMake: $CmakeVer"

# ─── Шаг 4: Qt6 ───────────────────────────────────────────────────────────────
Write-Step "4/6" "Qt6..."

# Поиск Qt6 в типичных местах
$QtSearchPaths = @(
    "C:\Qt\6.7.0\msvc2022_64",
    "C:\Qt\6.6.3\msvc2022_64",
    "C:\Qt\6.6.0\msvc2022_64",
    "C:\Qt\6.5.3\msvc2022_64",
    "C:\Qt\6.5.0\msvc2022_64",
    "$env:USERPROFILE\Qt\6.7.0\msvc2022_64",
    "$env:USERPROFILE\Qt\6.6.0\msvc2022_64",
    "C:\Qt6\msvc2022_64"
)

$QtDir = $QtPath
if (-not $QtDir) {
    foreach ($path in $QtSearchPaths) {
        if (Test-Path (Join-Path $path "bin\qmake.exe")) {
            $QtDir = $path
            break
        }
    }
}

if (-not $QtDir) {
    Write-Warn "Qt6 не найден автоматически."
    Write-Host ""
    Write-Host "  УСТАНОВКА Qt6:" -ForegroundColor Yellow
    Write-Host "  1. Откройте https://www.qt.io/download-qt-installer" -ForegroundColor Gray
    Write-Host "  2. Скачайте и запустите Qt Online Installer" -ForegroundColor Gray
    Write-Host "  3. Войдите или создайте бесплатный аккаунт Qt" -ForegroundColor Gray
    Write-Host "  4. Выберите компоненты:" -ForegroundColor Gray
    Write-Host "     Qt 6.x → MSVC 2022 64-bit (обязательно)" -ForegroundColor Green
    Write-Host "  5. После установки перезапустите этот скрипт" -ForegroundColor Gray
    Write-Host ""

    if (-not $Silent) {
        $QtDir = Read-Host "  Или введите путь к Qt вручную (пример: C:\Qt\6.7.0\msvc2022_64)"
    }

    if (-not $QtDir -or -not (Test-Path (Join-Path $QtDir "bin\qmake.exe"))) {
        Write-Fail "Qt6 не найден. Установка невозможна."
        Write-Host ""
        Write-Host "  Альтернатива: скачайте готовый бинарник NestingApp" -ForegroundColor Yellow
        Write-Host "  (если доступен в репозитории проекта)" -ForegroundColor Gray
        exit 1
    }
}

Write-OK "Qt6 найден: $QtDir"
$env:PATH = "$QtDir\bin;$env:PATH"
$Qt6CmakeDir = Join-Path $QtDir "lib\cmake\Qt6"

# ─── Шаг 5: Сборка ────────────────────────────────────────────────────────────
Write-Step "5/6" "Сборка NestingApp..."

if (-not $SkipBuild) {
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    # Ищем Visual Studio
    $VSWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $VSWhere)) {
        $VSWhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    }

    $VSGen = "Visual Studio 17 2022"
    if (Test-Path $VSWhere) {
        $VSVer = & $VSWhere -latest -property catalog_productLineVersion 2>$null
        if ($VSVer -eq "2019") { $VSGen = "Visual Studio 16 2019" }
        elseif ($VSVer -eq "2022") { $VSGen = "Visual Studio 17 2022" }
        Write-OK "Visual Studio $VSVer найдена"
    } else {
        Write-Warn "vswhere не найден, используем: $VSGen"
    }

    Write-Host "  Запуск CMake конфигурации..." -ForegroundColor Gray
    $SrcDir = Join-Path $ProjectDir "src"
    & cmake -B $BuildDir -S $SrcDir `
        -DCMAKE_BUILD_TYPE=Release `
        "-DQt6_DIR=$Qt6CmakeDir" `
        "-DCMAKE_PREFIX_PATH=$QtDir" `
        -G $VSGen -A x64 2>&1 | Tee-Object -Variable CmakeOut

    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Ошибка конфигурации CMake."
        Write-Host $CmakeOut
        exit 1
    }

    Write-Host "  Компиляция (Release)..." -ForegroundColor Gray
    & cmake --build $BuildDir --config Release --parallel 2>&1 | Tee-Object -Variable BuildOut

    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Ошибка компиляции."
        Write-Host $BuildOut
        exit 1
    }
    Write-OK "Сборка успешна"
}

# Ищем бинарник
$ExePath = Join-Path $BuildDir "Release\NestingApp.exe"
if (-not (Test-Path $ExePath)) {
    $ExePath = Join-Path $BuildDir "NestingApp.exe"
}
if (-not (Test-Path $ExePath)) {
    $ExePath = Get-ChildItem -Path $BuildDir -Name "NestingApp.exe" -Recurse | Select-Object -First 1
    if ($ExePath) { $ExePath = Join-Path $BuildDir $ExePath }
}

if (-not (Test-Path $ExePath)) {
    Write-Fail "NestingApp.exe не найден после сборки."
    exit 1
}

Write-OK "Бинарник: $ExePath"

# Деплой Qt DLL
Write-Host "  Копирование Qt библиотек (windeployqt)..." -ForegroundColor Gray
$WinDeployQt = Join-Path $QtDir "bin\windeployqt.exe"
if (Test-Path $WinDeployQt) {
    & $WinDeployQt --release --no-compiler-runtime $ExePath 2>&1 | Out-Null
    Write-OK "Qt DLL скопированы"
} else {
    Write-Warn "windeployqt не найден, Qt DLL нужно скопировать вручную"
}

# Копируем конфиги
$ExeDir   = Split-Path $ExePath
$ConfigSrc = Join-Path $ScriptDir "config"
$ConfigDst = Join-Path $ExeDir "config"
if (Test-Path $ConfigSrc) {
    if (-not (Test-Path $ConfigDst)) { New-Item -ItemType Directory $ConfigDst | Out-Null }
    Copy-Item "$ConfigSrc\*" $ConfigDst -Recurse -Force
    Write-OK "Конфиги скопированы"
}

# ─── Шаг 6: Ярлыки ────────────────────────────────────────────────────────────
Write-Step "6/6" "Создание ярлыков..."

$WshShell  = New-Object -ComObject WScript.Shell
$Desktop   = [System.Environment]::GetFolderPath("Desktop")
$Shortcut  = $WshShell.CreateShortcut("$Desktop\NestingApp.lnk")
$Shortcut.TargetPath       = $ExePath
$Shortcut.WorkingDirectory = Split-Path $ExePath
$Shortcut.Description      = "Раскладка деталей из листового металла"
$Shortcut.Save()
Write-OK "Ярлык на рабочем столе создан"

# ─── Готово ───────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              Установка завершена!                    ║" -ForegroundColor Green
Write-Host "╠══════════════════════════════════════════════════════╣" -ForegroundColor Green
Write-Host "║  Файл: $ExePath" -ForegroundColor Green
Write-Host "║  Ярлык создан на рабочем столе                      ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

if (-not $Silent) {
    $Launch = Read-Host "Запустить NestingApp сейчас? [Y/n]"
    if ($Launch -eq "" -or $Launch -imatch "^y") {
        Start-Process $ExePath
        Write-Host "Запущено!" -ForegroundColor Green
    }
}
