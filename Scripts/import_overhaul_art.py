"""Import the approved 80-credit batch without replacing the baseline source inputs."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
source=root/'ContentSource/WindowsOverhaul'
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
report=[]

def import_file(path,folder,name,options=None):
    t=unreal.AssetImportTask();t.filename=str(path);t.destination_path=folder;t.destination_name=name
    t.automated=True;t.replace_existing=True;t.save=True
    if options:t.options=options;t.factory=unreal.FbxFactory()
    tools.import_asset_tasks([t])
    assets=[unreal.load_asset(p) for p in t.imported_object_paths]
    return [a for a in assets if a]

def options(skeletal=False,skeleton=None):
    o=unreal.FbxImportUI();o.automated_import_should_detect_type=False;o.import_materials=False;o.import_textures=False
    o.import_as_skeletal=skeletal;o.import_mesh=skeleton is None;o.import_animations=skeleton is not None;o.create_physics_asset=False
    o.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if skeleton else unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH
    if skeleton:o.skeleton=skeleton
    if not skeletal:o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False
    return o

def material(name,folder,spirit=False):
    m=unreal.load_asset(folder+'/M_'+name) or tools.create_asset('M_'+name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('used_with_skeletal_mesh',spirit);m.set_editor_property('used_with_instanced_static_meshes',not spirit)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    nodes={}
    for channel,prop in [('base_color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('metallic',unreal.MaterialProperty.MP_METALLIC),('roughness',unreal.MaterialProperty.MP_ROUGHNESS)]:
        texture=next(a for a in import_file(source/name/'model_textures'/(channel+'.png'),folder,'T_'+name+'_'+channel) if isinstance(a,unreal.Texture2D))
        texture.set_editor_property('srgb',channel=='base_color');texture.set_editor_property('max_texture_size',2048)
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if channel=='normal' else unreal.TextureCompressionSettings.TC_DEFAULT if channel=='base_color' else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
        n=lib.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=texture
        n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='base_color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        lib.connect_material_property(n,'RGB' if channel in ['base_color','normal'] else 'R',prop);nodes[channel]=n
    def custom(code,names,kind):
        n=lib.create_material_expression(m,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',kind)
        pins=[]
        for name in names:p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
        n.set_editor_property('inputs',pins);return n
    time=lib.create_material_expression(m,unreal.MaterialExpressionTime)
    emission=custom('float hi=max(C.r,max(C.g,C.b));float lo=min(C.r,min(C.g,C.b));float mask=smoothstep(.5,.82,hi)*smoothstep(.15,.42,hi-lo);return C*mask*1.8*(.94+.06*sin(T*1.7))'+('+float3(.02,.28,.5)' if spirit else '')+';',['C','T'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    lib.connect_material_expressions(nodes['base_color'],'RGB',emission,'C');lib.connect_material_expressions(time,'',emission,'T');lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    world=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    if spirit:
        mask=custom('float lines=step(.07,frac(W.z*.035+T*.1));return lines;',['W','T'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        lib.connect_material_expressions(world,'',mask,'W');lib.connect_material_expressions(time,'',mask,'T')
    else:
        collection=unreal.load_asset('/Game/Art/City/MPC_City')
        hero=lib.create_material_expression(m,unreal.MaterialExpressionCollectionParameter);hero.set_editor_property('collection',collection);hero.set_editor_property('parameter_name','PlayerPosition')
        cam=lib.create_material_expression(m,unreal.MaterialExpressionCollectionParameter);cam.set_editor_property('collection',collection);cam.set_editor_property('parameter_name','CameraPosition')
        mask=custom('float3 d=C.xyz-P.xyz;float u=dot(W-P.xyz,d)/max(1,dot(d,d));float r=length(W-(P.xyz+d*saturate(u)));float f=(1-smoothstep(240,420,r))*step(.04,u)*step(u,.98)*step(P.z+35,W.z)*P.w;float h=frac(sin(dot(floor(W*1.3),float3(12.989,78.233,37.719)))*43758.5453);return step(h,1-f*.98);',['W','P','C'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        for n,key in [(world,'W'),(hero,'P'),(cam,'C')]:lib.connect_material_expressions(n,'',mask,key)
    lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False);return m

for name in ['Shopfront','ServiceCabinet','DigitalSpirit']:
    skeletal=name=='DigitalSpirit';folder='/Game/Art/Characters/DigitalSpirit' if skeletal else '/Game/Art/City/Overhaul/'+name
    prefix='SK_' if skeletal else 'SM_'
    mesh=next(a for a in import_file(source/name/('rig.fbx' if skeletal else 'model.fbx'),folder,prefix+name,options(skeletal)) if isinstance(a,unreal.SkeletalMesh if skeletal else unreal.StaticMesh))
    if mesh.get_name()!=prefix+name:unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),folder+'/'+prefix+name)
    mat=material(name,folder,skeletal)
    slots=list(mesh.get_editor_property('materials' if skeletal else 'static_materials'))
    for i in range(len(slots)):
        if skeletal:slot=slots[i];slot.material_interface=mat;slots[i]=slot
        else:mesh.set_material(i,mat)
    if skeletal:
        mesh.set_editor_property('materials',slots);skeleton=mesh.get_editor_property('skeleton')
        unreal.EditorAssetLibrary.save_loaded_asset(skeleton,only_if_is_dirty=False)
        unreal.SkeletalMeshEditorSubsystem.regenerate_lod(mesh,3,False,False)
        for action in ['Walk','Run']:
            clip=next(a for a in import_file(source/name/(action.lower()+'.fbx'),folder,'A_'+action,options(True,skeleton)) if isinstance(a,unreal.AnimSequence))
            clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True)
            unreal.EditorAssetLibrary.save_loaded_asset(clip,only_if_is_dirty=False)
    else:
        settings=mesh.get_editor_property('nanite_settings');settings.set_editor_property('enabled',True);mesh.set_editor_property('nanite_settings',settings)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
    report.append({'asset':mesh.get_path_name(),'material':mat.get_path_name(),'skeletal':skeletal})

ledger=json.loads((root/'Docs/WindowsOverhaul/meshy-ledger.json').read_text(encoding='utf-8-sig'))
for entry in ledger['animations']:
    name=entry['id'];folder='/Game/Art/Characters/'+name
    mesh=unreal.load_asset(folder+'/SK_'+name)
    if not mesh:raise RuntimeError('Existing roster mesh missing: '+name)
    skeleton=mesh.get_editor_property('skeleton')
    clip=next(a for a in import_file(source/'Enemies'/name/'attack.fbx',folder,'A_Attack',options(True,skeleton)) if isinstance(a,unreal.AnimSequence))
    if clip.get_name()!='A_Attack':unreal.EditorAssetLibrary.rename_asset(clip.get_path_name(),folder+'/A_Attack')
    clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True)
    if clip.get_editor_property('skeleton')!=skeleton or clip.get_play_length()<=0:raise RuntimeError('Combat rig mismatch: '+name)
    unreal.EditorAssetLibrary.save_loaded_asset(clip,only_if_is_dirty=False)
    report.append({'enemy':name,'attack':clip.get_path_name(),'duration':clip.get_play_length()})
(root/'Saved/Review/OverhaulArtImport.json').write_text(json.dumps(report,indent=2))
unreal.log('OVERHAUL_ART_IMPORT_PASS: 3 models, spirit locomotion, 10 matching enemy combat clips')
