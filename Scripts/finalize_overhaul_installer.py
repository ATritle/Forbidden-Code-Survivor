from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'Packaging/ForbiddenCodeSurvivor.nsi'
s=p.read_text(encoding='utf-8-sig')
s=s.replace('"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ForbiddenCodeSurvivor"','"${REG_KEY}"')
s=s.replace('"DisplayVersion" "0.2.0"','"DisplayVersion" "${BUILD_VERSION}"')
s=s.replace('"DisplayName" "Forbidden Code: Survivor"','"DisplayName" "Forbidden Code: Survivor ${BUILD_VERSION}"')
s=s.replace('$SMPROGRAMS\\Forbidden Code Survivor','$SMPROGRAMS\\Forbidden Code Survivor ${BUILD_VERSION}')
s=s.replace('$DESKTOP\\Forbidden Code Survivor.lnk','$DESKTOP\\Forbidden Code Survivor ${BUILD_VERSION}.lnk')
s=s.replace('"..\\Artifacts\\Installer\\uninstall-files.nsh"','"${UNINSTALL_INCLUDE}"')
p.write_bytes(s.encode('utf-8'))
