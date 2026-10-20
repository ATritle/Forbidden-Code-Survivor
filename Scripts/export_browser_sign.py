import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];source=ROOT/'Scripts/export_browser_models.py';exec(compile(source.read_text().split("if '--environment-only' in sys.argv:")[0],str(source),'exec'));environment('ChallengeSign',3.4)
p=ROOT/'Web/assets/model-manifest.json';manifest=json.loads(p.read_text());manifest['models']=[e for e in manifest['models'] if e['id']!='ChallengeSign']+REPORT;p.write_text(json.dumps(manifest,indent=2))
