"""Collect real playback/installer receipts and verify preserved Windows releases."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
out = root/'Docs/Windows-0.3.6-Validation'
out.mkdir(parents=True, exist_ok=True)

def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4*1024*1024), b''):
            value.update(chunk)
    return value.hexdigest()

reports = {
    'MusicPlayer-Shipping': ('Saved/QA/SignalCompactPlayerShipping/Saved/Review/MusicPlayer/report.json', 'MUSIC_PLAYER_PASS'),
    'MusicPlayer-Restart': ('Saved/QA/SignalCompactPlayerShipping/Saved/Review/MusicPlayer/reload.json', 'MUSIC_PLAYER_RELOAD_PASS'),
    'Package': ('Saved/Review/SignalCompactPackagedSmoke/report.json', 'PACKAGED_RELEASE_PASS'),
    'Installer': ('Saved/Review/InstallerRoundtrip-0.3.6.json', 'INSTALLER_ROUNDTRIP_PASS'),
    'Installer-Hashes': ('Saved/Review/InstallerExtraction-0.3.6.json', 'INSTALLER_EXTRACTION_HASH_PASS'),
}
for name, (relative, expected) in reports.items():
    path = root/relative
    report = json.loads(path.read_text(encoding='utf-8-sig'))
    assert report['result'] == expected, (name, report)
    if name.startswith('MusicPlayer') and name.endswith('Shipping'):
        assert report['peak'] > .003 and report['tracks'] == 4 and report['effectVoices'] == 18 and report['spectrumChange'] > .001
    shutil.copy2(path, out/(name+'.json'))
capture = root/'Saved/QA/SignalCompactPlayerShipping/Saved/Review/MusicPlayer'
for name in ['SignalDock.png', 'SignalPlaylist.png', 'SignalDockMuted.png', 'SignalDockUnmuted.png', 'MusicPlayback.wav']:
    path = capture/name
    assert path.stat().st_size > 10000
    shutil.copy2(path, out/name)

preserved = []
for version, receipt in [
    ('0.3.0', 'Docs/WindowsOverhaul/release-manifest.json'),
    ('0.3.1', 'Docs/Windows-0.3.1-Validation/release-manifest.json'),
    ('0.3.2', 'Docs/Windows-0.3.2-Validation/release-manifest.json'),
    ('0.3.3', 'Docs/Windows-0.3.3-Validation/release-manifest.json'),
    ('0.3.4', 'Docs/Windows-0.3.4-Validation/release-manifest.json'),
    ('0.3.5', 'Docs/Windows-0.3.5-Validation/release-manifest.json')]:
    previous = json.loads((root/receipt).read_text())
    assert sha(root/previous['installer']) == previous['installerSHA256']
    for entry in previous['packageFiles']:
        assert sha(root/('Artifacts/Windows-'+version)/entry['path']) == entry['sha256'], entry['path']
    preserved.append(version)
baseline = root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z/ForbiddenCode-Windows-Baseline.zip'
assert sha(baseline) == 'f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'

installer = root/'Artifacts/Installer-0.3.6/ForbiddenCode-Survivor-0.3.6-Setup.exe'
package = root/'Artifacts/Windows-0.3.6'
if not (package/'ForbiddenCodeSurvivor.exe').exists():
    package = package/'Windows'
manifest = {
    'result': 'SIGNAL_DOCK_RELEASE_PASS', 'version': '0.3.6',
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
print('SIGNAL_DOCK_RELEASE_PASS', manifest['installerSHA256'], 'previous releases and original baseline unchanged')
