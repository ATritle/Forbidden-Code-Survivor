"""Author physical wet road, sidewalk, curb and worn paint materials without rebuilding the map."""
import unreal

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
folder = '/Game/Art/City'
m = unreal.load_asset(folder+'/M_StreetSurface') or tools.create_asset('M_StreetSurface',folder,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
m.set_editor_property('used_with_instanced_static_meshes',True)

def node(cls): return lib.create_material_expression(m,cls)
def wire(a,out,b,key):
    if not lib.connect_material_expressions(a,out,b,key):raise RuntimeError('Street material connection failed: '+str(b)+' / '+key)
def custom(code,inputs,kind):
    n=node(unreal.MaterialExpressionCustom); n.set_editor_property('code',code); n.set_editor_property('output_type',kind)
    pins=[]
    for key in inputs:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
    n.set_editor_property('inputs',pins);return n
def scalar(name,value):
    n=node(unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n

noise='''struct NoiseField {
 float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
 float sample(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);}
 float fbm(float2 p){return sample(p)*.57+sample(p*2.07+17)*.28+sample(p*4.11-9)*.15;}
}; NoiseField n;
'''
world=node(unreal.MaterialExpressionWorldPosition);time=node(unreal.MaterialExpressionTime)
color=node(unreal.MaterialExpressionVectorParameter);color.set_editor_property('parameter_name','SurfaceColor');color.set_editor_property('default_value',unreal.LinearColor(.018,.023,.03,1))
dry=scalar('DryRoughness',.78);wet=scalar('WetRoughness',.13);kind=scalar('SurfaceKind',0);joint=scalar('JointSize',0)
surface=custom(noise+'''
float puddle=smoothstep(.40,.72,n.fbm(W.xy*.0034));
float aggregate=n.sample(W.xy*.23)*.16+n.sample(W.xy*1.1)*.08;
float3 base=C*(.86+aggregate)*(1-puddle*.34);
float rough=lerp(Dry,Wet,puddle);
if(Joints>1){
 float2 q=W.xy/Joints; q.x+=fmod(floor(q.y),2)*.5;
 float2 edge=min(frac(q),1-frac(q))*Joints;
 float seam=1-smoothstep(.25,.85,min(edge.x,edge.y));
 base*=1-seam*.65;rough=max(rough,seam*.8);
}
if(Kind>1.5 && Kind<2.5){
 float chips=smoothstep(.13,.32,n.sample(W.xy*.09))*smoothstep(.06,.2,n.sample(W.xy*.7));
 base=lerp(float3(.016,.02,.026),base,chips);rough=lerp(.35,rough,chips);
}
return float4(base,rough);
''',['W','C','Dry','Wet','Kind','Joints'],unreal.CustomMaterialOutputType.CMOT_FLOAT4)
for a,out,key in [(world,'','W'),(color,'RGB','C'),(dry,'','Dry'),(wet,'','Wet'),(kind,'','Kind'),(joint,'','Joints')]:wire(a,out,surface,key)
lib.connect_material_property(surface,'',unreal.MaterialProperty.MP_BASE_COLOR)
mask=node(unreal.MaterialExpressionComponentMask);mask.set_editor_property('r',False);mask.set_editor_property('g',False);mask.set_editor_property('b',False);mask.set_editor_property('a',True)
wire(surface,'',mask,'');lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_ROUGHNESS)
normal=custom(noise+'''
float p=smoothstep(.4,.72,n.fbm(W.xy*.0034));
float2 cell=floor(W.xy/48),q=frac(W.xy/48)-.5;float seed=n.hash(cell);
float age=frac(T*.62+seed),r=length(q);float ring=exp(-abs(r-age*.65)*70)*(1-age);
float2 drops=q/max(r,.01)*cos(r*90-age*38)*ring*.016*p;
float2 grain=float2(n.sample(W.xy*.31)-.5,n.sample(W.xy*.29+31)-.5)*.07*(1-p);
return normalize(float3(drops+grain,1));
''',['W','T'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
wire(world,'',normal,'W');wire(time,'',normal,'T');lib.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
lib.connect_material_property(scalar('Specular',.5),'',unreal.MaterialProperty.MP_SPECULAR)
lib.connect_material_property(scalar('Metallic',0),'',unreal.MaterialProperty.MP_METALLIC)
# Preserve the existing camera cutaway for bridge and elevated courtyard floor meshes.
collection=unreal.load_asset(folder+'/MPC_City')
params=[]
for name in ['PlayerPosition','CameraPosition']:
    p=node(unreal.MaterialExpressionCollectionParameter);p.set_editor_property('collection',collection);p.set_editor_property('parameter_name',name);params.append(p)
cut=custom('float3 d=C.xyz-P.xyz;float u=dot(W-P.xyz,d)/max(1,dot(d,d));float r=length(W-(P.xyz+d*saturate(u)));float cut=(1-smoothstep(130,230,r))*step(.04,u)*step(u,.98)*step(P.z+35,W.z)*P.w;float h=frac(sin(dot(floor(W*1.3),float3(12.989,78.233,37.719)))*43758.5453);return step(h,1-cut*.96);',['W','P','C'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
wire(world,'',cut,'W');wire(params[0],'',cut,'P');wire(params[1],'',cut,'C');lib.connect_material_property(cut,'',unreal.MaterialProperty.MP_OPACITY_MASK)
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,False)
for name,c,d,w,k,j in [
    ('MI_Asphalt',(.018,.022,.028,1),.83,.11,0,0),
    ('MI_Sidewalk',(.095,.105,.12,1),.85,.28,1,75),
    ('MI_Curb',(.20,.215,.23,1),.78,.26,3,100),
    ('MI_RoadWhite',(.57,.60,.53,1),.68,.22,2,0),
    ('MI_RoadAmber',(.72,.35,.035,1),.68,.22,2,0),
    ('MI_Courtyard',(.044,.049,.06,1),.78,.18,1,140)]:
    mi=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi,m);lib.set_material_instance_vector_parameter_value(mi,'SurfaceColor',unreal.LinearColor(*c))
    for key,value in [('DryRoughness',d),('WetRoughness',w),('SurfaceKind',k),('JointSize',j)]:lib.set_material_instance_scalar_parameter_value(mi,key,value)
    unreal.EditorAssetLibrary.save_loaded_asset(mi,False)
unreal.log('STREET_MATERIALS_READY: wet aggregate asphalt, puddle roughness, rain normals, staggered pavers, weathered reflective paint')
