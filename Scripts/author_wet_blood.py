import unreal
lib=unreal.MaterialEditingLibrary
m=unreal.load_asset('/Game/Art/Spells/M_BloodFloor')
lib.delete_all_material_expressions(m)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('tangent_space_normal',False)
m.set_editor_property('used_with_instanced_static_meshes',True)
def node(cls): return lib.create_material_expression(m,cls)
def wire(a,out,b,key): lib.connect_material_expressions(a,out,b,key)
def custom(code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n=node(unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',kind)
    pins=[]
    for name in inputs:
        v=unreal.CustomInput();v.set_editor_property('input_name',name);pins.append(v)
    n.set_editor_property('inputs',pins);return n
def parameter(name,value):
    n=node(unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value));return n
world=node(unreal.MaterialExpressionWorldPosition)
normal=node(unreal.MaterialExpressionVertexNormalWS)
time=node(unreal.MaterialExpressionTime)
bounds=parameter('PaintBounds',(-9500,-9500,0,0));size=parameter('PaintSize',(19000,19000,0,0))
uv=custom('return (World.xy-Bounds.xy)/Size.xy;',['World','Bounds','Size'],unreal.CustomMaterialOutputType.CMOT_FLOAT2)
for src,key in [(world,'World'),(bounds,'Bounds'),(size,'Size')]: wire(src,'',uv,key)
tex=node(unreal.MaterialExpressionTextureSampleParameter2D);tex.set_editor_property('parameter_name','BloodMask');tex.set_editor_property('texture',unreal.load_asset('/Game/Art/Spells/T_BloodSplatter'));wire(uv,'',tex,'UVs')
mask=custom('return saturate(Mask)*smoothstep(0.6,0.9,N.z);',['Mask','N'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
wire(tex,'R',mask,'Mask');wire(normal,'',mask,'N')
shade=custom('float depth=smoothstep(0.15,0.95,A);float grain=0.5+0.5*sin(W.x*0.19)*sin(W.y*0.23);return lerp(float3(0.07,0.09,0.1)*(0.95+grain*0.1),lerp(float3(0.085,0.002,0.004),float3(0.025,0.0007,0.0015),depth),A);',['A','W'])
wire(mask,'',shade,'A');wire(world,'',shade,'W');lib.connect_material_property(shade,'',unreal.MaterialProperty.MP_BASE_COLOR)
rough=custom('return lerp(0.72,lerp(0.18,0.065,smoothstep(0.2,0.9,A)),A);',['A'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
wire(mask,'',rough,'A');lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
# World-space shallow crossing ripples move specular reflections without sliding the stamped boundary.
ripple=custom('float x=W.x*0.075+W.y*0.033;float y=W.y*0.096-W.x*0.021;float a=0.025*A*A;float2 slope=float2(cos(x-T*1.1)+0.4*cos(y+T*0.7),sin(y+T*0.7)+0.35*cos(x-T*1.1))*a;float3 d=float3(slope,0);return normalize(N+d-N*dot(N,d));',['A','W','T','N'])
for src,key in [(mask,'A'),(world,'W'),(time,'T'),(normal,'N')]: wire(src,'',ripple,key)
lib.connect_material_property(ripple,'',unreal.MaterialProperty.MP_NORMAL)
for value,prop in [(0.5,unreal.MaterialProperty.MP_SPECULAR),(0.0,unreal.MaterialProperty.MP_METALLIC)]:
    n=node(unreal.MaterialExpressionConstant);n.set_editor_property('r',value);lib.connect_material_property(n,'',prop)
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('WET_BLOOD_READY')

# Animate the existing Meshy fire texture inside the smaller Niagara core.
m=unreal.load_asset('/Game/Art/Spells/M_Fireball')
# Rebuild explicitly so repeated authoring remains deterministic.
lib.delete_all_material_expressions(m)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property('used_with_niagara_sprites',True)
uv=node(unreal.MaterialExpressionTextureCoordinate);time=node(unreal.MaterialExpressionTime)
flow=custom('float2 p=UV-0.5;float r=length(p);float a=0.16*sin(T*4-r*12);float c=cos(a),s=sin(a);return mul(float2x2(c,-s,s,c),p)*(1+0.045*sin(T*7+r*20))+0.5;',['UV','T'],unreal.CustomMaterialOutputType.CMOT_FLOAT2)
wire(uv,'',flow,'UV');wire(time,'',flow,'T')
tex=node(unreal.MaterialExpressionTextureSample);tex.set_editor_property('texture',unreal.load_asset('/Game/Art/Spells/T_Fireball'));wire(flow,'',tex,'UVs')
col=node(unreal.MaterialExpressionParticleColor)
flame=custom('float edge=1-smoothstep(0.7,1.0,length(UV*2-1));return Tex*Color*edge*(0.8+0.2*sin(T*13+UV.y*25));',['Tex','Color','UV','T'])
for src,out,key in [(tex,'RGB','Tex'),(col,'RGB','Color'),(uv,'','UV'),(time,'','T')]: wire(src,out,flame,key)
lib.connect_material_property(flame,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(col,'A',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('DETAILED_FIRE_CORE_READY')
