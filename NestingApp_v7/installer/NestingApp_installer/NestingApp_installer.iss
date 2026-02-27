; ══════════════════════════════════════════════════════════════════════════════
; NestingApp v2.0 — Inno Setup Script
; Создаёт Setup.exe для установки на Windows
;
; Использование:
;   1. Соберите проект: cmake --build build --config Release
;   2. Запустите windeployqt на бинарнике
;   3. Скомпилируйте этот файл в Inno Setup Compiler
;      https://jrsoftware.org/isinfo.php
;
; Структура файлов должна быть:
;   installer/
;     NestingApp_installer.iss  (этот файл)
;   build_release/Release/
;     NestingApp.exe
;     Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll  (после windeployqt)
;     platforms/
;     styles/
;   components/
;     vc_redist.x64.exe         (скачать с Microsoft)
; ══════════════════════════════════════════════════════════════════════════════

#define AppName       "NestingApp"
#define AppVersion    "2.0.0"
#define AppPublisher  "MetalShop"
#define AppURL        "https://github.com/your-org/NestingApp"
#define AppExeName    "NestingApp.exe"
#define BuildDir      "..\build_release\Release"
#define ComponentsDir "..\installer\components"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=no
LicenseFile=..\LICENSE.txt
OutputDir=.\output
OutputBaseFilename=NestingApp_v2.0_Setup
SetupIconFile=..\resources\icon.ico
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ShowLanguageDialog=no
WizardStyle=modern
WizardResizable=yes
; Красивая тема
WizardImageFile=..\resources\wizard_banner.bmp
WizardSmallImageFile=..\resources\wizard_small.bmp

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать значок на рабочем столе"; GroupDescription: "Дополнительные значки:"; Flags: unchecked
Name: "quicklaunchicon"; Description: "Ярлык в панели задач"; GroupDescription: "Дополнительные значки:"; OnlyBelowVersion: 6.1; Flags: unchecked
Name: "fileassoc_dxf"; Description: "Открывать .dxf файлы через NestingApp"; GroupDescription: "Ассоциации файлов:"
Name: "fileassoc_nest"; Description: "Открывать .nest проекты через NestingApp"; GroupDescription: "Ассоциации файлов:"

[Files]
; Главный исполняемый файл
Source: "{#BuildDir}\{#AppExeName}";      DestDir: "{app}"; Flags: ignoreversion

; Qt библиотеки (результат windeployqt)
Source: "{#BuildDir}\Qt6Core.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Gui.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Widgets.dll";     DestDir: "{app}"; Flags: ignoreversion

; Qt плагины
Source: "{#BuildDir}\platforms\*";        DestDir: "{app}\platforms";  Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\styles\*";           DestDir: "{app}\styles";     Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\imageformats\*";     DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; Конфигурационные файлы
Source: "..\installer\config\settings.ini";    DestDir: "{app}\config"; Flags: onlyifdoesntexist
Source: "..\installer\config\materials.json";  DestDir: "{app}\config"; Flags: onlyifdoesntexist
Source: "..\installer\config\machines.json";   DestDir: "{app}\config"; Flags: onlyifdoesntexist

; Документация и примеры
Source: "..\README.md";                        DestDir: "{app}";        Flags: ignoreversion
Source: "..\installer\examples\*";            DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; VC++ Redistributable (запускается через [Run])
Source: "{#ComponentsDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Dirs]
Name: "{app}\config"
Name: "{app}\examples"
Name: "{app}\exports"
Name: "{userappdata}\NestingApp"
Name: "{userappdata}\NestingApp\templates"

[Icons]
Name: "{group}\{#AppName}";                      Filename: "{app}\{#AppExeName}"; Comment: "Раскладка деталей из листового металла"
Name: "{group}\Документация";                    Filename: "{app}\README.md"
Name: "{group}\Примеры DXF";                     Filename: "{app}\examples"
Name: "{group}\Удалить {#AppName}";              Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";                Filename: "{app}\{#AppExeName}"; Tasks: desktopicon; Comment: "Раскладка листового металла"
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: quicklaunchicon

[Registry]
; Ассоциация .dxf (если пользователь согласился)
Root: HKCU; Subkey: "Software\Classes\.dxf\OpenWithProgIds"; ValueType: string; ValueName: "NestingApp.dxf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc_dxf
Root: HKCU; Subkey: "Software\Classes\NestingApp.dxf"; ValueType: string; ValueName: ""; ValueData: "DXF Drawing"; Flags: uninsdeletekey; Tasks: fileassoc_dxf
Root: HKCU; Subkey: "Software\Classes\NestingApp.dxf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},0"; Tasks: fileassoc_dxf
Root: HKCU; Subkey: "Software\Classes\NestingApp.dxf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""; Tasks: fileassoc_dxf

; Ассоциация .nest
Root: HKCU; Subkey: "Software\Classes\.nest"; ValueType: string; ValueName: ""; ValueData: "NestingApp.Project"; Flags: uninsdeletevalue; Tasks: fileassoc_nest
Root: HKCU; Subkey: "Software\Classes\NestingApp.Project"; ValueType: string; ValueName: ""; ValueData: "NestingApp Project"; Flags: uninsdeletekey; Tasks: fileassoc_nest
Root: HKCU; Subkey: "Software\Classes\NestingApp.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},0"; Tasks: fileassoc_nest
Root: HKCU; Subkey: "Software\Classes\NestingApp.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""; Tasks: fileassoc_nest

; Путь установки для других программ
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"; Flags: uninsdeletekey

[Run]
; Visual C++ Redistributable (тихая установка, если файл есть)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "Установка Visual C++ Redistributable..."; \
    Check: VCRedistNeedsInstall; Flags: skipifdoesntexist

; Запуск программы после установки
Filename: "{app}\{#AppExeName}"; \
    Description: "Запустить {#AppName}"; \
    Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\config"
Type: filesandordirs; Name: "{app}\exports"

[Code]
// Проверяем нужно ли устанавливать VC++ Redist
function VCRedistNeedsInstall: Boolean;
var
  Version: String;
begin
  // Проверяем реестр на наличие VC++ 2022
  if RegQueryStringValue(HKLM,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64',
    'Version', Version) then
  begin
    // Версия >= 14.30 = VC++ 2022
    Result := (CompareStr(Version, 'v14.30.00000') < 0);
  end else
    Result := True; // Не найден — устанавливаем
end;

// Проверка минимальных требований при старте
function InitializeSetup: Boolean;
begin
  // Windows 10 или новее
  if not (GetWindowsVersion >= $0A000000) then
  begin
    MsgBox('NestingApp требует Windows 10 или новее.', mbError, MB_OK);
    Result := False;
    Exit;
  end;
  Result := True;
end;

// Показываем страницу «Что нового» после установки
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
  begin
    // Создаём папку для пользовательских данных
    ForceDirectories(ExpandConstant('{userappdata}\NestingApp'));
    ForceDirectories(ExpandConstant('{userappdata}\NestingApp\templates'));
    ForceDirectories(ExpandConstant('{userappdata}\NestingApp\exports'));
  end;
end;
