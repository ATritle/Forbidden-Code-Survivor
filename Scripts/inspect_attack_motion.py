import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'ContentSource/Meshy/NeonExorcist'
result={}
for filename in ['source-attack135.fbx','source-cast133.fbx','assembled/A_CastBlood.fbx','assembled/A_CastStorm.fbx']:
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/filename))
    arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    span=arm.animation_data.action.frame_range;data=[]
    for i in range(5):
        bpy.context.scene.frame_set(round(span[0]+(span[1]-span[0])*i/4))
        data.append({name:{'head':list(arm.matrix_world@arm.pose.bones[name].head),'mode':arm.pose.bones[name].rotation_mode,'q':list(arm.pose.bones[name].matrix_basis.to_quaternion())} for name in ['LeftHand','RightHand','LeftArm','RightArm']})
    result[filename]=data
(root/'attack-motion-audit.json').write_text(json.dumps(result,indent=2))
print('ATTACK_MOTION_AUDIT_READY')
