"""Blender source audit, no transforms or asset edits. Run blender -b --python this_file."""
import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
roster=json.loads((root/'ContentSource/Enemies/CyberRoster/roster.json').read_text())
report=[]
for entry in roster:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(root/'ContentSource/Meshy'/entry['id']/'rig.fbx'))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    arms=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes)
    bones=sum(len(a.data.bones) for a in arms)
    if triangles>15000 or not arms or not meshes:raise RuntimeError('Invalid source rig '+entry['id'])
    report.append({'id':entry['id'],'triangles':triangles,'bones':bones,'parts':len(meshes)})
    print('CYBER_SOURCE_AUDIT',entry['id'],triangles,bones,flush=True)
(root/'Saved/CyberSourceAudit.json').write_text(json.dumps(report,indent=2))
