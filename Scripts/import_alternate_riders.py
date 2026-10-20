"""Import approved Meshy sources. Re-running updates only /Game/Art assets."""
import os
import unreal

ROOT = os.path.join(unreal.Paths.project_dir(), 'ContentSource', 'Meshy')
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')

def import_file(filename, folder, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
        task.factory = unreal.FbxFactory()
    TOOLS.import_asset_tasks([task])
    assets = [unreal.load_asset(p) for p in task.imported_object_paths]
    return assets

def fbx_options(skeletal=False, skeleton=None):
    o = unreal.FbxImportUI()
    o.automated_import_should_detect_type = False
    o.import_materials = False
    o.import_textures = False
    o.import_as_skeletal = skeletal
    o.import_mesh = skeleton is None
    o.import_animations = skeleton is not None
    o.create_physics_asset = False
    if skeleton:
        o.skeleton = skeleton
        o.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
        o.anim_sequence_import_data.set_editor_property('import_translation', unreal.Vector(0,0,0))
    elif skeletal:
        o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        o.skeletal_mesh_import_data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    else:
        o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        o.static_mesh_import_data.combine_meshes = True
        o.static_mesh_import_data.auto_generate_collision = False
    return o

def material(asset_id, folder, skeletal):
    path = folder + '/M_' + asset_id
    mat = unreal.load_asset(path) or TOOLS.create_asset('M_'+asset_id, folder, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_skeletal_mesh', skeletal)
    mat.set_editor_property('used_with_instanced_static_meshes', not skeletal)
    props = [('base_color', unreal.MaterialProperty.MP_BASE_COLOR), ('normal', unreal.MaterialProperty.MP_NORMAL),
             ('roughness', unreal.MaterialProperty.MP_ROUGHNESS), ('metallic', unreal.MaterialProperty.MP_METALLIC)]
    for i, (channel, prop) in enumerate(props):
        textures = import_file(os.path.join(ROOT,CURRENT_ID,'board_textures' if asset_id.endswith('Board') else 'model_textures',channel+'.png'),folder,'T_'+asset_id+'_'+channel)
        tex = next(a for a in textures if isinstance(a, unreal.Texture2D))
        tex.set_editor_property('srgb', channel == 'base_color')
        tex.set_editor_property('max_texture_size', 2048 if asset_id.endswith('Board') else 4096)
        if channel == 'normal':
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif channel != 'base_color':
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        node = unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-400,i*220)
        node.texture = tex
        node.sampler_type = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel == 'normal' else
                             unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel == 'base_color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        unreal.MaterialEditingLibrary.connect_material_property(node,'RGB' if channel in ('base_color','normal') else 'R',prop)
    color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
    color.texture=unreal.load_asset(folder+'/T_'+asset_id+'_base_color')
    glow=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionCustom)
    pin=unreal.CustomInput();pin.set_editor_property('input_name','C')
    glow.set_editor_property('inputs',[pin]);glow.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    glow.set_editor_property('code','float hi=max(C.r,max(C.g,C.b));float lo=min(C.r,min(C.g,C.b));float sat=(hi-lo)/max(hi,0.001);return C*smoothstep(0.55,0.85,sat)*smoothstep(0.5,0.9,hi)*2.5;')
    unreal.MaterialEditingLibrary.connect_material_expressions(color,'RGB',glow,'C')
    unreal.MaterialEditingLibrary.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat



STATES=['HoverIdle','Cruise','Accelerate','Brake','TurnLeft','TurnRight','Cast','Hit','Fall','Land','Defeat','CastBlood','CastStorm','HoverIdleAlt']
for CURRENT_ID in ['Vex','Rook','Zero','Mira']:
    folder='/Game/Art/Characters/'+CURRENT_ID
    source=os.path.join(ROOT,CURRENT_ID,'assembled')
    assets=import_file(os.path.join(source,'SK_'+CURRENT_ID+'.fbx'),folder,'SK_'+CURRENT_ID,fbx_options(True))
    mesh=next(a for a in assets if isinstance(a,unreal.SkeletalMesh))
    if mesh.get_name()!='SK_'+CURRENT_ID:
        unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),folder+'/SK_'+CURRENT_ID)
    rider_mat=material(CURRENT_ID,folder,True)
    board_mat=material(CURRENT_ID+'Board',folder,True)
    skeleton=mesh.get_editor_property('skeleton')
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton,only_if_is_dirty=False)
    for state in STATES:
        imported=import_file(os.path.join(source,'A_'+state+'.fbx'),folder,'A_'+state,fbx_options(True,skeleton))
        anim=next(a for a in imported if isinstance(a,unreal.AnimSequence))
        if anim.get_name()!='A_'+state:
            unreal.EditorAssetLibrary.rename_asset(anim.get_path_name(),folder+'/A_'+state)
        anim.set_editor_property('enable_root_motion',False)
        anim.set_editor_property('force_root_lock',True)
        anim.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        unreal.EditorAssetLibrary.save_loaded_asset(anim,only_if_is_dirty=False)
        unreal.log('RIDER_ANIM_IMPORTED '+CURRENT_ID+' '+state+' '+str(anim.get_play_length()))
    slots=list(mesh.get_editor_property('materials'))
    for index,slot in enumerate(slots):
        label=str(slot.get_editor_property('imported_material_slot_name'))
        slot.set_editor_property('material_interface',board_mat if 'Board' in label else rider_mat)
        slots[index]=slot
    mesh.set_editor_property('materials',slots)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(folder,only_if_is_dirty=True,recursive=True)
    unreal.log('ALTERNATE_RIDER_IMPORTED '+CURRENT_ID)
unreal.log('FOUR_RIDERS_IMPORTED')
