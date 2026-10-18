import bpy, json, sys
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]/'ContentSource/Meshy/NeonExorcist'
bpy.ops.wm.read_factory_settings(use_empty=True)
source=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'board.glb'
if source.endswith('.fbx'): bpy.ops.import_scene.fbx(filepath=str(root/source))
else: bpy.ops.import_scene.gltf(filepath=str(root/source))
report=[]
for obj in bpy.context.scene.objects:
    item={'name':obj.name,'type':obj.type,'dimensions':list(obj.dimensions),'scale':list(obj.scale)}
    if obj.type=='MESH':
        item['vertices']=len(obj.data.vertices);item['materials']=[m.name if m else None for m in obj.data.materials]
        item['bounds']=[list(obj.matrix_world@Vector(c)) for c in obj.bound_box]
    if obj.type=='ARMATURE': item['bones']=[{'name':b.name,'head':list(b.head_local),'tail':list(b.tail_local),'parent':b.parent.name if b.parent else None} for b in obj.data.bones]
    report.append(item)
(root/(Path(source).stem+'-inspect.json')).write_text(json.dumps(report,indent=2))
print('NEON_INSPECT_DONE',source)
