"""Fetch only the approved FBX rigs, included clips, and PBR maps returned by Meshy."""
import json, urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'ContentSource/Enemies/CyberRoster/download-manifest.json').read_text())
for enemy in manifest:
    for asset in enemy['files']:
        target=root/'ContentSource/Meshy'/enemy['id']/asset['name']
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists() and target.stat().st_size>100:continue
        request=urllib.request.Request(asset['url'],headers={'User-Agent':'DeckBuilder-AssetImport/1.0'})
        with urllib.request.urlopen(request,timeout=90) as response:target.write_bytes(response.read())
    print('DOWNLOADED',enemy['id'],flush=True)
