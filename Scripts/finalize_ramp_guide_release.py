"""Collect only passing native/package receipts and verify preserved builds before release."""
from pathlib import Path
import hashlib, json, shutil
from datetime import datetime, timezone
root=Path(__file__).resolve().parents[1]
out=root/'Docs/Windows-0.3.1-Validation';out.mkdir(parents=True,exist_ok=True)
def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(4*1024*1024),b''):digest.update(chunk)
    return digest.hexdigest()
reports={
 'Guide-Shipping':('Saved/QA/RampGuideShipping/Saved/Review/RampGuide/report.json','RAMP_GUIDE_PASS'),
 'Quit-MainMenu':('Saved/QA/RampGuideShipping/Saved/Review/RampGuide/QuitMenuSmoke-report.json','CLOSE_GAME_PASS'),
 'Quit-Settings':('Saved/QA/RampGuideShipping/Saved/Review/RampGuide/QuitSettingsSmoke-report.json','CLOSE_GAME_PASS'),
 'Package':('Saved/Review/RampGuidePackagedSmoke/report.json','PACKAGED_RELEASE_PASS'),
 'Installer':('Saved/Review/InstallerRoundtrip-0.3.1.json','INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes':('Saved/Review/InstallerExtraction-0.3.1.json','INSTALLER_EXTRACTION_HASH_PASS'),
 'Source-Ramp':('Saved/Review/RampSource.json','RAMP_SOURCE_PASS'),
 'Walkable-Material':('Saved/Review/WalkableMaterial.json','WALKABLE_SURFACES_PASS'),
}
for name,(relative,result) in reports.items():
    path=root/relative; report=json.loads(path.read_text(encoding='utf-8-sig')); assert report['result']==result,(name,report)
    shutil.copy2(path,out/(name+'.json'))
captures=root/'Saved/QA/RampGuideShipping/Saved/Review/RampGuide'
for name in ['Menu','Guide-Overview','Guide-Program','Guide-Enemy','Guide-District','Ramp','Ramp-2','Ramp-3','Ramp-4','Ramp-5','Ramp-6','Ramp-7','Ramp-8']:
    path=captures/(name+'.png');assert path.stat().st_size>10000;shutil.copy2(path,out/path.name)
old=json.loads((root/'Docs/WindowsOverhaul/release-manifest.json').read_text())
assert sha(root/old['installer'])==old['installerSHA256'],'Preserved 0.3.0 installer changed'
for item in old['packageFiles']:
    assert sha(root/'Artifacts/Windows-0.3.0'/item['path'])==item['sha256'],'Preserved 0.3.0 file changed: '+item['path']
baseline=root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z'
expected='f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'
assert sha(baseline/'ForbiddenCode-Windows-Baseline.zip')==expected,'Original baseline ZIP changed'
assert sha(baseline/'manifest.json')=='0c4d5ff77f0504a84c22322ee8a63593bdfd1c7eb7fd80e19c93ab9989bbce94','Original manifest changed'
installer=root/'Artifacts/Installer-0.3.1/ForbiddenCode-Survivor-0.3.1-Setup.exe'
package=root/'Artifacts/Windows-0.3.1'
if not (package/'ForbiddenCodeSurvivor.exe').exists():package=package/'Windows'
manifest={'version':'0.3.1','createdUTC':datetime.now(timezone.utc).isoformat(),'result':'RAMP_GUIDE_RELEASE_PASS','engine':'UE 5.8.2','configuration':'Shipping',
 'installer':str(installer.relative_to(root)).replace('\\','/'),'installerBytes':installer.stat().st_size,'installerSHA256':sha(installer),
 'preservedPreviousPackageFiles':len(old['packageFiles']),'preservedOriginalZIP':expected,'guideEntries':52,'verifiedRamps':8,'reports':list(reports),'packageFiles':[]}
for path in sorted(package.rglob('*')):
    if not path.is_file() or path.suffix=='.pdb' or path.name.startswith('Manifest_DebugFiles'):continue
    manifest['packageFiles'].append({'path':str(path.relative_to(package)).replace('\\','/'),'bytes':path.stat().st_size,'sha256':sha(path)})
(out/'release-manifest.json').write_text(json.dumps(manifest,indent=2))
(installer.parent/'release-manifest.json').write_text(json.dumps(manifest,indent=2))
print('RAMP_GUIDE_RELEASE_PASS',manifest['installerSHA256'],len(manifest['packageFiles']),'package files; previous two builds verified unchanged')
