#ifndef AppVersion
#define AppVersion "0.1.1"
#endif
[Setup]
AppId={{EA5F4898-A19D-4BCA-B345-7AF6EF10EF09}
AppName=Zernia media
AppVersion={#AppVersion}
AppPublisher=Zernia
AppVerName=Zernia media (ZM) {#AppVersion}
DefaultDirName={localappdata}\Programs\Zernia media
DefaultGroupName=Zernia media
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=ZM-{#AppVersion}-setup-win64
SetupIconFile=..\resources\zm.ico
UninstallDisplayIcon={app}\ZM.exe
CreateUninstallRegKey=not IsTestInstall
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Zernia media installer

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Check: IsNormalInstall

[Files]
Source: "..\dist\ZM-{#AppVersion}-win64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\Zernia media"; Filename: "{app}\ZM.exe"; WorkingDir: "{app}"; Check: IsNormalInstall
Name: "{userdesktop}\Zernia media"; Filename: "{app}\ZM.exe"; WorkingDir: "{app}"; Tasks: desktopicon; Check: IsNormalInstall

[Run]
Filename: "{app}\ZM.exe"; Description: "{cm:LaunchProgram,Zernia media}"; Flags: nowait postinstall skipifsilent; Check: IsNormalInstall

[UninstallRun]
Filename: "{app}\ZM.exe"; Parameters: "--unregister"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveZMAssociations"

[Code]
function IsTestInstall: Boolean;
begin
  Result := ExpandConstant('{param:TESTMODE|0}') = '1';
end;

function IsNormalInstall: Boolean;
begin
  Result := not IsTestInstall;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ExitCode: Integer;
begin
  if (CurStep = ssPostInstall) and IsNormalInstall then
  begin
    if not Exec(ExpandConstant('{app}\ZM.exe'), '--register', '', SW_HIDE,
      ewWaitUntilTerminated, ExitCode) then
      MsgBox('Could not register ZM in Windows Default Apps.', mbError, MB_OK)
    else if ExitCode <> 0 then
      MsgBox('Could not register ZM in Windows Default Apps.', mbError, MB_OK);
  end;
end;
