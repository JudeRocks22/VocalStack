; Inno Setup script for the VocalStack VST3 (Windows).
; Build the plugin first (see README.md), then compile this script with Inno Setup 6:
;   iscc installer\VocalStack.iss
; Output: installer\Output\VocalStack-Setup.exe

#define AppName "VocalStack"
#define AppVersion "0.1.0"
; Path to the built VST3 bundle, relative to this script
#define Vst3Source "..\build\VocalStack_artefacts\Release\VST3\VocalStack.vst3"

[Setup]
AppId={{B2D1B6F3-6C2A-4F0B-9E61-5A5E7A3C9D41}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=VocalStack
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
OutputDir=Output
OutputBaseFilename=VocalStack-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Files]
; The .vst3 is a folder bundle on Windows; copy it recursively.
Source: "{#Vst3Source}\*"; DestDir: "{commoncf64}\VST3\VocalStack.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
FinishedLabel=VocalStack was installed to the system VST3 folder. Rescan plugins in your DAW to find it.
