"""Author persistent sign lettering and readable contract hazard materials."""
import unreal

tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
folder='/Game/Art/Challenges'

def node(m,kind):
    return lib.create_material_expression(m,kind)

def custom(m,code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n=node(m,unreal.MaterialExpressionCustom)
    pins=[]
    for name in inputs:
        p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
    n.set_editor_property('inputs',pins);n.set_editor_property('code',code);n.set_editor_property('output_type',kind)
    for name,(source,output) in inputs.items():
        if not lib.connect_material_expressions(source,output,n,name):raise RuntimeError('Missing pin '+name)
    return n

text=unreal.load_asset(folder+'/M_ChallengeText')
if not text:
    text=unreal.EditorAssetLibrary.duplicate_asset('/Engine/EngineMaterials/DefaultTextMaterialOpaque',folder+'/M_ChallengeText')
if not text:raise RuntimeError('Cannot duplicate the engine font material')
# Keep the engine's font sample/opacity graph, replacing only the emitted color.
text.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
vertex=node(text,unreal.MaterialExpressionVertexColor)
time=node(text,unreal.MaterialExpressionTime)
glow=custom(text,'return C.rgb*(3.2+0.35*sin(T*2.1));',{'C':(vertex,''),'T':(time,'')})
lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(text);unreal.EditorAssetLibrary.save_loaded_asset(text,only_if_is_dirty=False)

for name in ['M_ChallengeRing','M_ChallengeOrb','M_ChallengeShield']:
    m=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True)
    uv=node(m,unreal.MaterialExpressionTextureCoordinate);time=node(m,unreal.MaterialExpressionTime)
    tint=node(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(0,0,0,1) if name.endswith('Ring') else unreal.LinearColor(1,.012,.32,1))
    if name.endswith('Ring'):
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
        charge=node(m,unreal.MaterialExpressionScalarParameter);charge.set_editor_property('parameter_name','Charge');charge.set_editor_property('default_value',0)
        shader=custom(m,'float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);'
            'float edge=exp(-abs(r-.96)*150);float inner=exp(-abs(r-(.15+.75*saturate(Q)))*100);'
            'float ticks=pow(saturate(cos(a*12)),24)*exp(-abs(r-.86)*100);'
            'float crosshair=exp(-min(abs(p.x),abs(p.y))*200)*step(r,.15);'
            'float3 c=dot(C,C)>0.0001 ? C : lerp(float3(.02,.7,1),float3(1,.025,.25),saturate(Q));'
            'return c*(edge*2+inner*.8+ticks+crosshair)*2.5;',{'UV':(uv,''),'T':(time,''),'Q':(charge,''),'C':(tint,'')})
    elif name.endswith('Orb'):
        shader=custom(m,'float v=sin(UV.x*50+sin(UV.y*26+T*3)*3-T*4)*sin(UV.y*35+T*2);'
            'float seams=pow(saturate(1-abs(v)),24);return C.rgb*(1.8+seams*3)+float3(.01,.25,.6)*seams;',{'UV':(uv,''),'T':(time,''),'C':(tint,'')})
    else:
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
        shader=custom(m,'float2 p=UV*float2(30,16);float crack=pow(saturate(1-abs(sin(p.x+sin(p.y)*2)*sin(p.y))),28);'
            'return float3(.08,.45,.8)*(crack*.55+.012)*(1+.1*sin(T*2));',{'UV':(uv,''),'T':(time,'')})
    lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('CHALLENGE_MATERIALS_READY')
