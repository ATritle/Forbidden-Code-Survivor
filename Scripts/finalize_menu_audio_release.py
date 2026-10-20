"""Verify menu-audio release receipts and preserved versions before saving the manifest."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, shutil
root=Path(__file__).resolve().parents[1]
out=root/'Docs/Windows-0.3.2-Validation';out.mkdir(parents=True,exist_ok=True)
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(4*1024*1024),b''):h.update(chunk)
    return h.hexdigest()
reports={
 'MenuAudio-Shipping':('Saved/QA/MenuAudioShipping/Saved/Review/MenuAudio/report.json','MENU_AUDIO_PASS'),
 'Quit-MainMenu':('Saved/QA/MenuAudioShipping/Saved/Review/RampGuide/QuitMenuSmoke-report.json','CLOSE_GAME_PASS'),
 'Quit-Settings':('Saved/QA/MenuAudioShipping/Saved/Review/RampGuide/QuitSettingsSmoke-report.json','CLOSE_GAME_PASS'),
 'Package':('Saved/Review/MenuAudioPackagedSmoke/report.json','PACKAGED_RELEASE_PASS'),
 'Installer':('Saved/Review/InstallerRoundtrip-0.3.2.json','INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes':('Saved/Review/InstallerExtraction-0.3.2.json','INSTALLER_EXTRACTION_HASH_PASS'),
}
for name,(relative,result) in reports.items():
    path=root/relative;report=json.loads(path.read_text(encoding='utf-8-sig'));assert report['result']==result,(name,report)
    if name=='MenuAudio-Shipping':assert report['capturePeak']>.003 and report['effectVoices']==18
    shutil.copy2(path,out/(name+'.json'))
capture=root/'Saved/QA/MenuAudioShipping/Saved/Review/MenuAudio'
for name in ['MainMenu.png','MenuFeedback.wav']:
    path=capture/name;assert path.stat().st_size>10000;shutil.copy2(path,out/name)
preserved=[]
for version,receipt in [('0.3.0','Docs/WindowsOverhaul/release-manifest.json'),('0.3.1','Docs/Windows-0.3.1-Validation/release-manifest.json')]:
    previous=json.loads((root/receipt).read_text());assert sha(root/previous['installer'])==previous['installerSHA256']
    for entry in previous['packageFiles']:assert sha(root/('Artifacts/Windows-'+version)/entry['path'])==entry['sha256'],entry['path']
    preserved.append(version)
baseline=root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z/ForbiddenCode-Windows-Baseline.zip'
assert sha(baseline)=='f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'
installer=root/'Artifacts/Installer-0.3.2/ForbiddenCode-Survivor-0.3.2-Setup.exe'
package=root/'Artifacts/Windows-0.3.2'
if not (package/'ForbiddenCodeSurvivor.exe').exists():package=package/'Windows'
manifest={'result':'MENU_AUDIO_RELEASE_PASS','version':'0.3.2','createdUTC':datetime.now(timezone.utc).isoformat(),'configuration':'Shipping','engine':'UE 5.8.2',
 'installer':str(installer.relative_to(root)).replace('\\','/'),'installerBytes':installer.stat().st_size,'installerSHA256':sha(installer),'previousPackagesVerified':preserved,'originalBaselineVerified':True,'reports':list(reports),'soundSources':[],'packageFiles':[]}
for file in ['rescopicsound-ui-click-menu-modern-interface-select-medium-230477.mp3','rescopicsound-ui-click-menu-modern-interface-select-small-01-230473.mp3']:
    path=root/'SFX/menu'/file;manifest['soundSources'].append({'path':str(path.relative_to(root)).replace('\\','/'),'sha256':sha(path)})
for path in sorted(package.rglob('*')):
    if not path.is_file() or path.suffix=='.pdb' or path.name.startswith('Manifest_DebugFiles'):continue
    manifest['packageFiles'].append({'path':str(path.relative_to(package)).replace('\\','/'),'bytes':path.stat().st_size,'sha256':sha(path)})
(out/'release-manifest.json').write_text(json.dumps(manifest,indent=2));(installer.parent/'release-manifest.json').write_text(json.dumps(manifest,indent=2))
print('MENU_AUDIO_RELEASE_PASS',manifest['installerSHA256'],'prior packages and original baseline unchanged')
