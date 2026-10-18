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
        textures = import_file(os.path.join(ROOT,asset_id,'model_textures',channel+'.png'),folder,'T_'+asset_id+'_'+channel)
        tex = next(a for a in textures if isinstance(a, unreal.Texture2D))
        tex.set_editor_property('srgb', channel == 'base_color')
        tex.set_editor_property('max_texture_size', 1024)
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
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

for asset_id in ['CryptStalker','BoneWarden','PlagueBrute']:
    skeletal = True
    source = os.path.join(ROOT,asset_id,'rig.fbx' if skeletal else 'model.fbx')
    if not os.path.exists(source):
        unreal.log_warning('Waiting for '+source)
        continue
    folder = '/Game/Art/'+('Characters/' if skeletal else 'Props/')+asset_id
    name = ('SK_' if skeletal else 'SM_')+asset_id
    assets = import_file(source,folder,name,fbx_options(skeletal))
    cls = unreal.SkeletalMesh if skeletal else unreal.StaticMesh
    mesh = next(a for a in assets if isinstance(a,cls))
    if mesh.get_name() != name:
        unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),folder+'/'+name)
    mat = material(asset_id,folder,skeletal)
    if skeletal:
        slots = mesh.get_editor_property('materials')
        for index, slot in enumerate(slots):
            slot.material_interface = mat
            slots[index] = slot
        mesh.set_editor_property('materials',slots)
        skeleton = mesh.get_editor_property('skeleton')
        for action in ['Walk','Run']:
            animations = import_file(os.path.join(ROOT,asset_id,action.lower()+'.fbx'),folder,'A_'+action,fbx_options(True,skeleton))
            anim = next(a for a in animations if isinstance(a,unreal.AnimSequence))
            if anim.get_name() != 'A_'+action:
                unreal.EditorAssetLibrary.rename_asset(anim.get_path_name(),folder+'/A_'+action)
            anim.set_editor_property('enable_root_motion',False)
            anim.set_editor_property('force_root_lock',True)
            anim.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
            unreal.EditorAssetLibrary.save_loaded_asset(anim, only_if_is_dirty=False)
        # Animation imports can rebuild the skeletal mesh's imported material slots.
        slots = list(mesh.get_editor_property('materials'))
        for index in range(len(slots)):
            slot = slots[index]
            slot.set_editor_property('material_interface', mat)
            slots[index] = slot
        mesh.set_editor_property('materials', slots)
        unreal.log('ART_MATERIAL_ASSIGNED '+str(mesh.get_editor_property('materials')))
    else:
        for i in range(mesh.get_num_sections(0)):
            mesh.set_material(i,mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    unreal.log('MESHY_IMPORTED '+asset_id)
unreal.EditorAssetLibrary.save_directory('/Game/Art',only_if_is_dirty=True,recursive=True)
unreal.log('MESHY_IMPORT_COMPLETE')
