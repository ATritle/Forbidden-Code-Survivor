"""Deterministic energy materials; preserves the existing GPU collision simulations."""
import unreal
from pathlib import Path
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
art=Path(unreal.Paths.project_dir())/'ContentSource/UI/NeonEnergy'
for name in ['PulseDart','PrismBurst','HaloNova','KineticCards','PhotonLance','TalismanGuard','School_Pulse','School_Prism','School_Kinetic','School_Barrier']:
    task=unreal.AssetImportTask();task.filename=str(art/(name+'.png'))
    task.destination_path='/Game/UI/Codex';task.destination_name='T_'+name
    task.automated=True;task.replace_existing=True;task.save=True
    tools.import_asset_tasks([task])
    tex=unreal.load_asset('/Game/UI/Codex/T_'+name)
    if not tex:raise RuntimeError('Missing energy card texture '+name)
    tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)

def make(name,code,card=False):
    path='/Game/Art/Spells/'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,'/Game/Art/Spells',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('used_with_niagara_sprites',True)
    m.set_editor_property('used_with_niagara_ribbons',True)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    time=lib.create_material_expression(m,unreal.MaterialExpressionParticleRelativeTime if name=='M_EnergyAura' else unreal.MaterialExpressionTime)
    color=lib.create_material_expression(m,unreal.MaterialExpressionParticleColor)
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('code',code)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[]
    for key in ['UV','T','Color']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
    n.set_editor_property('inputs',pins)
    for src,out,key in [(uv,'','UV'),(time,'','T'),(color,'RGB','Color')]:lib.connect_material_expressions(src,out,n,key)
    lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    one=lib.create_material_expression(m,unreal.MaterialExpressionConstant);one.set_editor_property('r',1.0)
    lib.connect_material_property(one,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)

make('M_Fireball', '''float2 p=UV*2-1;float r=length(p);float a=atan2(p.y,p.x);float core=pow(saturate(1-r),4);float shell=exp(-abs(r-0.55-0.05*sin(a*6-T*12))*35);float halo=pow(saturate(1-r),2)*0.25;float filaments=pow(saturate(sin(a*11+r*24-T*16)),10)*saturate(1-r)*0.4;return Color*(core*3+shell*0.6+halo+filaments);''')
make('M_FireTail', '''float x=saturate(UV.x);float width=max(0.012,(1-x)*0.38);float y=UV.y-0.5-sin(x*30-T*16)*0.04*x;float body=pow(saturate(1-abs(y)/width),3)*(1-x);float streak=pow(saturate(1-abs(y-sin(x*49-T*24)*0.035)/max(0.01,width*0.12)),3)*(1-x);return lerp(float3(4,0.015,1.4),float3(0.15,0.8,3),x*0.5)*(body*0.7+streak);''')
make('M_ChargedCard', '''float2 p=abs(UV-0.5);float edge=1-smoothstep(0.012,0.035,min(0.48-p.x,0.48-p.y));float diamond=exp(-abs(p.x+p.y*0.7-0.23)*110);float circuit=pow(saturate(cos(UV.y*65+UV.x*17)),35)*step(p.x,0.32)*0.2;return float3(4,0.02,1.8)*(edge+diamond+circuit)+float3(0.01,0.03,0.055);''',True)
make('M_EnergyAura', '''float r=length(UV*2-1);float ring=exp(-abs(r-(0.08+T*0.85))*55);float inner=exp(-abs(r-(0.03+T*0.65))*70)*0.4;float haze=pow(saturate(1-r),3)*0.3;return float3(3,0.015,1.5)*(ring+inner+haze)*pow(1-saturate(T),2);''')
# The fluid simulation still supplies density/temperature; replace its orange LUT separately.
unreal.log('ENERGY_MATERIALS_READY')
