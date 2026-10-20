"""Import the editable city kit and author shared wet PBR / animated neon shaders."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())/'ContentSource/City'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
folder='/Game/Art/City'
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
for name in ['SM_CityPanel','SM_CityPipe','SM_CityRamp','SM_CityCanopy']:
    task=unreal.AssetImportTask();task.filename=str(root/'Kit'/(name+'.fbx'));task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True
    opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_materials=False;opts.import_textures=False
    opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False
    task.options=opts;task.factory=unreal.FbxFactory();tools.import_asset_tasks([task])
    if not unreal.load_asset(folder+'/'+name):raise RuntimeError('City mesh import failed: '+name)
task=unreal.AssetImportTask();task.filename=str(root/'NeonSigns.png');task.destination_path=folder;task.destination_name='T_NeonSigns'
task.automated=True;task.replace_existing=True;task.save=True;tools.import_asset_tasks([task])
texture=unreal.load_asset(folder+'/T_NeonSigns')
if not texture:raise RuntimeError('Original sign texture atlas is missing')
texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property('max_texture_size',2048);unreal.EditorAssetLibrary.save_loaded_asset(texture)
collection=unreal.load_asset(folder+'/MPC_City')
if not collection:
    collection=tools.create_asset('MPC_City',folder,unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
if len(collection.get_editor_property('vector_parameters'))<2:
    params=[]
    for i,(name,value) in enumerate([('PlayerPosition',(0,0,100,1)),('CameraPosition',(-1200,0,1600,1))]):
        p=unreal.CollectionVectorParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',unreal.LinearColor(*value))
        params.append(p)
    collection.set_editor_property('vector_parameters',params)
    unreal.EditorAssetLibrary.save_loaded_asset(collection)

def node(m,cls):return lib.create_material_expression(m,cls)
def wire(a,out,b,key):lib.connect_material_expressions(a,out,b,key)
def custom(m,code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n=node(m,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',kind)
    pins=[]
    for key in inputs:
        p=unreal.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
    n.set_editor_property('inputs',pins);return n
def scalar(m,name,value):
    n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def vector(m,name,value):
    n=node(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value));return n
def connect(n,prop,out=''):lib.connect_material_property(n,out,prop)
def save(m):lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
def material(name,blend=unreal.BlendMode.BLEND_OPAQUE,unlit=False):
    m=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m);m.set_editor_property('blend_mode',blend)
    m.set_editor_property('two_sided',True);m.set_editor_property('used_with_instanced_static_meshes',True)
    if unlit:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    return m
def cutaway(m,world):
    hero=node(m,unreal.MaterialExpressionCollectionParameter);hero.set_editor_property('collection',collection);hero.set_editor_property('parameter_name','PlayerPosition')
    cam=node(m,unreal.MaterialExpressionCollectionParameter);cam.set_editor_property('collection',collection);cam.set_editor_property('parameter_name','CameraPosition')
    n=custom(m,'float3 d=C.xyz-P.xyz;float u=dot(W-P.xyz,d)/max(1,dot(d,d));float r=length(W-(P.xyz+d*saturate(u)));float cut=(1-smoothstep(130,230,r))*step(0.04,u)*step(u,0.98)*step(P.z+35,W.z)*P.w;float hash=frac(sin(dot(floor(W*1.3),float3(12.989,78.233,37.719)))*43758.5453);return step(hash,1-cut*0.96);',['W','P','C'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wire(world,'',n,'W');wire(hero,'',n,'P');wire(cam,'',n,'C');return n

m=material('M_CitySurface',unreal.BlendMode.BLEND_MASKED)
world=node(m,unreal.MaterialExpressionWorldPosition);time=node(m,unreal.MaterialExpressionTime)
color=vector(m,'BaseColor',(0.025,0.03,0.055,1));metal=scalar(m,'Metallic',0.65);rough=scalar(m,'Roughness',0.33)
out=custom(m,'float grain=0.92+0.08*sin(W.x*0.16)*sin(W.y*0.18+W.z*0.11);return C*grain;',['W','C'])
wire(world,'',out,'W');wire(color,'RGB',out,'C');connect(out,unreal.MaterialProperty.MP_BASE_COLOR)
connect(metal,unreal.MaterialProperty.MP_METALLIC);connect(rough,unreal.MaterialProperty.MP_ROUGHNESS)
connect(cutaway(m,world),unreal.MaterialProperty.MP_OPACITY_MASK);save(m)
for name,color,metallic,roughness in [
    ('MI_Wall',(0.035,0.032,0.065,1),0.45,0.38),('MI_Panel',(0.018,0.025,0.04,1),0.7,0.3),
    ('MI_Trim',(0.11,0.12,0.18,1),0.8,0.25),('MI_Rust',(0.11,0.047,0.035,1),0.55,0.46),
    ('MI_Roof',(0.025,0.03,0.045,1),0.35,0.3),('MI_Canvas',(0.11,0.018,0.055,1),0.12,0.42),
    ('MI_Train',(0.035,0.08,0.10,1),0.7,0.24),('MI_Shrine',(0.17,0.028,0.055,1),0.6,0.28)]:
    mi=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi,m);lib.set_material_instance_vector_parameter_value(mi,'BaseColor',unreal.LinearColor(*color))
    lib.set_material_instance_scalar_parameter_value(mi,'Metallic',metallic);lib.set_material_instance_scalar_parameter_value(mi,'Roughness',roughness)
    unreal.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False)

m=material('M_WetConcrete',unreal.BlendMode.BLEND_OPAQUE);m.set_editor_property('two_sided',False);world=node(m,unreal.MaterialExpressionWorldPosition);time=node(m,unreal.MaterialExpressionTime)
base=custom(m,'float2 g=abs(frac((W.xy+200)/400)-0.5);float seam=1-smoothstep(0.485,0.499,max(g.x,g.y));float noise=0.8+0.12*sin(W.x*0.28)*sin(W.y*0.31)+0.08*sin(W.x*0.027+W.y*0.019);return float3(0.023,0.028,0.04)*noise*lerp(0.32,1,seam);',['W'])
wire(world,'',base,'W');connect(base,unreal.MaterialProperty.MP_BASE_COLOR)
rough=custom(m,'float2 q=W.xy/330;float2 i=floor(q),f=frac(q);f=f*f*(3-2*f);float4 h=frac(sin(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+1,float2(127.1,311.7))))*43758.5453);float n=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);return lerp(0.43,0.11,smoothstep(0.46,0.8,n));',['W'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
wire(world,'',rough,'W');connect(rough,unreal.MaterialProperty.MP_ROUGHNESS)
normal=custom(m,'float2 cell=floor(W.xy/55);float2 q=frac(W.xy/55)-0.5;float seed=frac(sin(dot(cell,float2(17.21,71.33)))*43758);float age=frac(T*0.65+seed);float r=length(q);float ring=exp(-abs(r-age*0.65)*55)*(1-age);float2 dir=q/max(r,0.01);float2 slope=dir*cos(r*90-age*35)*ring*0.012+float2(cos(W.x*0.17),sin(W.y*0.13))*0.002;return normalize(float3(slope,1));',['W','T'])
wire(world,'',normal,'W');wire(time,'',normal,'T');connect(normal,unreal.MaterialProperty.MP_NORMAL)
connect(scalar(m,'Metallic',0.04),unreal.MaterialProperty.MP_METALLIC);connect(scalar(m,'Specular',0.6),unreal.MaterialProperty.MP_SPECULAR);save(m)

m=material('M_NeonGlow',unreal.BlendMode.BLEND_MASKED,True)
world=node(m,unreal.MaterialExpressionWorldPosition);time=node(m,unreal.MaterialExpressionTime)
color=vector(m,'NeonColor',(0.02,0.5,1,1));power=scalar(m,'Power',3)
pulse=custom(m,'return C*Power*(0.92+0.08*sin(T*1.7+W.z*0.008));',['C','Power','T','W'])
for n,out,key in [(color,'RGB','C'),(power,'','Power'),(time,'','T'),(world,'','W')]:wire(n,out,pulse,key)
connect(pulse,unreal.MaterialProperty.MP_EMISSIVE_COLOR);connect(cutaway(m,world),unreal.MaterialProperty.MP_OPACITY_MASK);save(m)
for name,col,power in [('Cyan',(0.015,0.65,1,1),3.5),('Pink',(1,0.015,0.4,1),3.5),('Amber',(1,0.28,0.045,1),4),('Ivory',(1,0.68,0.35,1),3),('Lime',(0.3,1,0.04,1),3),('Violet',(0.35,0.045,1,1),3),('Dim',(0.15,0.32,0.48,1),0.5)]:
    mi=unreal.load_asset(folder+'/MI_'+name) or tools.create_asset('MI_'+name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi,m);lib.set_material_instance_vector_parameter_value(mi,'NeonColor',unreal.LinearColor(*col));lib.set_material_instance_scalar_parameter_value(mi,'Power',power);unreal.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False)

m=material('M_NeonSign',unreal.BlendMode.BLEND_MASKED,True)
uv=node(m,unreal.MaterialExpressionTextureCoordinate);world=node(m,unreal.MaterialExpressionWorldPosition);time=node(m,unreal.MaterialExpressionTime)
index=scalar(m,'Tile',0)
atlasuv=custom(m,'float2 tile=float2(fmod(Index,4),floor(Index/4));return (tile+lerp(0.01,0.99,float2(1-UV.x,1-UV.y)))/4;',['UV','Index'],unreal.CustomMaterialOutputType.CMOT_FLOAT2)
wire(uv,'',atlasuv,'UV');wire(index,'',atlasuv,'Index')
tex=node(m,unreal.MaterialExpressionTextureSample);tex.set_editor_property('texture',texture);wire(atlasuv,'',tex,'UVs')
out=custom(m,'float scan=0.94+0.06*sin(UV.y*230-T*9);float flicker=0.9+0.1*sin(T*1.8+Index*4);float sweep=1+0.14*pow(saturate(sin(UV.y*6.28-T*0.7+Index)),12);return Tex*3.7*scan*flicker*sweep;',['Tex','UV','T','Index'])
for n,o,k in [(tex,'RGB','Tex'),(uv,'','UV'),(time,'','T'),(index,'','Index')]:wire(n,o,out,k)
connect(out,unreal.MaterialProperty.MP_EMISSIVE_COLOR);connect(cutaway(m,world),unreal.MaterialProperty.MP_OPACITY_MASK);save(m)
for i in range(16):
    name='MI_Sign%02d'%i;mi=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi,m);lib.set_material_instance_scalar_parameter_value(mi,'Tile',i);unreal.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False)

m=material('M_Rain',unreal.BlendMode.BLEND_ADDITIVE,True)
time=node(m,unreal.MaterialExpressionTime);seed=node(m,unreal.MaterialExpressionPerInstanceRandom)
fall=custom(m,'float z=frac(Seed-T*0.9)*1600-Seed*1600;return float3(z*0.07,0,z);',['T','Seed'])
wire(time,'',fall,'T');wire(seed,'',fall,'Seed');connect(fall,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
connect(vector(m,'RainColor',(0.12,0.22,0.42,1)),unreal.MaterialProperty.MP_EMISSIVE_COLOR,'RGB')
connect(scalar(m,'Opacity',0.12),unreal.MaterialProperty.MP_OPACITY);save(m)

m=material('M_DigitalGhost',unreal.BlendMode.BLEND_ADDITIVE,True)
world=node(m,unreal.MaterialExpressionWorldPosition);time=node(m,unreal.MaterialExpressionTime)
view=node(m,unreal.MaterialExpressionCameraVectorWS);normal=node(m,unreal.MaterialExpressionPixelNormalWS)
out=custom(m,'float rim=pow(1-saturate(abs(dot(N,V))),2);float scan=pow(saturate(sin(W.z*0.45-T*3)),8);return float3(0.02,0.8,1.2)*(rim*1.2+scan*0.15+0.02);',['W','T','N','V'])
for n,o,k in [(world,'','W'),(time,'','T'),(normal,'','N'),(view,'','V')]:wire(n,o,out,k)
connect(out,unreal.MaterialProperty.MP_EMISSIVE_COLOR);connect(scalar(m,'Opacity',0.35),unreal.MaterialProperty.MP_OPACITY)
offset=custom(m,'return float3(0,0,sin(T*1.8+W.x*0.005)*9);',['W','T']);wire(world,'',offset,'W');wire(time,'',offset,'T');connect(offset,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET);save(m)

m=material('M_NightSky',unreal.BlendMode.BLEND_OPAQUE,True)
connect(vector(m,'SkyColor',(0.009,0.004,0.034,1)),unreal.MaterialProperty.MP_EMISSIVE_COLOR,'RGB');save(m)
unreal.log('CITY_MATERIALS_READY: beveled kit, PBR wet concrete/ripples, animated original signage, rain, digital spirits, camera cutaway')
