"""Import the approved ten-enemy roster, matching rigs, PBR maps and three skeletal LODs."""
import json
from pathlib import Path
import unreal
import sys
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Scripts'))
from combat_feedback_materials import add_hit_flash
root=Path(unreal.Paths.project_dir())
roster=json.loads((root/'ContentSource/Enemies/CyberRoster/roster.json').read_text())
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')

def import_asset(file,folder,name,options=None):
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True
    if options:task.options=options;task.factory=unreal.FbxFactory()
    tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.imported_object_paths]

def options(skeleton=None):
    o=unreal.FbxImportUI();o.automated_import_should_detect_type=False
    o.import_materials=False;o.import_textures=False;o.import_as_skeletal=True;o.create_physics_asset=False
    o.import_mesh=skeleton is None;o.import_animations=skeleton is not None
    o.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if skeleton else unreal.FBXImportType.FBXIT_SKELETAL_MESH
    if skeleton:o.skeleton=skeleton
    return o

report=[]
for entry in roster:
    asset_id=entry['id'];folder='/Game/Art/Characters/'+asset_id;source=root/'ContentSource/Meshy'/asset_id
    if not all((source/file).exists() for file in ['rig.fbx','walk.fbx','run.fbx']):raise RuntimeError('Missing approved source '+asset_id)
    mesh=next(a for a in import_asset(source/'rig.fbx',folder,'SK_'+asset_id,options()) if isinstance(a,unreal.SkeletalMesh))
    if mesh.get_name()!='SK_'+asset_id:unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),folder+'/SK_'+asset_id)
    skeleton=mesh.get_editor_property('skeleton')
    if not skeleton:raise RuntimeError('FBX import produced no skeleton '+asset_id)
    # AssetImportTask saves its primary mesh; the generated skeleton is a separate package.
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton,only_if_is_dirty=False)
    material=unreal.load_asset(folder+'/M_'+asset_id) or tools.create_asset('M_'+asset_id,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material);material.set_editor_property('used_with_skeletal_mesh',True)
    base=None
    for index,(channel,prop) in enumerate([('base_color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('metallic',unreal.MaterialProperty.MP_METALLIC)]):
        tex=next(a for a in import_asset(source/'model_textures'/(channel+'.png'),folder,'T_'+asset_id+'_'+channel) if isinstance(a,unreal.Texture2D))
        tex.set_editor_property('srgb',channel=='base_color');tex.set_editor_property('max_texture_size',2048 if entry['tier'] in ['ELITE','BOSS'] else 1024)
        if channel=='normal':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif channel!='base_color':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(tex,only_if_is_dirty=False)
        node=lib.create_material_expression(material,unreal.MaterialExpressionTextureSample,-500,index*180);node.texture=tex
        node.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='base_color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        lib.connect_material_property(node,'RGB' if channel in ['base_color','normal'] else 'R',prop)
        if channel=='base_color':base=node
    glow=lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    pin=unreal.CustomInput();pin.set_editor_property('input_name','C');glow.set_editor_property('inputs',[pin]);glow.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    glow.set_editor_property('code','float hi=max(C.r,max(C.g,C.b));float lo=min(C.r,min(C.g,C.b));float mask=smoothstep(.42,.8,hi)*smoothstep(.18,.45,hi-lo);return C*mask*1.8;')
    lib.connect_material_expressions(base,'RGB',glow,'C');lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    add_hit_flash(material,force=True)
    lib.recompile_material(material);unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
    skeleton=mesh.get_editor_property('skeleton')
    durations={}
    for action in ['Walk','Run']:
        clip=next(a for a in import_asset(source/(action.lower()+'.fbx'),folder,'A_'+action,options(skeleton)) if isinstance(a,unreal.AnimSequence))
        if clip.get_name()!='A_'+action:unreal.EditorAssetLibrary.rename_asset(clip.get_path_name(),folder+'/A_'+action)
        clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True)
        clip.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        if clip.get_editor_property('skeleton')!=skeleton or clip.get_play_length()<=0:raise RuntimeError('Invalid matching animation '+asset_id)
        durations[action]=clip.get_play_length();unreal.EditorAssetLibrary.save_loaded_asset(clip,only_if_is_dirty=False)
    slots=list(mesh.get_editor_property('materials'))
    for i in range(len(slots)):slot=slots[i];slot.material_interface=material;slots[i]=slot
    mesh.set_editor_property('materials',slots)
    if not unreal.SkeletalMeshEditorSubsystem.regenerate_lod(mesh,3,False,False):raise RuntimeError('LOD generation failed '+asset_id)
    lods=unreal.SkeletalMeshEditorSubsystem.get_lod_count(mesh)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(folder,only_if_is_dirty=False,recursive=True)
    report.append({'id':asset_id,'height_cm':entry['height'],'health':entry['hp'],'lods':lods,'animations':durations})
    unreal.log('CYBER_ENEMY_IMPORTED '+asset_id+' LODs='+str(lods))
(root/'Saved/CyberRosterImport.json').write_text(json.dumps(report,indent=2))
unreal.log('CYBER_ROSTER_IMPORTED: ten textured rigs, twenty matching clips, three LODs each')
