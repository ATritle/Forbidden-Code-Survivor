"""Import the approved 15-credit Meshy sign; PBR housing has no baked emissive output."""
from pathlib import Path
import json
import unreal

root=Path(unreal.Paths.project_dir())/'ContentSource/City/Meshy/ChallengeSign'
folder='/Game/Art/Challenges/Meshy'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
task=unreal.AssetImportTask();task.filename=str(root/'ChallengeSign-UE.fbx');task.destination_path=folder;task.destination_name='SM_ChallengeSign'
task.automated=True;task.replace_existing=True;task.save=True
opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_materials=False;opts.import_textures=False;opts.import_as_skeletal=False
opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False
task.options=opts;task.factory=unreal.FbxFactory();tools.import_asset_tasks([task])
mesh=unreal.load_asset(folder+'/SM_ChallengeSign')
if not mesh:raise RuntimeError('Missing imported Meshy sign')
mat=unreal.load_asset(folder+'/M_ChallengeSignPBR') or tools.create_asset('M_ChallengeSignPBR',folder,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat);mat.set_editor_property('used_with_nanite',True)
for channel,prop in [('base_color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('metallic',unreal.MaterialProperty.MP_METALLIC),('roughness',unreal.MaterialProperty.MP_ROUGHNESS)]:
    t=unreal.AssetImportTask();t.filename=str(root/'model_textures'/(channel+'.png'));t.destination_path=folder;t.destination_name='T_Sign_'+channel
    t.automated=True;t.replace_existing=True;t.save=True;tools.import_asset_tasks([t]);tex=unreal.load_asset(folder+'/'+t.destination_name)
    if not tex:raise RuntimeError('Missing '+channel)
    tex.set_editor_property('srgb',channel=='base_color');tex.set_editor_property('max_texture_size',2048)
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if channel=='normal' else unreal.TextureCompressionSettings.TC_DEFAULT if channel=='base_color' else unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    n=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample);n.set_editor_property('texture',tex)
    n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='base_color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    lib.connect_material_property(n,'RGB' if channel in ['base_color','normal'] else 'R',prop)
lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
for i in range(len(mesh.get_editor_property('static_materials'))):mesh.set_material(i,mat)
settings=mesh.get_editor_property('nanite_settings');settings.set_editor_property('enabled',True);mesh.set_editor_property('nanite_settings',settings)
unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
bounds=mesh.get_bounding_box()
report={'asset':mesh.get_path_name(),'nanite':True,'source_task':'01a15029-1807-72aa-b8cd-1d0b5da4bc3f','credits':15,'separate_runtime_neon':True,'bounds_min':str(bounds.min),'bounds_max':str(bounds.max)}
(root/'import-report.json').write_text(json.dumps(report,indent=2))
unreal.log('MESHY_CHALLENGE_SIGN_READY '+json.dumps(report))
