"""Blender companion to prepare_browser_assets.py; sources remain read-only."""
import bpy, json, math, struct, sys
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Web/assets/models'
CACHE = ROOT / 'Saved/BrowserAssetCache'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = []

def material(asset):
    mat = bpy.data.materials.new('Browser_' + asset)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = .58
    bsdf.inputs['Metallic'].default_value = .38
    for channel, socket in [('base_color', 'Base Color'), ('normal', 'Normal'), ('emission', 'Emission Color')]:
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(CACHE / asset / (channel + '.jpg')))
        if channel == 'normal':
            tex.image.colorspace_settings.name = 'Non-Color'
            normal = nodes.new('ShaderNodeNormalMap')
            normal.inputs['Strength'].default_value = .65
            links.new(tex.outputs['Color'], normal.inputs['Color'])
            links.new(normal.outputs['Normal'], bsdf.inputs[socket])
        else:
            links.new(tex.outputs['Color'], bsdf.inputs[socket])
    bsdf.inputs['Emission Strength'].default_value = 1.8
    return mat

def compact_meshes(meshes, budget):
    for mesh in meshes:
        triangles = sum(len(p.vertices)-2 for p in mesh.data.polygons)
        if triangles > budget:
            bpy.ops.object.select_all(action='DESELECT')
            mesh.select_set(True)
            bpy.context.view_layer.objects.active = mesh
            modifier = mesh.modifiers.new('BrowserTriangleBudget', 'DECIMATE')
            modifier.ratio = budget / triangles
            # Evaluate decimation in bind space before the skeletal deformation.
            bpy.ops.object.modifier_move_up(modifier=modifier.name)
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        for polygon in mesh.data.polygons:
            polygon.use_smooth = True

def export(asset, rig, meshes, height, actions, source, budget):
    rig.animation_data_clear()
    for bone in rig.pose.bones:
        for constraint in bone.constraints:
            constraint.mute = True
        bone.location = (0,0,0)
        bone.rotation_mode = 'QUATERNION'
        bone.rotation_quaternion = (1,0,0,0)
        bone.scale = (1,1,1)
    bpy.context.view_layer.update()
    coords = [mesh.matrix_world @ v.co for mesh in meshes for v in mesh.data.vertices]
    lo = Vector([min(v[i] for v in coords) for i in range(3)])
    hi = Vector([max(v[i] for v in coords) for i in range(3)])
    scale = height / (hi.z-lo.z)
    wrapper = bpy.data.objects.new(asset, None)
    bpy.context.scene.collection.objects.link(wrapper)
    world = rig.matrix_world.copy()
    rig.parent = wrapper
    rig.matrix_world = world
    # Existing Meshy skin meshes are parented to the rig; retain bind transforms.
    for mesh in meshes:
        if mesh.parent != rig:
            world = mesh.matrix_world.copy()
            mesh.parent = rig
            mesh.matrix_world = world
    wrapper.scale = (scale,)*3
    wrapper.location = (-(lo.x+hi.x)*.5*scale, -(lo.y+hi.y)*.5*scale, -lo.z*scale)
    keep = set(actions)
    for action in list(bpy.data.actions):
        if action not in keep:
            bpy.data.actions.remove(action)
    rig.animation_data_create()
    for action in actions:
        track = rig.animation_data.nla_tracks.new()
        track.name = action.name
        strip = track.strips.new(action.name, int(action.frame_range[0]), action)
        if action.slots:
            strip.action_slot = action.slots[0]
    compact_meshes(meshes, budget)
    bpy.context.scene.frame_set(1)
    bpy.ops.object.select_all(action='DESELECT')
    wrapper.select_set(True)
    rig.select_set(True)
    for mesh in meshes:
        mesh.select_set(True)
    bpy.context.view_layer.objects.active = rig
    path = OUT / (asset + '.glb')
    bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True,
        export_yup=True, export_animations=True, export_animation_mode='NLA_TRACKS',
        export_force_sampling=True, export_frame_range=False,
        export_image_format='JPEG', export_image_quality=85,
        export_skins=True, export_def_bones=True, export_apply=False,
        export_optimize_animation_size=True)
    data=path.read_bytes()
    length, kind=struct.unpack_from('<II',data,12)
    doc=json.loads(data[20:20+length])
    clips=[]
    for animation in doc.get('animations',[]):
        accessors=[doc['accessors'][s['input']] for s in animation['samplers']]
        clips.append({'name':animation['name'],'duration':max(a.get('max',[0])[0] for a in accessors)-min(a.get('min',[0])[0] for a in accessors)})
    record={'id':asset,'path':'assets/models/'+path.name,'bytes':path.stat().st_size,'heightMeters':height,
        'up':'+Y','forward':'+Z','origin':'feet or hoverboard underside, centered XZ',
        'triangles':sum(sum(len(p.vertices)-2 for p in m.data.polygons) for m in meshes),
        'animations':clips,'source':source,'textureSize':1024 if asset=='NeonExorcist' else 512}
    REPORT.append(record)
    print('BROWSER_MODEL_READY', json.dumps(record), flush=True)

