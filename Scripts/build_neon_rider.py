"""Assemble approved Meshy rider/board and bake board-safe animation clips in Blender.
All source FBXs remain untouched. Run Blender --background --python <this file>.
"""
import bpy, math, json
from pathlib import Path
from mathutils import Vector, Matrix, Quaternion

ROOT=Path(__file__).resolve().parents[1]/'ContentSource/Meshy/NeonExorcist'
OUT=ROOT/'assembled';OUT.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.render.fps=30
bpy.ops.import_scene.fbx(filepath=str(ROOT/'rig.fbx'))
rig=next(o for o in scene.objects if o.type=='ARMATURE')
rider=[o for o in scene.objects if o.type=='MESH']
rig.animation_data_clear()
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
# Apply mesh transforms before adding another weighted mesh to the same skeleton.
for obj in rider:
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    for m in obj.data.materials:
        if m:m.name='M_RiderSource'
rig.name='NeonExorcistRig'
before=set(scene.objects)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'board.glb'))
board=next(o for o in scene.objects if o not in before and o.type=='MESH')
board.name='Hoverboard'
coords=[board.matrix_world@v.co for v in board.data.vertices]
lo=Vector(tuple(min(v[i] for v in coords) for i in range(3)))
hi=Vector(tuple(max(v[i] for v in coords) for i in range(3)))
for v,c in zip(board.data.vertices,coords):
    v.co=Vector(((c.x-(lo.x+hi.x)/2)/(hi.x-lo.x)*1.1,(c.y-(lo.y+hi.y)/2)/(hi.y-lo.y)*0.3,(c.z-lo.z)/(hi.z-lo.z)*0.05))
board.matrix_world=Matrix.Identity(4)
for m in board.data.materials:
    if m:m.name='M_BoardSource'
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode='EDIT')
root=rig.data.edit_bones.new('HoverRoot');root.head=(0,0,0);root.tail=(0,0,0.1)
rig.data.edit_bones['Hips'].parent=root
deck=rig.data.edit_bones.new('Hoverboard');deck.head=(0,0,0);deck.tail=(0,0,0.1);deck.parent=root
bpy.ops.object.mode_set(mode='OBJECT')
board.parent=rig;board.matrix_parent_inverse=rig.matrix_world.inverted()
board_group=board.vertex_groups.new(name='Hoverboard');board_group.add(list(range(len(board.data.vertices))),1,'REPLACE')
mod=board.modifiers.new('BoardRigidRig','ARMATURE');mod.object=rig
all_meshes=rider+[board]
# Source rotations use the identical Meshy bind skeleton. Only upper-body motion is retained.
sources={}
for key in ['idle','cast','hit','attack135','cast133','idle250']:
    before=set(scene.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/('source-'+key+'.fbx')))
    imported=[o for o in scene.objects if o not in before]
    arm=next(o for o in imported if o.type=='ARMATURE')
    action=arm.animation_data.action
    # The supplied clip must target this body's bind skeleton before local adaptation.
    shared=[b.name for b in rig.data.bones if b.name in arm.data.bones]
    if len(shared)<24:raise RuntimeError('Source '+key+' does not contain the required humanoid bones')
    rest_error=max(((arm.matrix_world@arm.data.bones[n].head_local)-(rig.matrix_world@rig.data.bones[n].head_local)).length for n in shared)
    if rest_error>0.015:raise RuntimeError('Source '+key+' bind pose differs by '+str(rest_error)+' meters')
    sources[key]=(arm,action.frame_range.copy())
    print('SOURCE_COMPATIBLE',key,'bindError',rest_error,'frames',list(action.frame_range),flush=True)
    for obj in imported:obj.hide_render=True;obj.hide_set(True)
    arm.hide_set(False)
    for obj in imported:
        if obj.type=='MESH':bpy.data.objects.remove(obj,do_unlink=True)

targets={}
for side,sign in [('Left',1),('Right',-1)]:
    target=bpy.data.objects.new(side+'FootContact',None);scene.collection.objects.link(target)
    target.location=(sign*0.32,0.0,0.19);targets[side]=target
    pole=bpy.data.objects.new(side+'KneePole',None);scene.collection.objects.link(pole);pole.location=(sign*0.30,-0.65,0.60)
    shin=rig.pose.bones[side+'Leg'];ik=shin.constraints.new('IK');ik.target=target;ik.pole_target=pole;ik.chain_count=2;ik.pole_angle=-math.pi/2
    # Orient the foot to its bind-pose world orientation after solving the leg chain.
    foot=rig.pose.bones[side+'Foot'];orientation=foot.constraints.new('COPY_ROTATION');orientation.target=target;orientation.target_space='WORLD';orientation.owner_space='WORLD'
    target.rotation_mode='QUATERNION';target.rotation_quaternion=(rig.matrix_world@foot.bone.matrix_local).to_quaternion()

upper=[b.name for b in rig.pose.bones if not any(k in b.name for k in ['Leg','Foot','Toe','Hips','Hover'])]
clips={'HoverIdle':(2.0,'idle'),'Cruise':(1.5,'idle'),'Accelerate':(0.5,'idle'),'Brake':(0.5,'idle'),
       'TurnLeft':(1.2,'idle'),'TurnRight':(1.2,'idle'),'Cast':(0.8,'cast'),'Hit':(0.45,'hit'),
       'Fall':(1.0,'idle'),'Land':(0.45,'idle'),'Defeat':(1.2,'hit'),
       'CastBlood':(0.8,'attack135'),'CastStorm':(0.8,'cast133'),'HoverIdleAlt':(4.0,'idle250')}
