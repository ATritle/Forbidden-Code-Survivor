import unreal
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
def inp(name):
    v=unreal.CustomInput();v.set_editor_property('input_name',name);return v
def node(m,c):return lib.create_material_expression(m,c)
def custom(m,code,inputs,output=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n=node(m,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',output);n.set_editor_property('inputs',[inp(k) for k in inputs]);return n
def wire(a,out,b,key):lib.connect_material_expressions(a,out,b,key)
for name in ['FireTail','HoverWake','EssenceOrb','EssenceAura','LandingDust']:
    path='/Game/Art/Spells/M_'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_'+name,'/Game/Art/Spells',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE if name=='EssenceOrb' else unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('used_with_niagara_sprites',True);m.set_editor_property('used_with_niagara_ribbons',True)
    uv=node(m,unreal.MaterialExpressionTextureCoordinate)
    time=node(m,unreal.MaterialExpressionTime)
    if name in ['FireTail','HoverWake']:
        tex=node(m,unreal.MaterialExpressionTextureSample);tex.texture=unreal.load_asset('/Game/Art/Spells/T_Fireball')
        out=custom(m,'float x=saturate(UV.x);float width=max(0.015,(1-x)*0.43);float center=0.5+sin(x*26-Time*19)*0.07*x;float a=saturate(1-abs(UV.y-center)/width);a=a*a*(1-x);float f=0.65+0.35*sin(x*47-Time*24+UV.y*11);return float3(9,2.4,0.16)*a*f*(0.35+Tex.r);',['UV','Time','Tex'])
        if name=='HoverWake':out.set_editor_property('code',out.get_editor_property('code').replace('float3(9,2.4,0.16)','float3(0.03,1.8,3.2)'))
        wire(uv,'',out,'UV');wire(time,'',out,'Time');wire(tex,'RGB',out,'Tex')
    elif name=='LandingDust':
        color=node(m,unreal.MaterialExpressionParticleColor)
        out=custom(m,'float r=length(UV*2-1);return Color.rgb*pow(saturate(1-r),2);',['UV','Color'])
        wire(uv,'',out,'UV');wire(color,'',out,'Color')
    else:
        normal=node(m,unreal.MaterialExpressionPixelNormalWS);view=node(m,unreal.MaterialExpressionCameraVectorWS)
        out=custom(m,('float pulse=1+0.25*sin(Time*4);float facing=saturate(abs(dot(Normal,View)));return float3(0.12,3.8,2.3)*pulse*'+('1.5;' if name=='EssenceOrb' else 'pow(facing,3)*0.14;')),['Time','Normal','View'])
        wire(time,'',out,'Time');wire(normal,'',out,'Normal');wire(view,'',out,'View')
    lib.connect_material_property(out,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if name!='EssenceOrb':
        opacity=node(m,unreal.MaterialExpressionConstant);opacity.set_editor_property('r',1.0);lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('COMBAT_POLISH_MATERIALS_READY')
