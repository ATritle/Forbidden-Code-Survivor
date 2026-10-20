"""Verify preserved native packages and installers against their recorded release hashes."""
from pathlib import Path
import hashlib,json
from datetime import datetime,timezone
root=Path(__file__).resolve().parents[1]
def sha(path):
 value=hashlib.sha256()
 with path.open('rb') as stream:
  for chunk in iter(lambda:stream.read(4*1024*1024),b''):value.update(chunk)
 return value.hexdigest()
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
    ('0.4.1', 'Docs/Windows-0.4.1-Validation/release-manifest.json'),
    ('0.4.2', 'Docs/Windows-0.4.2-Validation/release-manifest.json'),
    ('0.4.3', 'Docs/Windows-0.4.3-Validation/release-manifest.json'),
    ('0.4.4', 'Docs/Windows-0.4.4-Validation/release-manifest.json'),
    ('0.4.5', 'Docs/Windows-0.4.5-Validation/release-manifest.json')]:
    previous = json.loads((root/receipt).read_text())
    assert sha(root/previous['installer']) == previous['installerSHA256']
    for entry in previous['packageFiles']:
        assert sha(root/('Artifacts/Windows-'+version)/entry['path']) == entry['sha256'], entry['path']
    preserved.append(version)
baseline = root/'Artifacts/Baselines/Windows-PreOverhaul-20261019T021817Z/ForbiddenCode-Windows-Baseline.zip'
assert sha(baseline) == 'f237b890ddaed08b508397b908774e273922549f4a5bacd458b469386fb775ae'

playtest=json.loads((root/'Docs/All16-Playtest-Validation/manifest.json').read_text())
for entry in playtest['packageFiles']:
 assert sha(root/'Artifacts/Windows-All16-Playtest'/entry['path'])==entry['sha256'],entry['path']
receipt={'all16ExperimentPreserved':True,'result':'PRESERVED_RELEASES_PASS','verifiedUTC':datetime.now(timezone.utc).isoformat(),'versions':preserved,'originalBaselineVerified':True}
(root/'Saved/Review/RarityEconomy-Rollback.json').write_text(json.dumps(receipt,indent=2))
print('PRESERVED_RELEASES_PASS',len(preserved),'previous versions and original baseline verified')
