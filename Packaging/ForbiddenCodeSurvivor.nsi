Unicode true
!ifndef PACKAGE_ROOT
!define PACKAGE_ROOT "..\Artifacts\Windows"
!endif
!ifndef BUILD_VERSION
!define BUILD_VERSION "0.4.3"
!endif
!ifndef OUTPUT_DIRECTORY
!define OUTPUT_DIRECTORY "..\Artifacts\Installer-${BUILD_VERSION}"
!endif
!ifndef UNINSTALL_INCLUDE
!define UNINSTALL_INCLUDE "${OUTPUT_DIRECTORY}\uninstall-files.nsh"
!endif
!define REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor-${BUILD_VERSION}"
!include "MUI2.nsh"
Name "Forbidden Code: Survivor"
OutFile "${OUTPUT_DIRECTORY}\ForbiddenCode-Survivor-${BUILD_VERSION}-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\ForbiddenCodeSurvivor-${BUILD_VERSION}"
InstallDirRegKey HKCU "${REG_KEY}" "InstallLocation"
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
  WriteRegStr HKCU "${REG_KEY}" "DisplayName" "Forbidden Code: Survivor ${BUILD_VERSION}"
  WriteRegStr HKCU "${REG_KEY}" "DisplayVersion" "${BUILD_VERSION}"
  WriteRegStr HKCU "${REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REG_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "${REG_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REG_KEY}" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\Forbidden Code Survivor ${BUILD_VERSION}"
  CreateShortcut "$SMPROGRAMS\Forbidden Code Survivor ${BUILD_VERSION}\Forbidden Code Survivor.lnk" "$INSTDIR\ForbiddenCodeSurvivor.exe"
SectionEnd
Section /o "Desktop shortcut" Desktop
  CreateShortcut "$DESKTOP\Forbidden Code Survivor ${BUILD_VERSION}.lnk" "$INSTDIR\ForbiddenCodeSurvivor.exe"
SectionEnd
Section "Uninstall"
  ; Delete only files enumerated from this build, preserving unrelated files and saved games.
  !include "${UNINSTALL_INCLUDE}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Forbidden Code Survivor ${BUILD_VERSION}\Forbidden Code Survivor.lnk"
  RMDir "$SMPROGRAMS\Forbidden Code Survivor ${BUILD_VERSION}"
  Delete "$DESKTOP\Forbidden Code Survivor ${BUILD_VERSION}.lnk"
  DeleteRegKey HKCU "${REG_KEY}"
SectionEnd
