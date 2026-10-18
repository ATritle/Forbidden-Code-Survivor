"""Reusable enemy feedback shader hook; preserves imported PBR and emissive art."""
import unreal

def add_hit_flash(material, force=False):
    lib=unreal.MaterialEditingLibrary
    version=unreal.EditorAssetLibrary.get_metadata_tag(material,'CombatFeedbackVersion')
    code='return Glow+(Flash*1.2+.08)*saturate(Strength);'
    if not force and version=='2':
        return
    if not force and version=='1':
        shader=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if not isinstance(shader,unreal.MaterialExpressionCustom):
            raise RuntimeError('Hit-flash material changed unexpectedly: '+material.get_name())
        shader.set_editor_property('code',code)
        unreal.EditorAssetLibrary.set_metadata_tag(material,'CombatFeedbackVersion','2')
        return
    previous=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output=lib.get_material_property_input_node_output_name(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    flash=lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter)
    flash.set_editor_property('parameter_name','CombatHitFlash')
    flash.set_editor_property('default_value',unreal.LinearColor(0,0,0,0))
    flash.set_editor_property('use_custom_primitive_data',True)
    flash.set_editor_property('primitive_data_index',0)
    shader=lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    pins=[]
    for name in ['Glow','Flash','Strength']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    shader.set_editor_property('inputs',pins);shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    shader.set_editor_property('code',code)
    if previous:
        lib.connect_material_expressions(previous,output,shader,'Glow')
    else:
        zero=lib.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
        lib.connect_material_expressions(zero,'',shader,'Glow')
    lib.connect_material_expressions(flash,'RGB',shader,'Flash')
    lib.connect_material_expressions(flash,'A',shader,'Strength')
    lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.EditorAssetLibrary.set_metadata_tag(material,'CombatFeedbackVersion','2')

