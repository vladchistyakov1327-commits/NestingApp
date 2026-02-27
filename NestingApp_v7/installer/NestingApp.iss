; Inno Setup script for NestingApp
; Полностью исправленная версия

#define AppName "NestingApp"
#define AppVersion "2.0"
#define AppPublisher "MetalShop"
#define AppExeName "NestingApp.exe"

[Setup]
; Основная информация о приложении
AppId={{A7F3C2D1-4E5B-4F6A-8C9D-0E1F2A3B4C5D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes

; Выходной файл - создается в папке installer\dist\
OutputDir=dist
OutputBaseFilename=NestingApp_v2_Setup

; Настройки сжатия
Compression=lzma2/ultra64
SolidCompression=yes

; Архитектура
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
PrivilegesRequired=admin

; Иконка для удаления
UninstallDisplayIcon={app}\{#AppExeName}

; Современный стиль
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Основной исполняемый файл (путь передается через /DAppDir=...)
Source: "{#AppDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt6 основные библиотеки
Source: "{#AppDir}\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Svg.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6OpenGL.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Network.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#AppDir}\Qt6Concurrent.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Qt плагины
Source: "{#AppDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#AppDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; Visual C++ Redistributable (путь передается через /DRedistDir=...)
Source: "{#RedistDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

; README файл
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Примеры DXF файлов (если есть)
Source: "..\examples\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Установка Visual C++ Redistributable
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Установка Visual C++ Redistributable..."; Flags: waituntilterminated skipifdoesntexist

; Запуск программы после установки
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Registry]
; Ассоциация файлов .dxf
Root: HKCU; Subkey: "Software\Classes\.dxf\OpenWithProgIds"; ValueType: string; ValueName: "NestingApp.dxf"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\NestingApp.dxf\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExeName}"" ""%1"""; Flags: uninsdeletevalue

; Ассоциация файлов .nest
Root: HKCU; Subkey: "Software\Classes\.nest"; ValueType: string; ValueName: ""; ValueData: "NestingApp.Project"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\NestingApp.Project\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExeName}"" ""%1"""; Flags: uninsdeletevalue

[UninstallDelete]
Type: filesandordirs; Name: "{app}\examples"
Type: filesandordirs; Name: "{app}\exports"

[Code]
// Проверка нужно ли устанавливать VC++ Redist
function VCRedistNeedsInstall: Boolean;
var
  Version: String;
begin
  if RegQueryStringValue(HKLM,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64',
    'Version', Version) then
  begin
    Result := (CompareStr(Version, 'v14.30.00000') < 0);
  end else
    Result := True;
end;

// Создание папок при установке
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    ForceDirectories(ExpandConstant('{userappdata}\NestingApp'));
    ForceDirectories(ExpandConstant('{userappdata}\NestingApp\templates'));
    ForceDirectories(ExpandConstant('{userappdata}\NestingApp\exports'));
  end;
end;
