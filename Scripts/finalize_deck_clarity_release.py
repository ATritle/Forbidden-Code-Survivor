"""Collect real playback/installer receipts and verify preserved Windows releases."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
out = root/'Docs/Windows-0.4.2-Validation'
out.mkdir(parents=True, exist_ok=True)

def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4*1024*1024), b''):
            value.update(chunk)
    return value.hexdigest()

reports = {
 'Guide': ('Saved/QA/DeckClarityGuideShipping/Saved/Review/RampGuide/report.json','RAMP_GUIDE_PASS'),
 'Quit-MainMenu': ('Saved/QA/DeckClarityGuideShipping/Saved/Review/RampGuide/QuitMenuSmoke-report.json','CLOSE_GAME_PASS'),
 'Quit-Settings': ('Saved/QA/DeckClarityGuideShipping/Saved/Review/RampGuide/QuitSettingsSmoke-report.json','CLOSE_GAME_PASS'),
 'MenuAudio': ('Saved/QA/DeckClarityAudioShipping/Saved/Review/MenuAudio/report.json','MENU_AUDIO_PASS'),
 'Package': ('Saved/Review/DeckClarityPackagedSmoke/report.json','PACKAGED_RELEASE_PASS'),
 'Installer': ('Saved/Review/InstallerRoundtrip-0.4.2.json','INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes': ('Saved/Review/InstallerExtraction-0.4.2.json','INSTALLER_EXTRACTION_HASH_PASS')}
for size in ['Wide','Compact']:
 for file in ['report','reload']:
  reports['Workbench-'+size+'-'+file]=('Saved/QA/DeckClarityShipping/'+size+'/Saved/Review/Workbench/'+file+'.json','WORKBENCH_PASS')
 reports['Hover-'+size]=('Saved/Review/DeckClarityHover-'+size+'.json','WORKBENCH_HOVER_PASS')
for name,(relative,expected) in reports.items():
 path=root/relative;report=json.loads(path.read_text(encoding='utf-8-sig'))
 assert report['result']==expected,(name,report)
 shutil.copy2(path,out/(name+'.json'))
for size in ['Wide','Compact']:
 for name in ['Workbench-Deck.png','Workbench-Library.png','Workbench-Saved.png','Workbench-Card-Hover.png','Workbench-Rider-Hover.png','Workbench-Replace-Card.png']:
  capture=root/'Saved/QA/DeckClarityShipping'/size/'Saved/Review/Workbench'/name
  assert capture.stat().st_size>10000
  shutil.copy2(capture,out/(size+'-'+name))
ledger=json.loads((root/'ContentSource/Meshy/AlternateRiders/submitted-jobs.json').read_text())
assert ledger['spentCredits']==248 and ledger['paidRetries']==0

preserved = []
for version, receipt in [
    ('0.3.0', 'Docs/WindowsOverhaul/release-manifest.json'),
    ('0.3.1', 'Docs/Windows-0.3.1-Validation/release-manifest.json'),
    ('0.3.2', 'Docs/Windows-0.3.2-Validation/release-manifest.json'),
    ('0.3.3', 'Docs/Windows-0.3.3-Validation/release-manifest.json'),
    ('0.3.4', 'Docs/Windows-0.3.4-Validation/release-manifest.json'),
    ('0.3.5', 'Docs/Windows-0.3.5-Validation/release-manifest.json'),
    ('0.3.6', 'Docs/Windows-0.3.6-Validation/release-manifest.json'),
    ('0.3.7', 'Docs/Windows-0.3.7-Validation/release-manifest.json'),
    ('0.3.8', 'Docs/Windows-0.3.8-Validation/release-manifest.json'),
    ('0.3.9', 'Docs/Windows-0.3.9-Validation/release-manifest.json'),
    ('0.4.0', 'Docs/Windows-0.4.0-Validation/release-manifest.json'),
    ('0.4.1', 'Docs/Windows-0.4.1-Validation/release-manifest.json')]:
    previous = json.loads((root/receipt).read_text())
    assert sha(root/previous['installer']) == previous['installerSHA256']
    for entry in previous['packageFiles']:
        assert sha(root/('Artifacts/Windows-'+version)/entry['path']) == entry['sha256'], entry['path']
    preserved.append(version)
baseline = root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z/ForbiddenCode-Windows-Baseline.zip'
assert sha(baseline) == 'f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'

installer = root/'Artifacts/Installer-0.4.2/ForbiddenCode-Survivor-0.4.2-Setup.exe'
package = root/'Artifacts/Windows-0.4.2'
if not (package/'ForbiddenCodeSurvivor.exe').exists():
    package = package/'Windows'
manifest = {
    'result': 'DECK_CLARITY_RELEASE_PASS', 'version': '0.4.2',
    'createdUTC': datetime.now(timezone.utc).isoformat(),
    'configuration': 'Shipping', 'engine': 'UE 5.8.2',
    'installer': installer.relative_to(root).as_posix(),
    'installerBytes': installer.stat().st_size, 'installerSHA256': sha(installer),
    'previousPackagesVerified': preserved, 'originalBaselineVerified': True,
    'meshyCredits':0, 'imagegenPortraits':0, 'paidRetries':0, 'reports': list(reports), 'musicSources': [], 'packageFiles': []}
from PIL import Image
manifest['layoutReference']='ContentSource/Concepts/CombinedLoadoutMenu/A-Loadout-Workbench-v2.png'
manifest['layoutReferenceSHA256']=sha(root/manifest['layoutReference'])
manifest['riderArtwork']=[]
for rider in ['NeonExorcist','Vex','Rook','Zero','Mira']:
 source=root/'ContentSource/UI/LoadoutWorkbench'/('Rider-'+rider+'-v1.png')
 image=Image.open(source)
 assert image.mode=='RGBA' and image.size==(1024,1536) and image.getextrema()[3][0]==0
 manifest['riderArtwork'].append(dict(rider=rider,source=source.relative_to(root).as_posix(),sha256=sha(source),size=list(image.size),generation='reused built-in imagegen artwork from 0.4.1',menuAnimation='fixed pose'))
for entry in json.loads((root/'ContentSource/Audio/manifest.json').read_text()):
    if not entry['asset'].startswith('Music_'):
        continue
    source = root/entry['source']
    manifest['musicSources'].append(dict(asset=entry['asset'], source=source.relative_to(root).as_posix(),
                                        seconds=entry['seconds'], sha256=sha(source)))
for path in sorted(package.rglob('*')):
    if not path.is_file() or path.suffix == '.pdb' or path.name.startswith('Manifest_DebugFiles'):
        continue
    manifest['packageFiles'].append(dict(path=path.relative_to(package).as_posix(),
                                         bytes=path.stat().st_size, sha256=sha(path)))
(out/'release-manifest.json').write_text(json.dumps(manifest, indent=2))
(installer.parent/'release-manifest.json').write_text(json.dumps(manifest, indent=2))
print('DECK_CLARITY_RELEASE_PASS', manifest['installerSHA256'], 'previous releases and original baseline unchanged')
