"""Sharp procedural hard-light graphics for the gold Kinetic Cards Niagara kit."""
import unreal
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()

def material(name, code):
    folder='/Game/Art/Spells'
    m=unreal.load_asset(folder+'/'+name)
    if not m:
        m=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('used_with_niagara_sprites',True)
    m.set_editor_property('used_with_niagara_ribbons',True)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    time=lib.create_material_expression(m,unreal.MaterialExpressionTime)
    age=lib.create_material_expression(m,unreal.MaterialExpressionParticleRelativeTime)
    color=lib.create_material_expression(m,unreal.MaterialExpressionParticleColor)
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('code',code)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[]
    for key in ['UV','T','Age','Alpha']:
        p=unreal.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
    n.set_editor_property('inputs',pins)
    for src,out,key in [(uv,'','UV'),(time,'','T'),(age,'','Age'),(color,'A','Alpha')]:
        lib.connect_material_expressions(src,out,n,key)
    lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    one=lib.create_material_expression(m,unreal.MaterialExpressionConstant);one.set_editor_property('r',1.0)
    lib.connect_material_property(one,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)

material('M_KineticCard', '''
float2 q=abs(UV-0.5);float inside=1-smoothstep(0.46,0.49,max(q.x,q.y));
float edge=exp(-abs(max(q.x,q.y)-0.455)*170)*inside;
float diamond=exp(-abs(q.x+q.y*0.65-0.22)*140)*inside;
float chevron=exp(-abs(abs(UV.x-0.5)-abs(UV.y-0.5)*0.55-0.08)*160)*step(q.y,0.3);
float ticks=pow(saturate(cos(UV.y*112)),30)*step(0.35,q.x)*step(q.y,0.4);
float scan=exp(-abs(frac(UV.y-T*1.8)-0.5)*40)*inside;
return float3(3.6,1.65,0.035)*(edge+diamond*0.65+ticks*0.55+chevron*0.3)+float3(0.35,0.65,0.04)*scan*0.18+float3(0.025,0.045,0.01)*inside;
''')
material('M_KineticTrail', '''
float x=saturate(UV.x);float2 p=float2(frac(x*7-T*1.3)-0.5,UV.y-0.5);
float frame=exp(-abs(max(abs(p.x)*0.75,abs(p.y))-0.22)*130);
float diamond=exp(-abs(abs(p.x)*0.55+abs(p.y)-0.18)*100)*0.35;
float streak=exp(-abs(UV.y-0.5)*150)*0.22;
return float3(3.2,1.45,0.035)*(frame+diamond+streak)*pow(1-x,1.5)*saturate(Alpha);
''')
material('M_KineticImpact', '''
float2 p=abs(UV*2-1);float diamond=exp(-abs(p.x+p.y-0.6)*38);
float shard=pow(saturate(1-p.y*1.4-p.x*0.55),5);
float cross=exp(-min(p.x,p.y)*65)*pow(saturate(1-max(p.x,p.y)),3);
return float3(3.8,1.8,0.05)*(diamond*0.5+shard+cross*0.3)*pow(1-saturate(Age),1.6)*saturate(Alpha);
''')
unreal.log('KINETIC_MATERIALS_READY: electric-gold etched cards, segmented echoes, diamond impact shards')
