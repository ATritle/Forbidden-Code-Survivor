"""Collect real playback/installer receipts and verify preserved Windows releases."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
out = root/'Docs/Windows-0.4.0-Validation'
out.mkdir(parents=True, exist_ok=True)

def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4*1024*1024), b''):
            value.update(chunk)
    return value.hexdigest()

reports = {
 'MusicPlayer': ('Saved/QA/RidersMusicShipping/Saved/Review/MusicPlayer/report.json','MUSIC_PLAYER_PASS'),
 'MusicPlayer-Restart': ('Saved/QA/RidersMusicShipping/Saved/Review/MusicPlayer/reload.json','MUSIC_PLAYER_RELOAD_PASS'),
 'Guide': ('Saved/QA/RidersGuideShipping/Saved/Review/RampGuide/report.json','RAMP_GUIDE_PASS'),
 'MenuAudio': ('Saved/QA/RidersAudioShipping/Saved/Review/MenuAudio/report.json','MENU_AUDIO_PASS'),
 'Settings-Wide': ('Saved/QA/RidersSettingsShipping/Wide/Saved/Review/SettingsReview/report.json','SETTINGS_REVIEW_PASS'),
 'Settings-Compact': ('Saved/QA/RidersSettingsShipping/Compact/Saved/Review/SettingsReview/report.json','SETTINGS_REVIEW_PASS'),
 'Package': ('Saved/Review/RidersPackagedSmoke/report.json','PACKAGED_RELEASE_PASS'),
 'Installer': ('Saved/Review/InstallerRoundtrip-0.4.0.json','INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes': ('Saved/Review/InstallerExtraction-0.4.0.json','INSTALLER_EXTRACTION_HASH_PASS')}
for rider in ['NeonExorcist','Vex','Rook','Zero','Mira']:
 for filename in ['report','reload']:
  reports[rider+'-'+filename]=('Saved/QA/RiderSelectionShipping/'+rider+'/Saved/Review/RiderSelection/'+filename+'.json','RIDER_SELECTION_PASS')
for rider in ['Zero','Mira']:
 reports[rider+'-Compact']=('Saved/QA/RiderSelectionCompact/'+rider+'/Saved/Review/RiderSelection/report.json','RIDER_SELECTION_PASS')
for name,(relative,expected) in reports.items():
 path=root/relative;report=json.loads(path.read_text(encoding='utf-8-sig'))
 assert report['result']==expected,(name,report)
 shutil.copy2(path,out/(name+'.json'))
for rider in ['NeonExorcist','Vex','Rook','Zero','Mira']:
 capture=root/'Saved/QA/RiderSelectionShipping'/rider/'Saved/Review/RiderSelection'
 for name in [rider+'-View-0.png',rider+'-Attack.png',rider+'-Gameplay.png']:
  assert (capture/name).stat().st_size>10000
  shutil.copy2(capture/name,out/name)
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
    ('0.3.9', 'Docs/Windows-0.3.9-Validation/release-manifest.json')]:
    previous = json.loads((root/receipt).read_text())
    assert sha(root/previous['installer']) == previous['installerSHA256']
    for entry in previous['packageFiles']:
        assert sha(root/('Artifacts/Windows-'+version)/entry['path']) == entry['sha256'], entry['path']
    preserved.append(version)
baseline = root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z/ForbiddenCode-Windows-Baseline.zip'
assert sha(baseline) == 'f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'

installer = root/'Artifacts/Installer-0.4.0/ForbiddenCode-Survivor-0.4.0-Setup.exe'
package = root/'Artifacts/Windows-0.4.0'
if not (package/'ForbiddenCodeSurvivor.exe').exists():
    package = package/'Windows'
manifest = {
    'result': 'RIDER_ROSTER_RELEASE_PASS', 'version': '0.4.0',
    'createdUTC': datetime.now(timezone.utc).isoformat(),
    'configuration': 'Shipping', 'engine': 'UE 5.8.2',
    'installer': installer.relative_to(root).as_posix(),
    'installerBytes': installer.stat().st_size, 'installerSHA256': sha(installer),
    'previousPackagesVerified': preserved, 'originalBaselineVerified': True,
    'meshyCredits':248, 'paidRetries':0, 'reports': list(reports), 'musicSources': [], 'packageFiles': []}
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
print('RIDER_ROSTER_RELEASE_PASS', manifest['installerSHA256'], 'previous releases and original baseline unchanged')
