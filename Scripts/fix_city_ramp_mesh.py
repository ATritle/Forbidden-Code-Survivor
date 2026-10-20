"""Run with Blender: rebuild only the ramp with outward faces and explicit convex collision."""
from pathlib import Path
import bpy, json
from mathutils import Vector
root = Path(__file__).resolve().parents[1]
(root/'Saved/Review').mkdir(parents=True,exist_ok=True)
target = root / 'ContentSource/City/Kit/SM_CityRamp.fbx'
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = .01
verts = [(-50,-50,-10),(-50,50,-10),(50,50,-10),(50,-50,-10),(-50,-50,0),(-50,50,0),(50,50,100),(50,-50,100)]
faces = [(1,2,3,0),(7,6,5,4),(4,5,1,0),(5,6,2,1),(6,7,3,2),(7,4,0,3)]
mesh = bpy.data.meshes.new('OutwardWedge')
mesh.from_pydata(verts, [], faces)
mesh.update()
obj = bpy.data.objects.new('SM_CityRamp', mesh)
bpy.context.collection.objects.link(obj)
# One planar, consistently oriented UV island per face; material uses world coordinates.
uv = mesh.uv_layers.new(name='UVMap')
for poly in mesh.polygons:
    for loop, value in zip(poly.loop_indices, [(0,0),(1,0),(1,1),(0,1)]): uv.data[loop].uv = value
top = mesh.polygons[1].normal
assert top.z > 0 and top.x < 0, tuple(top)
coll = obj.copy(); coll.data = mesh.copy(); coll.name = 'UCX_SM_CityRamp_00'
bpy.context.collection.objects.link(coll)
obj.select_set(True); coll.select_set(True); bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'MESH'},global_scale=1.,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
report = {'result':'RAMP_SOURCE_PASS','topNormal':list(top),'faces':len(faces),'convexCollision':True,'source':str(target)}
(root/'Saved/Review/RampSource.json').write_text(json.dumps(report,indent=2))
print('RAMP_SOURCE_PASS', report)
