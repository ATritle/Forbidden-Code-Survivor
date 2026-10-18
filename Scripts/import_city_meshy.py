"""Import the approved 45-credit city batch with PBR and bounded neon emission."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())/'ContentSource/City/Meshy'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
report=[]
for name in ['ShoppingBlock','MarketKiosk','RooftopShrine']:
    folder='/Game/Art/City/Meshy/'+name
    task=unreal.AssetImportTask();task.filename=str(root/name/'model.fbx');task.destination_path=folder;task.destination_name='SM_'+name
    task.automated=True;task.replace_existing=True;task.save=True
    opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_materials=False;opts.import_textures=False;opts.import_as_skeletal=False
    opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False
    task.options=opts;task.factory=unreal.FbxFactory();tools.import_asset_tasks([task])
    mesh=unreal.load_asset(folder+'/SM_'+name)
    if not mesh:raise RuntimeError('Meshy city model import failed: '+name)
    mat=unreal.load_asset(folder+'/M_'+name) or tools.create_asset('M_'+name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat);mat.set_editor_property('used_with_instanced_static_meshes',True);mat.set_editor_property('used_with_nanite',True);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    tex_nodes={}
    for channel,prop in [('base_color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('metallic',unreal.MaterialProperty.MP_METALLIC),('roughness',unreal.MaterialProperty.MP_ROUGHNESS)]:
        t=unreal.AssetImportTask();t.filename=str(root/name/'model_textures'/(channel+'.png'));t.destination_path=folder;t.destination_name='T_'+name+'_'+channel
        t.automated=True;t.replace_existing=True;t.save=True;tools.import_asset_tasks([t]);tex=unreal.load_asset(folder+'/'+t.destination_name)
        tex.set_editor_property('srgb',channel=='base_color');tex.set_editor_property('max_texture_size',4096 if channel=='base_color' else 2048)
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if channel=='normal' else unreal.TextureCompressionSettings.TC_DEFAULT if channel=='base_color' else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        n=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample);n.set_editor_property('texture',tex)
        n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='base_color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        tex_nodes[channel]=n
        lib.connect_material_property(n,'RGB' if channel in ['base_color','normal'] else 'R',prop)
    def custom(code,keys,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
        n=lib.create_material_expression(mat,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',kind)
        pins=[]
        for key in keys:p=unreal.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
        n.set_editor_property('inputs',pins);return n
    time=lib.create_material_expression(mat,unreal.MaterialExpressionTime)
    emit=custom('float hi=max(C.r,max(C.g,C.b));float lo=min(C.r,min(C.g,C.b));float sat=(hi-lo)/max(hi,0.001);float3 neon=lerp(C*float3(1,0.52,0.22),C,sat);return neon*smoothstep(0.38,0.72,hi)*4.5*(0.94+0.06*sin(T*1.4))+C*0.045;',['C','T'])
    lib.connect_material_expressions(tex_nodes['base_color'],'RGB',emit,'C');lib.connect_material_expressions(time,'',emit,'T');lib.connect_material_property(emit,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    collection=unreal.load_asset('/Game/Art/City/MPC_City')
    world=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    hero=lib.create_material_expression(mat,unreal.MaterialExpressionCollectionParameter);hero.set_editor_property('collection',collection);hero.set_editor_property('parameter_name','PlayerPosition')
    cam=lib.create_material_expression(mat,unreal.MaterialExpressionCollectionParameter);cam.set_editor_property('collection',collection);cam.set_editor_property('parameter_name','CameraPosition')
    cut=custom('float3 d=C.xyz-P.xyz;float u=dot(W-P.xyz,d)/max(1,dot(d,d));float r=length(W-(P.xyz+d*saturate(u)));float f=(1-smoothstep(130,230,r))*step(0.04,u)*step(u,0.98)*step(P.z+35,W.z)*P.w;float h=frac(sin(dot(floor(W*1.3),float3(12.989,78.233,37.719)))*43758.5453);return step(h,1-f*0.96);',['W','P','C'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    for src,key in [(world,'W'),(hero,'P'),(cam,'C')]:lib.connect_material_expressions(src,'',cut,key)
    lib.connect_material_property(cut,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
    for i in range(len(mesh.get_editor_property('static_materials'))):mesh.set_material(i,mat)
    settings=mesh.get_editor_property('nanite_settings');settings.set_editor_property('enabled',True);mesh.set_editor_property('nanite_settings',settings)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
    report.append({'asset':name,'nanite':True,'materials':len(mesh.get_editor_property('static_materials'))})
(root/'import-report.json').write_text(__import__('json').dumps(report,indent=2))
unreal.log('MESHY_CITY_ASSETS_READY: 3 textured Nanite models, emissive neon, independent gameplay collision')
