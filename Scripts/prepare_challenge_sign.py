"""Normalize the approved Meshy FBX and render an inspection image with Blender."""
import bpy, json, math
from mathutils import Vector, Matrix
from pathlib import Path

root=Path(__file__).resolve().parents[1]/'ContentSource/City/Meshy/ChallengeSign'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(root/'model.fbx'))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
if not meshes:raise RuntimeError('Meshy sign has no mesh')
coords=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
low=Vector(tuple(min(p[i] for p in coords) for i in range(3)))
high=Vector(tuple(max(p[i] for p in coords) for i in range(3)))
print('SOURCE_SIGN_BOUNDS',list(low),list(high),'EXTENTS',list(high-low))
triangles=sum(len(o.data.polygons) for o in meshes)
# Meshy FBX is imported by Blender with Z up and its front toward -Y.
# Bake all transforms, rotate its front to UE's local +X, and normalize to centimetres.
rotation=Matrix.Rotation(math.pi/2,4,'Z')
for o in meshes:
    transform=rotation@o.matrix_world
    for v in o.data.vertices:v.co=transform@v.co
    o.matrix_world=Matrix.Identity(4)
coords=[v.co for o in meshes for v in o.data.vertices]
low=Vector(tuple(min(p[i] for p in coords) for i in range(3)));high=Vector(tuple(max(p[i] for p in coords) for i in range(3)))
height=high.z-low.z
scale=480/height
width_scale=365/(high.y-low.y)
for o in meshes:
    for v in o.data.vertices:
        v.co=Vector(((v.co.x-(low.x+high.x)/2)*scale,(v.co.y-(low.y+high.y)/2)*width_scale,(v.co.z-low.z)*scale))
# Locate the inset panel's front plane in the interior, away from the thick outer frame.
interior=[v.co.x for o in meshes for v in o.data.vertices if abs(v.co.y)<100 and 275<v.co.z<410]
front=max(interior) if interior else 0
for o in meshes:
    for v in o.data.vertices:v.co.x+=36-front

mat=bpy.data.materials.new('ChallengeSignPBR');mat.use_nodes=True
nodes=mat.node_tree.nodes;links=mat.node_tree.links;bsdf=nodes.get('Principled BSDF')
for name,socket in [('base_color','Base Color'),('metallic','Metallic'),('roughness','Roughness'),('normal','Normal')]:
    tex=nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(root/'model_textures'/(name+'.png')))
    if name!='base_color':tex.image.colorspace_settings.name='Non-Color'
    if name=='normal':
        normal=nodes.new('ShaderNodeNormalMap');links.new(tex.outputs['Color'],normal.inputs['Color']);links.new(normal.outputs['Normal'],bsdf.inputs[socket])
    else:links.new(tex.outputs['Color'],bsdf.inputs[socket])
for o in meshes:
    o.data.materials.clear();o.data.materials.append(mat);o.select_set(True)
    for p in o.data.polygons:p.use_smooth=True
bpy.context.view_layer.objects.active=meshes[0]
bpy.ops.export_scene.fbx(filepath=str(root/'ChallengeSign-UE.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',global_scale=0.01,apply_unit_scale=True,bake_space_transform=False,path_mode='AUTO',add_leaf_bones=False)

scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=12
scene.render.resolution_x=768;scene.render.resolution_y=1024;scene.render.resolution_percentage=100
scene.world.color=(.12,.12,.12)
def aim(obj,p):obj.rotation_euler=(Vector(p)-obj.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(900,-550,460));camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=590;camera.data.clip_end=10000;aim(camera,(0,0,245));scene.camera=camera
for p,power,size in [((500,-500,800),3500000,500),((100,500,500),2000000,450),((-300,-200,650),1500000,350)]:
    bpy.ops.object.light_add(type='AREA',location=p);lamp=bpy.context.object;lamp.data.energy=power;lamp.data.shape='DISK';lamp.data.size=size;aim(lamp,(0,0,220))
scene.view_settings.view_transform='AgX'
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(root/'Meshy-Housing-Inspection.png');bpy.ops.render.render(write_still=True)
coords=[v.co for o in meshes for v in o.data.vertices]
report={'meshy_task':'01a15029-1807-72aa-b8cd-1d0b5da4bc3f','consumed_credits':15,'target_triangles':12000,'actual_triangles':sum(len(p.vertices)-2 for o in meshes for p in o.data.polygons),'objects':len(meshes),'panel_front_x':36,'height_cm':480,'width_cm':365,'bounds_min':[min(p[i] for p in coords) for i in range(3)],'bounds_max':[max(p[i] for p in coords) for i in range(3)]}
(root/'mesh-audit.json').write_text(json.dumps(report,indent=2))
print('CHALLENGE_SIGN_NORMALIZED',json.dumps(report))
