Unicode true
!ifndef PACKAGE_ROOT
!define PACKAGE_ROOT "..\Artifacts\Windows"
!endif
!include "MUI2.nsh"
Name "Forbidden Code: Survivor"
OutFile "..\Artifacts\Installer\ForbiddenCode-Survivor-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\ForbiddenCodeSurvivor"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\ForbiddenCodeSurvivor.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Section "Forbidden Code: Survivor (required)" Game
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r /x "*.pdb" /x "Manifest_DebugFiles*.txt" "${PACKAGE_ROOT}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "DisplayName" "Forbidden Code: Survivor"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "DisplayVersion" "0.2.0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\Forbidden Code Survivor"
  CreateShortcut "$SMPROGRAMS\Forbidden Code Survivor\Forbidden Code Survivor.lnk" "$INSTDIR\ForbiddenCodeSurvivor.exe"
SectionEnd
Section /o "Desktop shortcut" Desktop
  CreateShortcut "$DESKTOP\Forbidden Code Survivor.lnk" "$INSTDIR\ForbiddenCodeSurvivor.exe"
SectionEnd
Section "Uninstall"
  ; Delete only files enumerated from this build, preserving unrelated files and saved games.
  !include "..\Artifacts\Installer\uninstall-files.nsh"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Forbidden Code Survivor\Forbidden Code Survivor.lnk"
  RMDir "$SMPROGRAMS\Forbidden Code Survivor"
  Delete "$DESKTOP\Forbidden Code Survivor.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor"
SectionEnd
