import hashlib
import json
from pathlib import Path
from urllib.request import urlopen

root=Path(__file__).resolve().parents[1]
folder=root/'ContentSource/WindowsOverhaul/DigitalSpirit'
data=json.loads((folder/'rig-download.json').read_text())
files={'rig.fbx':data['rigged_character_fbx_url']}
for key,value in data.get('basic_animations',{}).items():
    if isinstance(value,str) and '.fbx' in value:
        name='run.fbx' if 'run' in key.lower() else 'walk.fbx' if 'walk' in key.lower() else None
        if name: files[name]=value
report={}
for name,url in files.items():
    path=folder/name
    with urlopen(url,timeout=120) as response:path.write_bytes(response.read())
    raw=path.read_bytes()
    if not raw.startswith(b'Kaydara FBX Binary'):raise RuntimeError('Not a valid FBX: '+name)
    report[name]={'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
    print(name,len(raw),'verified',flush=True)
(folder/'verified.json').write_text(json.dumps(report,indent=2))
ledger_path=root/'Docs/WindowsOverhaul/meshy-ledger.json'
ledger=json.loads(ledger_path.read_text(encoding='utf-8-sig'))
ledger['spiritRig']={'task_id':'01a15203-c8bd-702e-a6bb-d658a9ba76dc','status':'SUCCEEDED','credits':5}
ledger['consumedCredits']=80
for model in ledger['models']:model['status']='SUCCEEDED'
ledger_path.write_text(json.dumps(ledger,indent=2),encoding='utf-8')
