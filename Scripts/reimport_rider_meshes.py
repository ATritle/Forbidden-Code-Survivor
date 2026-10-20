"""Reimport repaired bind meshes without replacing animation or PBR assets."""
import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
for rid in ['Vex','Rook','Zero','Mira']:
 folder='/Game/Art/Characters/'+rid
 old=unreal.load_asset(folder+'/SK_'+rid)
 mats=list(old.get_editor_property('materials'))
 options=unreal.FbxImportUI();options.automated_import_should_detect_type=False
 options.import_as_skeletal=True;options.import_mesh=True;options.import_animations=False
 options.import_materials=False;options.import_textures=False;options.create_physics_asset=False
 options.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
 options.skeleton=old.get_editor_property('skeleton')
 options.skeletal_mesh_import_data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
 task=unreal.AssetImportTask();task.filename=str(root/'ContentSource/Meshy'/rid/'assembled'/('SK_'+rid+'.fbx'))
 task.destination_path=folder;task.destination_name='SK_'+rid;task.automated=True;task.replace_existing=True;task.save=True;task.options=options;task.factory=unreal.FbxFactory()
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 mesh=unreal.load_asset(folder+'/SK_'+rid);mesh.set_editor_property('materials',mats)
 unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
 unreal.log('RIDER_REPAIRED_MESH_IMPORTED '+rid)
