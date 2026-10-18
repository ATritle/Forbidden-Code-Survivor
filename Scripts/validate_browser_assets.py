"""Roundtrip each browser GLB, validate its skin/clips, and render an art audit."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/BrowserAssetQA'
OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((ROOT/'Web/assets/model-manifest.json').read_text())
report=[]
for entry in manifest['models']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(ROOT/'Web'/entry['path']))
    scene=bpy.context.scene
    meshes=[o for o in scene.objects if o.type=='MESH']
    if entry.get('kind')=='environment':
        coords=[mesh.matrix_world@Vector(c) for mesh in meshes for c in mesh.bound_box]
        lo=[min(c[i] for c in coords) for i in range(3)]
        hi=[max(c[i] for c in coords) for i in range(3)]
        if abs(lo[2])>.001:
            raise RuntimeError('Environment bottom offset: '+entry['id'])
        width=max(hi[0]-lo[0],hi[1]-lo[1])
        if abs(width-entry['maxHorizontalWidthMeters'])>.01:
            raise RuntimeError('Environment scale changed: '+entry['id'])
        report.append({'id':entry['id'],'kind':'environment','bounds':{'min':lo,'max':hi},'widthMeters':width})
        continue
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    for track in rig.animation_data.nla_tracks:
        track.mute=True
    actions=list(bpy.data.actions)
    # Blender names imported actions <clip>_<armature>. Use the first exported clip.
    desired=entry['animations'][0]['name']
    action=next(a for a in actions if a.name.startswith(desired))
    rig.animation_data.action=action
    rig.animation_data.action_slot=action.slots[0]
    samples=[]
    bounds=[]
    for frame in [1, int(action.frame_range[1]/2),int(action.frame_range[1])]:
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        samples.append([list(b.matrix.to_translation()) for b in rig.pose.bones])
        deps=bpy.context.evaluated_depsgraph_get()
        coords=[]
        for obj in meshes:
            evaluated=obj.evaluated_get(deps)
            coords.extend(evaluated.matrix_world@Vector(c) for c in evaluated.bound_box)
        lo=[min(c[i] for c in coords) for i in range(3)]
        hi=[max(c[i] for c in coords) for i in range(3)]
        bounds.append({'min':lo,'max':hi})
    movement=max((Vector(a)-Vector(b)).length for a,b in zip(samples[0],samples[1]))
    if movement<1e-6:
        raise RuntimeError('No skeletal motion after GLB roundtrip: '+entry['id'])
    report.append({'id':entry['id'],'animation':desired,'boneMotion':movement,'frames':list(action.frame_range),'bounds':bounds,'skinMeshes':len(meshes)})
    if entry['id'] not in ['NeonExorcist','WireThrall','BroadcastSovereign']:
        continue
    scene.frame_set(1)
    # A portrait audit lit from the source model's visual front (-Y in Blender).
    height=entry['heightMeters']
    bpy.ops.mesh.primitive_plane_add(size=height*6,location=(0,0,-.005))
    floor=bpy.context.object
    mat=bpy.data.materials.new('AuditFloor')
    mat.diffuse_color=(.018,.025,.05,1)
    floor.data.materials.append(mat)
    bpy.ops.object.camera_add(location=(height*1.25,-height*2.1,height*1.15))
    camera=bpy.context.object
    target=Vector((0,0,height*.48))
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO'
    camera.data.ortho_scale=height*1.3
    scene.camera=camera
    for loc,color,energy in [((height*2,-height*2,height*3),(.65,.8,1),height*height*300),((-height*2,height,height*2),(1,.1,.4),height*height*180)]:
        bpy.ops.object.light_add(type='AREA',location=loc)
        light=bpy.context.object
        light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
        light.data.energy=energy
        light.data.color=color
        light.data.shape='DISK'
        light.data.size=height*2
    scene.world=bpy.data.worlds.new('AuditWorld')
    scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.025,.035,.06,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value=.4
    scene.render.engine='CYCLES'
    scene.cycles.samples=16
    scene.render.resolution_x=640
    scene.render.resolution_y=640
    scene.render.resolution_percentage=100
    scene.render.filepath=str(OUT/(entry['id']+'.png'))
    bpy.ops.render.render(write_still=True)
(OUT/'roundtrip-report.json').write_text(json.dumps(report,indent=2))
print('BROWSER_ASSET_ROUNDTRIP_PASS',len(report),flush=True)
