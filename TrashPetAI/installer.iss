#define MyAppName "TrashPet AI"
#define MyAppVersion "1.0"
#define MyAppExeName "TrashPetAI.exe"

[Setup]
AppId={{B6F1E2B0-6C3A-4E9A-9F0E-1A2B3C4D5E6F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\TrashPetAI
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=installer_output
OutputBaseFilename=TrashPetAI_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=imgs\logo.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Tạo shortcut ngoài Desktop"; GroupDescription: "Shortcut bổ sung:"

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Gỡ cài đặt {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Chạy {#MyAppName} ngay"; Flags: nowait postinstall skipifsilent
