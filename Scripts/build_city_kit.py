"""Beveled architectural kit in centimeters, with explicit convex UCX collision."""
from pathlib import Path
import bpy, math
root=Path(__file__).resolve().parents[1]/'ContentSource/City/Kit'
root.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=0.01

def export(name,obj,bevel=0,collider=None):
    obj.name=name
    if bevel:
        mod=obj.modifiers.new('Manufactured edge bevel','BEVEL');mod.width=bevel;mod.segments=3
        bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=mod.name)
        mod=obj.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');mod.keep_sharp=True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
    if collider:
        collider.name='UCX_'+name+'_00';collider.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(root/(name+'.fbx')),use_selection=True,object_types={'MESH'},global_scale=1.0,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
    bpy.data.objects.remove(obj,do_unlink=True)
    if collider:bpy.data.objects.remove(collider,do_unlink=True)

bpy.ops.mesh.primitive_cube_add(size=100);cube=bpy.context.object
coll=cube.copy();coll.data=cube.data.copy();bpy.context.collection.objects.link(coll)
export('SM_CityPanel',cube,0.9,coll)
bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=50,depth=100);cyl=bpy.context.object
coll=cyl.copy();coll.data=cyl.data.copy();bpy.context.collection.objects.link(coll)
export('SM_CityPipe',cyl,0.6,coll)
verts=[(-50,-50,-10),(-50,50,-10),(50,50,-10),(50,-50,-10),(-50,-50,0),(-50,50,0),(50,50,100),(50,-50,100)]
faces=[(1,2,3,0),(7,6,5,4),(4,5,1,0),(5,6,2,1),(6,7,3,2),(7,4,0,3)]
mesh=bpy.data.meshes.new('Wedge');mesh.from_pydata(verts,[],faces);mesh.update()
obj=bpy.data.objects.new('Ramp',mesh);bpy.context.collection.objects.link(obj)
coll=obj.copy();coll.data=obj.data.copy();bpy.context.collection.objects.link(coll)
export('SM_CityRamp',obj,0,coll)
# A curved roof section, used on market awnings and shrine gate canopies.
verts=[]
for y in [-50,50]:
    for i in range(13):
        x=-50+i*100/12;z=10+15*(abs(x)/50)**2
        verts.extend([(x,y,z),(x,y,z-6)])
faces=[]
for i in range(12):
    a=2*i;b=26+a
    faces.extend([(a,a+2,b+2,b),(a+1,b+1,b+3,a+3),(a,a+1,a+3,a+2),(b,b+2,b+3,b+1)])
faces.extend([(0,26,27,1),(24,25,51,50)])
mesh=bpy.data.meshes.new('Canopy');mesh.from_pydata(verts,[],faces);mesh.update()
obj=bpy.data.objects.new('Roof',mesh);bpy.context.collection.objects.link(obj)
export('SM_CityCanopy',obj,0)
print('CITY_KIT_READY',root)
