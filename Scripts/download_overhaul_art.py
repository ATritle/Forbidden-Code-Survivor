"""Download only the approved FBX output URLs returned by Meshy's download tool."""
import hashlib
import json
import pathlib
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
ledger_path = ROOT / 'Docs/WindowsOverhaul/meshy-ledger.json'
ledger = json.loads((ROOT / 'Docs/WindowsOverhaul/meshy-downloads.json').read_text(encoding='utf-8-sig'))
for item in ledger['animations']:
    output = ROOT / 'ContentSource/WindowsOverhaul/Enemies' / item['id'] / 'attack.fbx'
    output.parent.mkdir(parents=True, exist_ok=True)
    url = item['download']['structuredContent']['download_url']
    if not output.exists():
        with urllib.request.urlopen(url, timeout=120) as response:
            output.write_bytes(response.read())
    data = output.read_bytes()
    if not data.startswith(b'Kaydara FBX Binary'):
        raise RuntimeError(f'Invalid FBX: {output}')
    item['local_path'] = str(output.relative_to(ROOT)).replace('\\', '/')
    item['sha256'] = hashlib.sha256(data).hexdigest()
    item['bytes'] = len(data)
    print(item['id'], len(data), 'FBX verified', flush=True)
ledger_path.write_text(json.dumps(ledger, indent=2), encoding='utf-8')
