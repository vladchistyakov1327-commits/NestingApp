; ============================================================
;  NestingApp — Inno Setup installer script
;  Сборка: iscc NestingApp.iss
;  Требования: Inno Setup 6.x (https://jrsoftware.org/isinfo.php)
; ============================================================

#define AppName      "NestingApp"
#define AppVersion   "2.0"
#define AppPublisher "MetalShop"
#define AppExeName   "NestingApp.exe"
#define AppDir       "..\build_release"

[Setup]
AppId={{A7F3C2D1-4E5B-4F6A-8C9D-0E1F2A3B4C5D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=NestingApp_v2_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64os
ArchitecturesAllowed=x64os
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}
WizardStyle=modern
SetupIconFile=..\assets\icon.ico
; Если нет иконки — закомментируй строку выше

[Languages]
Name: "russian";    MessagesFile: "compiler:Languages\Russian.isl"
Name: "english";    MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";   Description: "{cm:CreateDesktopIcon}";   GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunch";   Description: "Ярлык в панели задач";     GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1

[Files]
; ── Основной исполняемый файл ──────────────────────────────
Source: "{#AppDir}\{#AppExeName}";     DestDir: "{app}";            Flags: ignoreversion

; ── Qt6 DLL (windeployqt копирует их в build_release) ──────
Source: "{#AppDir}\Qt6Core.dll";       DestDir: "{app}";            Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Gui.dll";        DestDir: "{app}";            Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Widgets.dll";    DestDir: "{app}";            Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Svg.dll";        DestDir: "{app}";            Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6OpenGL.dll";     DestDir: "{app}";            Flags: ignoreversion skipifsourcedoesntexist

; ── Плагины Qt ─────────────────────────────────────────────
Source: "{#AppDir}\platforms\*";       DestDir: "{app}\platforms";  Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\styles\*";          DestDir: "{app}\styles";     Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\imageformats\*";    DestDir: "{app}\imageformats";Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; ── Visual C++ Runtime ─────────────────────────────────────
Source: "redist\vc_redist.x64.exe";    DestDir: "{tmp}";            Flags: deleteafterinstall skipifsourcedoesntexist

; ── Примеры DXF файлов ─────────────────────────────────────
Source: "..\examples\*";               DestDir: "{app}\examples";   Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; ── README ─────────────────────────────────────────────────
Source: "..\src\README.md";            DestDir: "{app}";            Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";                   Filename: "{app}\{#AppExeName}"
Name: "{group}\Удалить {#AppName}";           Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";             Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Установка Visual C++ Runtime (тихо, без перезагрузки)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; \
    StatusMsg: "Установка Visual C++ Runtime..."; \
    Flags: waituntilterminated skipifdoesntexist

; Запуск программы после установки
Filename: "{app}\{#AppExeName}"; \
    Description: "Запустить NestingApp"; \
    Flags: nowait postinstall skipifsilent

[Registry]
; Ассоциация файлов .nest с программой
Root: HKA; Subkey: "Software\Classes\.nest"; ValueType: string; ValueData: "NestingApp.Project"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\NestingApp.Project"; ValueType: string; ValueData: "Проект NestingApp"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\NestingApp.Project\Shell\Open\Command"; ValueType: string; ValueData: """{app}\{#AppExeName}"" ""%1"""; Flags: uninsdeletevalue

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
// Проверка наличия Visual C++ Redist перед установкой
function VCRedistInstalled(): Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(HKLM,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
    'Version', Version);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then begin
    if not VCRedistInstalled() then
      MsgBox('Visual C++ Runtime будет установлен автоматически.', mbInformation, MB_OK);
  end;
end;
