import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];source=ROOT/'Scripts/export_browser_models.py';code=source.read_text().split("if '--environment-only' in sys.argv:")[0];code=code.replace("ROOT/'ContentSource/City/Meshy'/asset", "ROOT/'ContentSource/WindowsOverhaul'/asset");exec(compile(code,str(source),'exec'))
for ident,width in [('Shopfront',4),('ServiceCabinet',1.3)]:environment(ident,width)
p=ROOT/'Web/assets/model-manifest.json';manifest=json.loads(p.read_text());manifest['models']=[e for e in manifest['models'] if e['id'] not in {r['id'] for r in REPORT}]+REPORT;p.write_text(json.dumps(manifest,indent=2))
