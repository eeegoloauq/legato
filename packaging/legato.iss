; The Windows installer, built by build-windows.sh from the bundle it has just made. Per user and
; without administrator rights, into %LOCALAPPDATA%\Programs\Legato.
;
; Uninstalling leaves %LOCALAPPDATA%\legato alone: that is the settings and the cover cache, and a
; reinstall should find them. Passwords are in the Windows Credential Manager.

#ifndef Version
  #error Run as: iscc /DVersion=X.Y.Z[-beta.N] /DFileVersion=X.Y.Z legato.iss
#endif

[Setup]
; Fixed for good: it is how Windows knows a new version is this app.
AppId={{38AC8DF5-2728-41B8-B085-AADD1B704587}
AppName=Legato
AppVersion={#Version}
; Without it the setup file's own version resource reads 0.0.0.0. Numbers only, so a beta gives
; its release's.
VersionInfoVersion={#FileVersion}
AppPublisher=Legato
AppPublisherURL=https://github.com/eeegoloauq/legato
DefaultDirName={autopf}\Legato
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=legato-{#Version}-windows-x86_64-setup
SetupIconFile=legato.ico
UninstallDisplayIcon={app}\bin\legato.exe
LicenseFile=..\LICENSE
WizardStyle=modern
SolidCompression=yes

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "..\dist\bundle\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Legato"; Filename: "{app}\bin\legato.exe"
Name: "{autodesktop}\Legato"; Filename: "{app}\bin\legato.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\legato.exe"; Description: "{cm:LaunchProgram,Legato}"; Flags: nowait postinstall skipifsilent