validation={};original_constraints={}
for state,(duration,source_key) in clips.items():
    frames=round(duration*30)+1
    rig.animation_data_clear()
    action=bpy.data.actions.new('A_'+state);action.use_fake_user=True;rig.animation_data_create();rig.animation_data.action=action
    max_error=0.0
    for frame in range(1,frames+1):
        t=(frame-1)/(frames-1)
        src,span=sources[source_key]
        # Loop idle seamlessly; play a useful initial section of cast/hit clips.
        sample=span[0]+(span[1]-span[0])*(t if source_key!='idle' else (0.5-0.5*math.cos(t*2*math.pi)))
        scene.frame_set(round(sample))
        sampled={name:src.pose.bones[name].matrix_basis.to_quaternion() for name in upper if name in src.pose.bones}
        scene.frame_set(frame)
        for pb in rig.pose.bones:
            pb.rotation_mode='QUATERNION';pb.location=(0,0,0);pb.rotation_quaternion=Quaternion();pb.scale=(1,1,1)
        for name,q in sampled.items():rig.pose.bones[name].rotation_quaternion=q
        pulse=math.sin(t*math.pi)
        crouch=0.10+0.012*math.sin(t*2*math.pi)
        lean=0.0;roll=0.0
        if state=='Accelerate':lean=-0.12*pulse;crouch+=0.06*pulse
        if state=='Brake':lean=0.14*pulse;crouch+=0.07*pulse
        if state in ['TurnLeft','TurnRight']:roll=(0.11 if state=='TurnLeft' else -0.11)*math.sin(t*math.pi*2);crouch+=0.025
        if state=='Cruise':lean=-0.035;crouch+=0.025
        if state=='Fall':crouch+=0.08;lean=-0.03
        if state=='Land':crouch+=0.14*math.sin(t*math.pi)*math.exp(-t*1.0)
        if state=='Hit':lean=0.09*pulse;crouch+=0.05*pulse
        if state=='Defeat':crouch+=0.16*(0.5-0.5*math.cos(t*math.pi));lean=0.30*t
        hips=rig.pose.bones['Hips'];world_delta=Vector((lean,0,-crouch+0.05))
        hips.location=hips.bone.matrix_local.to_3x3().inverted()@world_delta
        spine=rig.pose.bones['Spine02'];spine.rotation_quaternion=Quaternion((0,1,0),roll)@spine.rotation_quaternion
        # Deck remains rigid; all rider-foot contacts share its stable local coordinate system.
        for side in targets: targets[side].location.z=0.19
        bpy.context.view_layer.update()
        for side in targets:
            foot=rig.pose.bones[side+'Foot'];error=(rig.matrix_world@foot.head-targets[side].location).length
            max_error=max(max_error,error)
        # Bake visual matrices so exported clips need no Blender IK constraints.
        matrices={pb.name:pb.matrix.copy() for pb in rig.pose.bones}
        for pb in rig.pose.bones:
            for constraint in pb.constraints:constraint.mute=True
        for pb in rig.pose.bones:
            pb.matrix=matrices[pb.name]
            pb.keyframe_insert('location',frame=frame,group=pb.name);pb.keyframe_insert('rotation_quaternion',frame=frame,group=pb.name);pb.keyframe_insert('scale',frame=frame,group=pb.name)
        for pb in rig.pose.bones:
            for constraint in pb.constraints:constraint.mute=False
    for pb in rig.pose.bones:
        for constraint in pb.constraints:constraint.mute=True
    scene.frame_start=1;scene.frame_end=frames;scene.frame_set(1)
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
    for obj in all_meshes:obj.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(OUT/('A_'+state+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},
        add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0.0,axis_forward='-Y',axis_up='Z',path_mode='STRIP',use_mesh_modifiers=True,mesh_smooth_type='FACE')
    validation[state]={'frames':frames,'duration':duration,'maxFootContactErrorMeters':max_error,'source':source_key,'boardBone':'Hoverboard'}
    for pb in rig.pose.bones:
        for constraint in pb.constraints:constraint.mute=False
    print('RIDER_CLIP_READY',state,max_error,flush=True)

# Export one common bind mesh and complete, editable local rig project.
rig.animation_data_clear()
for pb in rig.pose.bones:
    for constraint in pb.constraints:constraint.mute=True
    pb.location=(0,0,0);pb.rotation_quaternion=Quaternion();pb.scale=(1,1,1)
scene.frame_set(1);bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
for obj in all_meshes:obj.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_NeonExorcist.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',path_mode='STRIP',mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'NeonExorcist-RidingRig.blend'))
(OUT/'animation-validation.json').write_text(json.dumps(validation,indent=2))
if any(v['maxFootContactErrorMeters']>0.015 for v in validation.values()):raise RuntimeError('Foot contact exceeded 1.5 cm tolerance')
print('NEON_RIDING_ASSEMBLY_READY',flush=True)
