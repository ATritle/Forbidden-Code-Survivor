"""Collect real playback/installer receipts and verify preserved Windows releases."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
out = root/'Docs/Windows-0.3.7-Validation'
out.mkdir(parents=True, exist_ok=True)

def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4*1024*1024), b''):
            value.update(chunk)
    return value.hexdigest()

reports = {'Settings-Wide': ('Saved/QA/SettingsReviewShipping/Wide/Saved/Review/SettingsReview/report.json',
                   'SETTINGS_REVIEW_PASS'),
 'Settings-Compact': ('Saved/QA/SettingsReviewShipping/Compact/Saved/Review/SettingsReview/report.json',
                      'SETTINGS_REVIEW_PASS'),
 'Settings-Restart': ('Saved/QA/SettingsReviewShipping/Wide/Saved/Review/SettingsReview/preferences-reload.json',
                      'SETTINGS_REVIEW_PASS'),
 'Display-Native': ('Saved/QA/SettingsReviewShipping/Display/Saved/Review/SettingsReview/report.json',
                    'SETTINGS_REVIEW_PASS'),
 'Display-Restart': ('Saved/QA/SettingsReviewShipping/Display/Saved/Review/SettingsReview/display-reload.json',
                     'SETTINGS_REVIEW_PASS'),
 'MenuAudio': ('Saved/QA/SettingsMenuAudioShipping/Saved/Review/MenuAudio/report.json', 'MENU_AUDIO_PASS'),
 'Quit-MainMenu': ('Saved/QA/SettingsMenuAudioShipping/Saved/Review/RampGuide/QuitMenuSmoke-report.json',
                   'CLOSE_GAME_PASS'),
 'Quit-Settings': ('Saved/QA/SettingsMenuAudioShipping/Saved/Review/RampGuide/QuitSettingsSmoke-report.json',
                   'CLOSE_GAME_PASS'),
 'Package': ('Saved/Review/SettingsPackagedSmoke/report.json', 'PACKAGED_RELEASE_PASS'),
 'Installer': ('Saved/Review/InstallerRoundtrip-0.3.7.json', 'INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes': ('Saved/Review/InstallerExtraction-0.3.7.json', 'INSTALLER_EXTRACTION_HASH_PASS')}
for name, (relative, expected) in reports.items():
    path = root/relative
    report = json.loads(path.read_text(encoding='utf-8-sig'))
    assert report['result'] == expected, (name, report)
    shutil.copy2(path, out/(name+'.json'))
for layout in ['Wide', 'Compact']:
    capture = root/'Saved/QA/SettingsReviewShipping'/layout/'Saved/Review/SettingsReview'
    target = out/layout
    target.mkdir(exist_ok=True)
    for name in ['Display.png', 'Audio.png', 'Accessibility.png', 'Controls.png']:
        source = capture/name
        assert source.stat().st_size > 10000
        shutil.copy2(source, target/name)

preserved = []
for version, receipt in [
    ('0.3.0', 'Docs/WindowsOverhaul/release-manifest.json'),
    ('0.3.1', 'Docs/Windows-0.3.1-Validation/release-manifest.json'),
    ('0.3.2', 'Docs/Windows-0.3.2-Validation/release-manifest.json'),
    ('0.3.3', 'Docs/Windows-0.3.3-Validation/release-manifest.json'),
    ('0.3.4', 'Docs/Windows-0.3.4-Validation/release-manifest.json'),
    ('0.3.5', 'Docs/Windows-0.3.5-Validation/release-manifest.json'),
    ('0.3.6', 'Docs/Windows-0.3.6-Validation/release-manifest.json')]:
    previous = json.loads((root/receipt).read_text())
    assert sha(root/previous['installer']) == previous['installerSHA256']
    for entry in previous['packageFiles']:
        assert sha(root/('Artifacts/Windows-'+version)/entry['path']) == entry['sha256'], entry['path']
    preserved.append(version)
baseline = root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z/ForbiddenCode-Windows-Baseline.zip'
assert sha(baseline) == 'f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'

installer = root/'Artifacts/Installer-0.3.7/ForbiddenCode-Survivor-0.3.7-Setup.exe'
package = root/'Artifacts/Windows-0.3.7'
if not (package/'ForbiddenCodeSurvivor.exe').exists():
    package = package/'Windows'
manifest = {
    'result': 'SETTINGS_RELEASE_PASS', 'version': '0.3.7',
    'createdUTC': datetime.now(timezone.utc).isoformat(),
    'configuration': 'Shipping', 'engine': 'UE 5.8.2',
    'installer': installer.relative_to(root).as_posix(),
    'installerBytes': installer.stat().st_size, 'installerSHA256': sha(installer),
    'previousPackagesVerified': preserved, 'originalBaselineVerified': True,
    'reports': list(reports), 'musicSources': [], 'packageFiles': []}
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
print('SETTINGS_RELEASE_PASS', manifest['installerSHA256'], 'previous releases and original baseline unchanged')
