"""Export existing approved Windows riders, spirit and enemy combat clips; no generation costs."""
import bpy,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'Scripts/export_browser_models.py'
exec(compile(source.read_text().split("if '--environment-only' in sys.argv:")[0],str(source),'exec'))
for ident,height in [('Vex',1.8),('Rook',1.95),('Zero',1.88),('Mira',1.8)]:
 path=ROOT/f'ContentSource/Meshy/{ident}/assembled/{ident}-RidingRig.blend'
 bpy.ops.wm.open_mainfile(filepath=str(path));rig=bpy.data.objects[ident+'Rig'];meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
 for mesh in meshes:
  mesh.data.materials.clear();mesh.data.materials.append(material(ident+'Board' if mesh.name=='Hoverboard' else ident))
 for obj in list(bpy.context.scene.objects):
  if obj not in [rig]+meshes:bpy.data.objects.remove(obj,do_unlink=True)
 actions=[]
 for state in ['HoverIdle','Cruise','Cast','CastBlood','CastStorm','HoverIdleAlt','Land','Fall','Hit']:
  a=bpy.data.actions.get('A_'+state)
  if a:a.name={'CastBlood':'Attack135','CastStorm':'Cast133','HoverIdleAlt':'Attack250'}.get(state,state);actions.append(a)
 export(ident,rig,meshes,height,actions,str(path.relative_to(ROOT)),6500)
# Existing enemy geometry stays unchanged: append only the Meshy attack clip to its GLB rig.
for entry in json.loads((ROOT/'ContentSource/Enemies/CyberRoster/roster.json').read_text()):
 ident=entry['id'];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(OUT/(ident+'.glb')))
 rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');before=set(bpy.context.scene.objects)
 bpy.ops.import_scene.fbx(filepath=str(ROOT/f'ContentSource/WindowsOverhaul/Enemies/{ident}/attack.fbx'))
 imported=[o for o in bpy.context.scene.objects if o not in before];animated=next(o for o in imported if o.type=='ARMATURE');action=animated.animation_data.action;action.name='Attack';action.use_fake_user=True
 for obj in imported:bpy.data.objects.remove(obj,do_unlink=True)
 rig.animation_data_create();track=rig.animation_data.nla_tracks.new();track.name='Attack';strip=track.strips.new('Attack',int(action.frame_range[0]),action)
 if action.slots:strip.action_slot=action.slots[0]
 bpy.ops.export_scene.gltf(filepath=str(OUT/(ident+'.glb')),export_format='GLB',export_animations=True,export_animation_mode='NLA_TRACKS',export_force_sampling=True,export_frame_range=False,export_image_format='JPEG',export_image_quality=85,export_skins=True)
 print('BROWSER_COMBAT_CLIP',ident,flush=True)
# Spirit uses its approved humanoid rig and locomotion; sources remain untouched.
bpy.ops.wm.read_factory_settings(use_empty=True);folder=ROOT/'ContentSource/WindowsOverhaul/DigitalSpirit';bpy.ops.import_scene.fbx(filepath=str(folder/'rig.fbx'))
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];actions=[]
for name in ['Walk','Run']:
 before=set(bpy.context.scene.objects);bpy.ops.import_scene.fbx(filepath=str(folder/(name.lower()+'.fbx')));imported=[o for o in bpy.context.scene.objects if o not in before];a=next(o for o in imported if o.type=='ARMATURE').animation_data.action;a.name=name;a.use_fake_user=True;actions.append(a)
 for obj in imported:bpy.data.objects.remove(obj,do_unlink=True)
for mesh in meshes:mesh.data.materials.clear();mesh.data.materials.append(material('DigitalSpirit'))
export('DigitalSpirit',rig,meshes,.85,actions,str(folder.relative_to(ROOT)),1800)
p=ROOT/'Web/assets/model-manifest.json';manifest=json.loads(p.read_text());manifest['models']=[e for e in manifest['models'] if e['id'] not in {x['id'] for x in REPORT}]+REPORT;p.write_text(json.dumps(manifest,indent=2))
print('PARITY_ASSET_EXPORT_COMPLETE',flush=True)
