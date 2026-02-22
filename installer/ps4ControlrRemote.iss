[Setup]
AppName=PS4 Controlr Remote
AppVersion=2.0.0
DefaultDirName={autopf}\PS4ControlrRemote
OutputDir=installer\out
OutputBaseFilename=PS4ControlrRemote_Setup
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
DisableProgramGroupPage=yes

[Files]
Source: "..\dist\ps4ControlrRemote.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\SDL3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\ViGEmClient.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\PS4 Controlr Remote"; Filename: "{app}\ps4ControlrRemote.exe"
Name: "{commondesktop}\PS4 Controlr Remote"; Filename: "{app}\ps4ControlrRemote.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na área de trabalho"; Flags: unchecked

[Run]
Filename: "{app}\ps4ControlrRemote.exe"; Description: "Executar PS4 Controlr Remote agora"; Flags: nowait postinstall skipifsilent

[Code]
function ViGEmBusInstalled(): Boolean;
begin
  Result := RegKeyExists(HKLM, 'SYSTEM\CurrentControlSet\Services\ViGEmBus');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if not ViGEmBusInstalled() then
    begin
      MsgBox('ATENÇÃO: ViGEmBus não foi detectado. ' +
             'Sem ele, o controle virtual Xbox 360 não aparecerá nos jogos. ' +
             'Instale o driver ViGEmBus antes de usar o aplicativo.',
             mbInformation, MB_OK);
    end;
  end;
end;
