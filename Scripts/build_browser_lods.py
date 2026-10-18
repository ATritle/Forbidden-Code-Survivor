"""Generate geometry-only low LODs from shipped GLBs; preserve original art/rigs."""
import bpy,json,hashlib,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'Web/assets/models'
manifest=json.loads((ROOT/'Web/assets/model-manifest.json').read_text(encoding='utf-8'))
out=folder/'low';out.mkdir(exist_ok=True)
records=[]
for entry in manifest['models']:
    if entry.get('kind')=='environment':continue
    source=ROOT/'Web'/entry['path'];digest=hashlib.sha256(source.read_bytes()).hexdigest()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    for rig in [o for o in bpy.context.scene.objects if o.type=='ARMATURE']:
        rig.animation_data_clear();rig.data.pose_position='REST'
        for bone in rig.pose.bones:
            bone.location=(0,0,0);bone.rotation_mode='QUATERNION';bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
    for mesh in meshes:
        triangles=sum(len(p.vertices)-2 for p in mesh.data.polygons)
        budget=(2500 if mesh.name!='Hoverboard' else 350) if entry['id']=='NeonExorcist' else (1800 if entry['id']=='BroadcastSovereign' else 900)
        if triangles>budget:
            bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
            modifier=mesh.modifiers.new('BrowserLowLOD','DECIMATE');modifier.ratio=budget/triangles
            while mesh.modifiers.find(modifier.name)>0:bpy.ops.object.modifier_move_up(modifier=modifier.name)
            bpy.ops.object.modifier_apply(modifier=modifier.name)
    target=out/source.name
    bpy.ops.export_scene.gltf(filepath=str(target),export_format='GLB',export_yup=True,export_animations=False,export_skins=True,export_materials='NONE')
    data=target.read_bytes();length=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+length])
    assert doc.get('skins') and not doc.get('animations')
    assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
    record={'id':entry['id'],'path':'assets/models/low/'+source.name,'bytes':target.stat().st_size,'triangles':sum(sum(len(p.vertices)-2 for p in m.data.polygons) for m in meshes),'sourceSha256':digest,'sourcePath':entry['path']}
    records.append(record);print('LOW_LOD_READY',json.dumps(record),flush=True)
(ROOT/'Web/assets/low-lod-manifest.json').write_text(json.dumps({'models':records},indent=2),encoding='utf-8')
print('BROWSER_LOW_LODS_PASS',len(records),flush=True)
