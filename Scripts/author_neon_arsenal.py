"""Author ten distinct hardlight materials and import full, square card illustrations."""
import os
import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
folder='/Game/Art/Arsenal'
ids=['ArcRelay','GhostLink','NeonCataclysm','OrbitRazor','RicochetSeal','StrobeRail','EchoPetals','EventHorizon','ShatterSigil','SatelliteChoir']
colors=[(.02,.55,1),(.65,.2,1),(1,.01,.38),(.85,1,.08),(.12,1,.3),(.65,.9,1),(1,.22,.12),(.3,.04,1),(1,.38,.025),(.015,1,.72)]
patterns=[
    'exp(-abs(p.y-sin(p.x*16+T*30)*0.1)*35)*saturate(1-r)',
    'exp(-abs(r-(.6+.08*sin(a*6+T*7)))*30)+exp(-r*6)*.5',
    'pow(saturate(1-r),2)*(.6+.4*sin(a*8+r*25-T*20))+exp(-abs(r-.6)*40)',
    'exp(-abs(r-.64)*42)*smoothstep(-.5,.1,p.x)+exp(-abs(r-.43)*50)*smoothstep(-.1,.3,-p.x)',
    'exp(-abs(max(abs(p.x)*.866+abs(p.y)*.5,abs(p.y))-.58)*50)+exp(-r*10)',
    'exp(-abs(p.y)*60)+exp(-abs(r-.55)*50)*.5',
    'pow(saturate(1-abs(p.x)-p.y*p.y*4),4)+exp(-abs(abs(p.y)-(.22-.18*abs(p.x)))*60)*saturate(1-abs(p.x))',
    'exp(-abs(r-.75)*55)+exp(-abs(r-(.3+.1*sin(T*12)))*35)+pow(saturate(cos(a*12+T*4)),16)*exp(-abs(r-.55)*20)',
    'exp(-abs(max(abs(p.x)*.866+p.y*.5,-p.y)-.5)*55)+exp(-r*12)',
    'pow(saturate(1-r),3)+exp(-abs(r-.7)*40)+pow(saturate(cos(a*3+T*5)),20)*exp(-abs(r-.45)*22)'
]
def make(name,index,sparks=False):
    mat=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('used_with_niagara_sprites',True)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    time=lib.create_material_expression(mat,unreal.MaterialExpressionTime)
    age=lib.create_material_expression(mat,unreal.MaterialExpressionParticleRelativeTime)
    custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
    pins=[]
    for key in ['UV','T','Age']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
    custom.set_editor_property('inputs',pins)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    color=colors[index]
    expr=patterns[index]
    if sparks:expr='('+expr+')*pow(1-saturate(Age),1.5)'
    custom.set_editor_property('code',f'float2 p=UV*2-1;float r=length(p);float a=atan2(p.y,p.x);float energy=max(0,({expr}));return float3({color[0]},{color[1]},{color[2]})*energy*3.5;')
    for node,key in [(uv,'UV'),(time,'T'),(age,'Age')]:lib.connect_material_expressions(node,'',custom,key)
    lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
for index,asset_id in enumerate(ids):
    make('M_'+asset_id,index);make('M_Sparks_'+asset_id,index,True)
    path=os.path.join(unreal.Paths.project_dir(),'ContentSource','UI','NeonArsenal',asset_id+'.png')
    if os.path.exists(path):
        task=unreal.AssetImportTask();task.filename=path;task.destination_path='/Game/UI/Codex';task.destination_name='T_'+asset_id;task.automated=True;task.replace_existing=True;task.save=True
        tools.import_asset_tasks([task])
        texture=unreal.load_asset('/Game/UI/Codex/T_'+asset_id)
        texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('max_texture_size',1024)
        unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
unreal.log('NEON_ARSENAL_MATERIALS_READY')
