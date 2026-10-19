"""Apply per-player board/wake color without duplicating textures or recompiling at runtime."""
import unreal
lib=unreal.MaterialEditingLibrary
for path in ['/Game/Art/Characters/NeonExorcist/M_NeonHoverboard','/Game/Art/Spells/M_HoverWake']:
    m=unreal.load_asset(path)
    if not m:raise RuntimeError('Missing protocol material '+path)
    existing=[n for n in unreal.ObjectIterator(unreal.MaterialExpressionVectorParameter) if n.get_outer()==m and str(n.get_editor_property('parameter_name'))=='ProtocolTint']
    if not existing:
        source=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
        tint.set_editor_property('parameter_name','ProtocolTint');tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
        tint.set_editor_property('use_custom_primitive_data',True);tint.set_editor_property('primitive_data_index',4)
        multiply=lib.create_material_expression(m,unreal.MaterialExpressionMultiply)
        lib.connect_material_expressions(source,'',multiply,'A');lib.connect_material_expressions(tint,'RGB',multiply,'B')
        lib.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('OVERHAUL_PROTOCOL_MATERIALS_PASS')
