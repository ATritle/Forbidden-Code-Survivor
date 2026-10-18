"""Shared palette, separate shapes/motion for the six neon spell identities."""
import unreal
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()

def make(name,code,color):
    m=unreal.load_asset('/Game/Art/Spells/'+name)
    if not m:m=tools.create_asset(name,'/Game/Art/Spells',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('used_with_niagara_sprites',True);m.set_editor_property('used_with_niagara_ribbons',True)
    nodes=[lib.create_material_expression(m,t) for t in [unreal.MaterialExpressionTextureCoordinate,unreal.MaterialExpressionTime,unreal.MaterialExpressionParticleRelativeTime,unreal.MaterialExpressionParticleColor]]
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('code',code+f'\nreturn float3({color[0]},{color[1]},{color[2]})*power;')
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[]
    for key in ['UV','T','Age','Alpha']:
        p=unreal.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
    n.set_editor_property('inputs',pins)
    for src,out,key in zip(nodes,['','','','A'],['UV','T','Age','Alpha']):lib.connect_material_expressions(src,out,n,key)
    lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    one=lib.create_material_expression(m,unreal.MaterialExpressionConstant);one.set_editor_property('r',1.0)
    lib.connect_material_property(one,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)

mint=(0.18,3.5,1.1);violet=(2.0,0.3,4.2);cyan=(0.08,2.6,4.0);teal=(0.06,3.5,2.5)
make('M_PulseDart','''float2 p=UV*2-1;float shaft=exp(-abs(p.y)*23)*pow(saturate(1-abs(p.x)),0.5);float tip=exp(-abs(abs(p.y)-(0.55-p.x)*0.5)*40)*step(0,p.x);float power=(shaft+tip*0.7)*(0.85+0.15*sin(T*35));''',mint)
make('M_HaloShard','''float2 p=UV*2-1;float d=abs(p.x)+abs(p.y);float shell=exp(-abs(d-0.65)*45);float core=pow(saturate(1-d),4);float facets=exp(-min(abs(p.x-p.y),abs(p.x+p.y))*40)*saturate(1-d);float power=shell+core*0.65+facets*0.5;''',violet)
make('M_PhotonLance','''float2 p=UV*2-1;float core=exp(-abs(p.y)*35)*pow(saturate(1-abs(p.x)),0.35);float rings=pow(saturate(cos(p.x*24-T*28)),14)*exp(-abs(abs(p.y)-0.28)*38);float power=core+rings*0.5;''',cyan)
make('M_PulseTrail','''float x=UV.x;float y=abs(UV.y-0.5);float rails=exp(-abs(y-0.16)*70);float dashes=0.4+0.6*pow(saturate(cos(x*65-T*30)),10);float power=rails*dashes*pow(1-saturate(x),2)*saturate(Alpha);''',mint)
make('M_HaloTrail','''float x=UV.x;float2 p=float2(frac(x*4-T*0.8)-0.5,UV.y-0.5);float hex=abs(p.x)*0.8+abs(p.y);float power=exp(-abs(hex-0.27)*75)*pow(1-saturate(x),1.8)*saturate(Alpha);''',violet)
make('M_PhotonTrail','''float x=UV.x;float y=abs(UV.y-0.5);float power=(exp(-y*100)+exp(-abs(y-0.24)*80)*(0.2+0.15*sin(x*90-T*30)))*pow(1-saturate(x),1.7)*saturate(Alpha);''',cyan)
make('M_PulseImpact','''float2 p=UV*2-1;float r=length(p);float ring=exp(-abs(r-(0.2+Age*0.65))*45);float cross=exp(-min(abs(p.x),abs(p.y))*80)*pow(saturate(1-r),2);float power=(ring+cross*0.45)*pow(1-saturate(Age),2)*saturate(Alpha);''',mint)
make('M_HaloImpact','''float2 p=UV*2-1;float d=abs(p.x)+abs(p.y);float power=(exp(-abs(d-0.6)*42)+pow(saturate(1-d),4)*0.7)*pow(1-saturate(Age),1.5)*saturate(Alpha);''',violet)
make('M_PhotonImpact','''float2 p=UV*2-1;float r=length(p);float power=(exp(-abs(r-(0.15+Age*0.7))*55)+exp(-abs(r-(0.05+Age*0.45))*65)*0.4)*pow(1-saturate(Age),1.5)*saturate(Alpha);''',cyan)
make('M_BarrierImpact','''float2 p=abs(UV*2-1);float d=max(p.x*0.866+p.y*0.5,p.y);float frame=exp(-abs(d-(0.32+Age*0.4))*50);float power=(frame+exp(-abs(p.x+p.y-0.45)*45)*0.25)*pow(1-saturate(Age),2)*saturate(Alpha);''',teal)
unreal.log('SPELL_IDENTITIES_MATERIALS_READY: mint darts, violet shards, cyan lances, teal ward impacts')
