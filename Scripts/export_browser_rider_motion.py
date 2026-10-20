"""Export the complete already-authored, board-safe rider motion set for WebGL."""
import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];source=ROOT/'Scripts/export_browser_models.py';exec(compile(source.read_text().split("if '--environment-only' in sys.argv:")[0],str(source),'exec'))
for ident,height in [('NeonExorcist',1.8),('Vex',1.8),('Rook',1.95),('Zero',1.88),('Mira',1.8)]:
 path=ROOT/f'ContentSource/Meshy/{ident}/assembled/{ident}-RidingRig.blend';bpy.ops.wm.open_mainfile(filepath=str(path));rig=bpy.data.objects[ident+'Rig'];meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
 for mesh in meshes:
  mesh.data.materials.clear();mesh.data.materials.append(material(('NeonHoverboard' if ident=='NeonExorcist' else ident+'Board') if mesh.name=='Hoverboard' else ident))
 for obj in list(bpy.context.scene.objects):
  if obj not in [rig]+meshes:bpy.data.objects.remove(obj,do_unlink=True)
 actions=[]
 for state in ['HoverIdle','Cruise','Accelerate','Brake','TurnLeft','TurnRight','Cast','CastBlood','CastStorm','HoverIdleAlt','Land','Fall','Hit','Defeat']:
  a=bpy.data.actions.get('A_'+state)
  if a:a.name={'CastBlood':'Attack135','CastStorm':'Cast133','HoverIdleAlt':'Attack250'}.get(state,state);actions.append(a)
 export(ident,rig,meshes,height,actions,str(path.relative_to(ROOT)),16000 if ident=='NeonExorcist' else 6500)
p=ROOT/'Web/assets/model-manifest.json';manifest=json.loads(p.read_text());ids={e['id'] for e in REPORT};manifest['models']=[e for e in manifest['models'] if e['id'] not in ids]+REPORT;p.write_text(json.dumps(manifest,indent=2))
