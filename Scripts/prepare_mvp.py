"""Run once in Unreal Editor's Python commandlet; creates only MVP-owned content."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
path = "/Game/MVP/M_Prototype"
material = unreal.load_asset(path)
if not material:
    material = tools.create_asset("M_Prototype", "/Game/MVP", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -250, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.3, 0.8, 0.6, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

# The arena floor/grid are instanced; save that shader usage explicitly.
material.set_editor_property("used_with_instanced_static_meshes", True)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

# A saved blank map with native GameMode builds the arena at run time.
map_path = "/Game/MVP/Arena"
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor.new_level(map_path)
    editor.save_current_level()
unreal.log("MVP_CONTENT_READY")
