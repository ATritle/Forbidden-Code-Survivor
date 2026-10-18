"""Author spell-colored sparks, instanced hit pulses, bright beams and enemy hit flashes."""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Scripts'))
from combat_feedback_materials import add_hit_flash

tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
folder='/Game/Art/Feedback'

def node(mat,kind):
    return lib.create_material_expression(mat,kind)

def custom(mat,code,inputs):
    shader=node(mat,unreal.MaterialExpressionCustom)
    pins=[]
    for name,(expression,output) in inputs.items():
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    shader.set_editor_property('inputs',pins);shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);shader.set_editor_property('code',code)
    for name,(expression,output) in inputs.items():
        if not lib.connect_material_expressions(expression,output,shader,name):
            raise RuntimeError('Cannot connect '+name)
    lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)

for name in ['M_PhysicalSpark','M_ConfirmedHit','M_NeonBeam']:
    mat=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('used_with_niagara_sprites',True)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate)
    if name=='M_PhysicalSpark':
        color=node(mat,unreal.MaterialExpressionParticleColor)
        age=node(mat,unreal.MaterialExpressionParticleRelativeTime)
        custom(mat,
            'float2 p=UV*2-1;float core=exp(-dot(p,p)*12);float streak=exp(-p.x*p.x*36)*pow(saturate(1-abs(p.y)),2);'
            'float halo=exp(-dot(p,p)*4)*.12;return (C*(streak+halo)+lerp(C,float3(1,1,1),.4)*core)*pow(1-saturate(Age),.7);',
            {'UV':(uv,''),'C':(color,'RGB'),'Age':(age,'')})
    elif name=='M_ConfirmedHit':
        mat.set_editor_property('disable_depth_test',True)
        inputs={'UV':(uv,'')}
        for index,key in enumerate(['R','G','B','Age']):
            data=node(mat,unreal.MaterialExpressionPerInstanceCustomData)
            data.set_editor_property('data_index',index);inputs[key]=(data,'')
        custom(mat,
            'float2 p=UV*2-1;float r=length(p);float a=atan2(p.y,p.x);float t=saturate(Age);'
            'float ring=exp(-abs(r-(.16+.65*t))*65);float rays=pow(saturate(cos(a*6)),20)*exp(-abs(r-.27-.2*t)*14);'
            'float core=exp(-r*r*90)*(1-t);float fade=pow(1-t,1.5);'
            'return (float3(R,G,B)*(ring*1.6+rays*.8+core*2)+core*.8)*fade*3;',inputs)
    else:
        color=node(mat,unreal.MaterialExpressionVectorParameter)
        color.set_editor_property('parameter_name','Color');color.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
        custom(mat,'float core=pow(saturate(1-abs(UV.x*2-1)),2);return C*(1.6+core*2);',{'UV':(uv,''),'C':(color,'RGB')})
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)

roster=json.loads((Path(unreal.Paths.project_dir())/'ContentSource/Enemies/CyberRoster/roster.json').read_text())
for entry in roster:
    asset_id=entry['id'];mat=unreal.load_asset('/Game/Art/Characters/'+asset_id+'/M_'+asset_id)
    if not mat:raise RuntimeError('Missing enemy PBR material '+asset_id)
    add_hit_flash(mat)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('COMBAT_FEEDBACK_MATERIALS_READY: 3 feedback shaders and 10 PBR hit-flash hooks')