def player():
    path = ROOT/'ContentSource/Meshy/NeonExorcist/assembled/NeonExorcist-RidingRig.blend'
    bpy.ops.wm.open_mainfile(filepath=str(path))
    scene = bpy.context.scene
    rig = bpy.data.objects['NeonExorcistRig']
    meshes = [bpy.data.objects['char1'], bpy.data.objects['Hoverboard']]
    for obj in list(scene.objects):
        if obj not in [rig]+meshes:
            bpy.data.objects.remove(obj, do_unlink=True)
    for mesh in meshes:
        mesh.data.materials.clear()
        mesh.data.materials.append(material('NeonHoverboard' if mesh.name=='Hoverboard' else 'NeonExorcist'))
    states=['HoverIdle','Cruise','Cast','CastBlood','CastStorm','HoverIdleAlt','Hit','Land','Fall']
    actions=[]
    for state in states:
        action=bpy.data.actions.get('A_'+state)
        action.name={'CastBlood':'Attack135','CastStorm':'Cast133','HoverIdleAlt':'Attack250'}.get(state,state)
        actions.append(action)
    export('NeonExorcist', rig, meshes, 1.8, actions, str(path.relative_to(ROOT)).replace('\\','/'), 16000)

def enemy(entry):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene
    scene.render.fps=30
    source=ROOT/'ContentSource/Meshy'/entry['id']
    bpy.ops.import_scene.fbx(filepath=str(source/'rig.fbx'))
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    meshes=[o for o in scene.objects if o.type=='MESH']
    rig.animation_data_clear()
    actions=[]
    for name in ['Walk','Run']:
        before=set(scene.objects)
        bpy.ops.import_scene.fbx(filepath=str(source/(name.lower()+'.fbx')))
        imported=[o for o in scene.objects if o not in before]
        animated=next(o for o in imported if o.type=='ARMATURE')
        action=animated.animation_data.action
        action.name=name
        action.use_fake_user=True
        actions.append(action)
        for obj in imported:
            bpy.data.objects.remove(obj,do_unlink=True)
    mat=material(entry['id'])
    for mesh in meshes:
        mesh.data.materials.clear()
        mesh.data.materials.append(mat)
    export(entry['id'],rig,meshes,entry['height']/100,actions,
        str(source.relative_to(ROOT)).replace('\\','/')+'/rig.fbx + walk.fbx + run.fbx',3500 if entry['tier']!='BOSS' else 6000)

def environment(asset, width):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source=ROOT/'ContentSource/City/Meshy'/asset/'model.fbx'
    bpy.ops.import_scene.fbx(filepath=str(source))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    coords=[m.matrix_world@v.co for m in meshes for v in m.data.vertices]
    lo=Vector([min(v[i] for v in coords) for i in range(3)])
    hi=Vector([max(v[i] for v in coords) for i in range(3)])
    scale=width/max(hi.x-lo.x,hi.y-lo.y)
    mat=material(asset)
    for mesh in meshes:
        coords=[mesh.matrix_world@v.co for v in mesh.data.vertices]
        mesh.parent=None
        mesh.matrix_world.identity()
        for vertex,co in zip(mesh.data.vertices,coords):
            vertex.co=((co.x-(hi.x+lo.x)*.5)*scale,(co.y-(hi.y+lo.y)*.5)*scale,(co.z-lo.z)*scale)
        mesh.data.materials.clear()
        mesh.data.materials.append(mat)
        for polygon in mesh.data.polygons:
            polygon.use_smooth=True
    bpy.ops.object.select_all(action='DESELECT')
    for mesh in meshes:
        mesh.select_set(True)
    path=OUT/(asset+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_yup=True,export_animations=False,export_image_format='JPEG',export_image_quality=85)
    record={'id':asset,'kind':'environment','path':'assets/models/'+path.name,'bytes':path.stat().st_size,
        'dimensionsMeters':{'x':(hi.x-lo.x)*scale,'y':(hi.z-lo.z)*scale,'z':(hi.y-lo.y)*scale},
        'maxHorizontalWidthMeters':width,'heightMeters':(hi.z-lo.z)*scale,'up':'+Y','forward':'+Z',
        'origin':'bottom at Y0, centered XZ','triangles':sum(sum(len(p.vertices)-2 for p in m.data.polygons) for m in meshes),
        'source':str(source.relative_to(ROOT)).replace('\\','/'),'animations':[],'textureSize':512,
        'collision':'visual mesh only; use independent simplified collision boxes'}
    REPORT.append(record)
    print('BROWSER_ENVIRONMENT_READY',json.dumps(record),flush=True)

if '--environment-only' in sys.argv:
    existing=json.loads((OUT.parent/'model-manifest.json').read_text())
    REPORT.extend(entry for entry in existing['models'] if entry.get('kind')!='environment')
else:
    player()
    for entry in json.loads((ROOT/'ContentSource/Enemies/CyberRoster/roster.json').read_text()):
        enemy(entry)
for asset,width in [('ShoppingBlock',24.0),('MarketKiosk',4.2),('RooftopShrine',4.6)]:
    environment(asset,width)
(OUT.parent/'model-manifest.json').write_text(json.dumps({'format':'glTF 2.0','units':'meters','up':'+Y','forward':'+Z','models':REPORT},indent=2))
print('BROWSER_MODELS_READY',flush=True)
