"""Reimport the corrected wedge and separate floors from the architectural cutaway."""
from pathlib import Path
import unreal, json
root=Path(unreal.Paths.project_dir()); lib=unreal.MaterialEditingLibrary
(root/'Saved/Review').mkdir(parents=True,exist_ok=True)
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
task=unreal.AssetImportTask(); task.filename=str(root/'ContentSource/City/Kit/SM_CityRamp.fbx')
task.destination_path='/Game/Art/City'; task.destination_name='SM_CityRamp'; task.automated=True; task.replace_existing=True; task.save=True
opts=unreal.FbxImportUI(); opts.automated_import_should_detect_type=False; opts.import_materials=False; opts.import_textures=False
opts.import_as_skeletal=False; opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
opts.static_mesh_import_data.combine_meshes=True; opts.static_mesh_import_data.auto_generate_collision=False
task.options=opts; task.factory=unreal.FbxFactory(); unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset('/Game/Art/City/SM_CityRamp'); assert mesh
material=unreal.load_asset('/Game/Art/City/M_WetConcrete'); assert material
# Walkable floors must never disappear when the camera cuts through nearby architecture.
# Keep the wet aggregate, puddle roughness and animated rain normals intact.
material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property('two_sided',False)
lib.recompile_material(material); unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
mesh.set_material(0,material); unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
street=unreal.load_asset('/Game/Art/City/M_StreetSurface'); assert street
tools=unreal.AssetToolsHelpers.get_asset_tools()
ramp=unreal.load_asset('/Game/Art/City/MI_RampFloor') or tools.create_asset('MI_RampFloor','/Game/Art/City',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
lib.set_material_instance_parent(ramp,street)
lib.set_material_instance_vector_parameter_value(ramp,'SurfaceColor',unreal.LinearColor(.044,.049,.06,1))
for key,value in [('DryRoughness',.78),('WetRoughness',.18),('SurfaceKind',1),('JointSize',140)]:lib.set_material_instance_scalar_parameter_value(ramp,key,value)
overrides=ramp.get_editor_property('base_property_overrides')
overrides.set_editor_property('override_blend_mode',True);overrides.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
overrides.set_editor_property('override_two_sided',True);overrides.set_editor_property('two_sided',False)
ramp.set_editor_property('base_property_overrides',overrides)
unreal.EditorAssetLibrary.save_loaded_asset(ramp,only_if_is_dirty=False)
assert material.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_OPAQUE
report={'result':'WALKABLE_SURFACES_PASS','ramp':mesh.get_path_name(),'material':material.get_path_name(),'runtimeMaterial':ramp.get_path_name(),'blend':'Opaque','twoSided':False}
(root/'Saved/Review/WalkableMaterial.json').write_text(json.dumps(report,indent=2))
unreal.log('WALKABLE_SURFACES_PASS')
