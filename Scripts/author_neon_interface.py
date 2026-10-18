"""Canvas + UMG glass styles, real anisotropic backdrop sampling, and HUD alarm."""
from pathlib import Path
import json
import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
folder='/Game/UI/NeonGlass'
root=Path(unreal.Paths.project_dir())/'ContentSource/UI/NeonGlass'
task=unreal.AssetImportTask();task.filename=str(root/'NeonDistrict-Menu.png');task.destination_path=folder;task.destination_name='T_NeonDistrictMenu'
task.automated=True;task.replace_existing=True;task.save=True;tools.import_asset_tasks([task])
backdrop=unreal.load_asset(folder+'/T_NeonDistrictMenu');backdrop.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
backdrop.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);backdrop.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
unreal.EditorAssetLibrary.save_loaded_asset(backdrop)

def make(name,code,ui=False):
    m=unreal.load_asset(folder+'/'+name)
    if not m:m=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI if ui else unreal.MaterialDomain.MD_SURFACE)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    if not ui:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    time=lib.create_material_expression(m,unreal.MaterialExpressionTime);time.set_editor_property('ignore_pause',True)
    texture=lib.create_material_expression(m,unreal.MaterialExpressionTextureObjectParameter);texture.set_editor_property('parameter_name','Backdrop');texture.set_editor_property('texture',backdrop)
    nodes=[(uv,'UV'),(time,'T'),(texture,'Backdrop')]
    for key,default in [('Rect',(0,0,1,1)),('Accent',(0.02,0.8,1,1)),('Control',(0,0,0,1)),('PanelSize',(460,300,0,0))]:
        n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',unreal.LinearColor(*default))
        rgb=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask);rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True);rgb.set_editor_property('a',False)
        lib.connect_material_expressions(n,'',rgb,'')
        vector4=lib.create_material_expression(m,unreal.MaterialExpressionAppendVector);lib.connect_material_expressions(rgb,'',vector4,'A');lib.connect_material_expressions(n,'A',vector4,'B');nodes.append((vector4,key))
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    pins=[]
    for src,key in nodes:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
    n.set_editor_property('inputs',pins)
    for src,key in nodes:lib.connect_material_expressions(src,'',n,key)
    rgb=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask);rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True);rgb.set_editor_property('a',False)
    alpha=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask);alpha.set_editor_property('r',False);alpha.set_editor_property('g',False);alpha.set_editor_property('b',False);alpha.set_editor_property('a',True)
    lib.connect_material_expressions(n,'',rgb,'');lib.connect_material_expressions(n,'',alpha,'')
    lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)

glass='''
float2 size=max(PanelSize.xy,float2(8,8));float2 p=UV*size;
float cut=Control.z>1.5 ? 18 : 7;
float d=max(max(2-p.x,p.x-size.x+2),max(2-p.y,p.y-size.y+2));
d=max(d,(p.x-p.y-size.x+cut)*0.7071);d=max(d,(p.y-p.x-size.y+cut)*0.7071);
float inside=1-smoothstep(-0.5,0.8,d);float rim=exp(-abs(d+1.2)*1.8);float glow=exp(-abs(d+2)*0.14);
float2 pos=Rect.xy+UV*Rect.zw;float3 bg=Texture2DSample(Backdrop,BackdropSampler,pos).rgb*0.2;
bg+=(Texture2DSample(Backdrop,BackdropSampler,pos+float2(0.006,0)).rgb+Texture2DSample(Backdrop,BackdropSampler,pos-float2(0.006,0)).rgb)*0.16;
bg+=(Texture2DSample(Backdrop,BackdropSampler,pos+float2(0.014,0)).rgb+Texture2DSample(Backdrop,BackdropSampler,pos-float2(0.014,0)).rgb)*0.12;
bg+=(Texture2DSample(Backdrop,BackdropSampler,pos+float2(0.026,0)).rgb+Texture2DSample(Backdrop,BackdropSampler,pos-float2(0.026,0)).rgb)*0.08;
bg+=(Texture2DSample(Backdrop,BackdropSampler,pos+float2(0,0.001)).rgb+Texture2DSample(Backdrop,BackdropSampler,pos-float2(0,0.001)).rgb)*0.04;
float2 q=p/20;float2 period=float2(1,1.73205);float2 a=fmod(q,period)-period*0.5;float2 b=fmod(q-period*0.5,period)-period*0.5;
float2 cell=abs(dot(a,a)<dot(b,b)?a:b);float hex=max(cell.x*0.866025+cell.y*0.5,cell.y);float grid=exp(-abs(hex-0.48)*100);
float pulse=0.9+0.1*sin(T*2.2+Rect.x*9);float emphasis=0.65+Control.x*0.35+Control.y*0.5;
float chevron=Control.z>1.5 ? pow(saturate(sin((p.x+abs(p.y-size.y*0.5)*1.4)*0.085-T*3.8)),16)*0.07 : 0;
float3 base=lerp(float3(0.009,0.02,0.038),pow(max(bg,0.000001),0.4545)*0.19+float3(0.005,0.015,0.03),saturate(Control.w));
float3 rgb=base+Accent.rgb*(grid*0.04+chevron+rim*emphasis*pulse*1.8+glow*emphasis*0.07);
return float4(rgb,(0.76+Control.x*0.12)*inside);
'''
make('M_NeonGlass',glass);make('M_NeonGlass_UMG',glass,True)
make('M_NeonBackdrop','''float3 c=pow(max(Texture2DSample(Backdrop,BackdropSampler,UV).rgb,0.000001),0.4545);float shade=0.58+0.12*smoothstep(0.2,0.8,UV.y);float edge=1-smoothstep(0.25,0.8,length(UV-0.5));return float4(c*shade*(0.8+edge*0.2),1);''')
make('M_NeonCardArt','''float3 c=pow(max(Texture2DSample(Backdrop,BackdropSampler,UV).rgb,0.000001),0.4545);float edge=smoothstep(0,0.035,UV.x)*smoothstep(0,0.035,1-UV.x)*smoothstep(0,0.035,UV.y)*smoothstep(0,0.035,1-UV.y);return float4(c*(0.95+Control.x*0.12),edge);''')
make('M_CriticalHealth','''float distance=min(min(UV.x,1-UV.x),min(UV.y,1-UV.y));float edge=1-smoothstep(0.0,0.22,distance);float heartbeat=0.75+0.25*pow(saturate(sin(T*7)),6);float scan=pow(saturate(sin((UV.x+UV.y)*350-T*6)),12);float alpha=edge*(0.28+Control.x*0.15)*heartbeat+exp(-distance*700)*0.25;return float4(float3(0.95,0.015,0.14)+scan*float3(0.05,0,0.05),alpha);''')
(root/'style-manifest.json').write_text(json.dumps({'background':'NeonDistrict-Menu.png','generation':'built-in imagegen','palette':['mint','pink','violet','cyan','gold','teal'],'glass':'nine-tap anisotropic texture blur, honeycomb SDF grid, pulsing neon rims','parameters':['Backdrop','Rect','Accent','Control','PanelSize']},indent=2))
unreal.log('NEON_INTERFACE_MATERIALS_READY: anisotropic glass, honeycomb, chamfer scans, spell art tint, health edge warning')
