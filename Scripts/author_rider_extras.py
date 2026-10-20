"""Small procedural VFX materials for post-rig rider accessories. No paid assets."""
import unreal

tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
folder='/Game/Art/Characters/RiderExtras'
for name,color,ghost in [('M_RiderGhost',unreal.LinearColor(.8,.05,1,1),True),('M_RiderShield',unreal.LinearColor(1,.45,.015,1),False)]:
    m=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('used_with_skeletal_mesh',ghost)
    tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',color)
    fresnel=lib.create_material_expression(m,unreal.MaterialExpressionFresnel)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    node=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    inputs=[]
    for label in ['UV','C','F']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',label);inputs.append(pin)
    node.set_editor_property('inputs',inputs)
    node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('code','float2 p=UV*float2(24,32); float gridEdge=1-smoothstep(.025,.065,min(abs(frac(p.x)-.5),abs(frac(p.y)-.5))); return C*(.35+F*2.0+gridEdge*'+('1.4' if ghost else '.55')+');')
    lib.connect_material_expressions(uv,'',node,'UV');lib.connect_material_expressions(tint,'RGB',node,'C');lib.connect_material_expressions(fresnel,'',node,'F')
    lib.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    inp=unreal.CustomInput();inp.set_editor_property('input_name','F');opacity.set_editor_property('inputs',[inp])
    opacity.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    opacity.set_editor_property('code','return '+('.12+F*.5;' if ghost else '.025+F*.2;'))
    lib.connect_material_expressions(fresnel,'',opacity,'F');lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('RIDER_ACCESSORY_MATERIALS_READY')
