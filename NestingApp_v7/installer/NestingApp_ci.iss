; ============================================================
;  NestingApp_ci.iss  —  Скрипт Inno Setup для GitHub Actions CI
;
;  Запуск из workflow:
;    iscc /DAppDir=<путь к build\Release> ^
;         /DOutputDir=<папка dist> ^
;         /DRedistDir=<папка с vc_redist.x64.exe> ^
;         NestingApp_ci.iss
; ============================================================

#ifndef AppDir
  #define AppDir "..\..\..\build\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\..\dist"
#endif
#ifndef RedistDir
  #define RedistDir "redist"
#endif

#define AppName      "NestingApp"
#define AppVersion   "2.0"
#define AppPublisher "MetalShop"
#define AppExeName   "NestingApp.exe"

[Setup]
AppId={{A7F3C2D1-4E5B-4F6A-8C9D-0E1F2A3B4C5D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir={#OutputDir}
OutputBaseFilename=NestingApp_v2_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64os
ArchitecturesAllowed=x64os
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}
WizardStyle=modern
; Без иконки — для CI (нет assets/icon.ico)
DisableWelcomePage=no
DisableDirPage=no

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
  GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; ── Основной .exe ──────────────────────────────────────────
Source: "{#AppDir}\{#AppExeName}";   DestDir: "{app}"; Flags: ignoreversion

; ── Qt6 DLL (windeployqt скопировал их рядом с .exe) ──────
Source: "{#AppDir}\Qt6Core.dll";     DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Gui.dll";      DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Widgets.dll";  DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Svg.dll";      DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6OpenGL.dll";   DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6OpenGLWidgets.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; ── Плагины Qt (папки platforms, styles, imageformats) ────
Source: "{#AppDir}\platforms\*";     DestDir: "{app}\platforms";    Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\styles\*";        DestDir: "{app}\styles";       Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\imageformats\*";  DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; ── Visual C++ Runtime (тихая установка) ──────────────────
Source: "{#RedistDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; \
  Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}";                Filename: "{app}\{#AppExeName}"
Name: "{group}\Удалить {#AppName}";        Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";          Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Устанавливаем VC++ Runtime (тихо, без перезагрузки)
Filename: "{tmp}\vc_redist.x64.exe"; \
  Parameters: "/quiet /norestart"; \
  StatusMsg: "Установка Visual C++ Runtime..."; \
  Flags: waituntilterminated skipifdoesntexist

; Запуск программы после установки
Filename: "{app}\{#AppExeName}"; \
  Description: "Запустить {#AppName}"; \
  Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
