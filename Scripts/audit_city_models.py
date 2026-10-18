import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'ContentSource/City/Meshy'
rows=[]
for name in ['ShoppingBlock','MarketKiosk','RooftopShrine']:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(root/name/'model.fbx'))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    vertices=triangles=0
    for obj in meshes:
        obj.data.calc_loop_triangles();vertices+=len(obj.data.vertices);triangles+=len(obj.data.loop_triangles)
    rows.append({'name':name,'objects':len(meshes),'vertices':vertices,'triangles':triangles})
(root/'topology-audit.json').write_text(json.dumps(rows,indent=2))
print('CITY_TOPOLOGY_AUDIT',json.dumps(rows))
