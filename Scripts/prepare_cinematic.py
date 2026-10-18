"""Create a lit test floor; the production Meshy/SSS/VT graphs are specified in Docs/CinematicFramework.md."""
import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools()
path='/Game/Cinematic/M_StoneTest'
mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_StoneTest','/Game/Cinematic',unreal.Material,unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
mat.set_editor_property('used_with_instanced_static_meshes',True)
for name,value,prop in [('Roughness',0.72,unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',0.05,unreal.MaterialProperty.MP_METALLIC)]:
    node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-300,150)
    node.set_editor_property('parameter_name',name)
    node.set_editor_property('default_value',value)
    unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-300,0)
color.set_editor_property('parameter_name','BaseTint')
color.set_editor_property('default_value',unreal.LinearColor(0.07,0.09,0.1,1))
unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('CINEMATIC_TEST_CONTENT_READY')
