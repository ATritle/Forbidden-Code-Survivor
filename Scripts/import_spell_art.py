import unreal
from pathlib import Path
root = Path(unreal.Paths.project_dir()) / 'ContentSource/Meshy/SpellEffects'
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
for name in ['StormLance', 'BloodRite', 'School_Lightning', 'School_Blood', 'Lightning', 'Fireball', 'BloodSplatter', 'BloodPool', 'BloodDroplets']:
    task = unreal.AssetImportTask()
    task.filename = str(root / (name + '.png'))
    task.destination_path = '/Game/UI/Codex' if name in ['StormLance', 'BloodRite', 'School_Lightning', 'School_Blood'] else '/Game/Art/Spells'
    task.destination_name = 'T_' + name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(task.destination_path + '/T_' + name)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
task = unreal.AssetImportTask()
task.filename = str(root / 'Spear.fbx')
task.destination_path = '/Game/Art/Spells'
task.destination_name = 'SM_Spear'
task.automated = True
task.replace_existing = True
task.save = True
opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = False
opts.import_materials = True
opts.import_textures = True
opts.automated_import_should_detect_type = False
opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
opts.static_mesh_import_data.combine_meshes = True
task.options = opts
tools.import_asset_tasks([task])

def custom_input(name):
    value=unreal.CustomInput()
    value.set_editor_property("input_name",name)
    return value
def material(name):
    path = '/Game/Art/Spells/' + name
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name, '/Game/Art/Spells', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    return m

def node(m, cls):
    return lib.create_material_expression(m, cls)

def connect(a, out, b, inp):
    lib.connect_material_expressions(a, out, b, inp)

def save(m):
    lib.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False)
for label in ['Lightning', 'Fireball', 'BloodDroplets']:
    m = material('M_' + label)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_niagara_sprites', True)
    m.set_editor_property('used_with_niagara_ribbons', True)
    tex = node(m, unreal.MaterialExpressionTextureSample)
    tex.set_editor_property('texture', unreal.load_asset('/Game/Art/Spells/T_' + label))
    color = node(m, unreal.MaterialExpressionParticleColor)
    mul = node(m, unreal.MaterialExpressionMultiply)
    connect(tex, 'RGB', mul, 'A')
    connect(color, 'RGB', mul, 'B')
    lib.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(color, 'A', unreal.MaterialProperty.MP_OPACITY)
    save(m)
m = material('M_BloodFloor')
m.set_editor_property('used_with_instanced_static_meshes', True)
world = node(m, unreal.MaterialExpressionWorldPosition)
bounds = node(m, unreal.MaterialExpressionVectorParameter)
bounds.set_editor_property('parameter_name', 'PaintBounds')
bounds.set_editor_property('default_value', unreal.LinearColor(-9500, -9500, 19000, 19000))
uv = node(m, unreal.MaterialExpressionCustom)
uv.set_editor_property('code', 'return (World.xy-Bounds.xy)/Size.xy;')
uv.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
uv.set_editor_property('inputs', [custom_input('World'), custom_input('Bounds'),custom_input('Size')])
connect(world, '', uv, 'World')
connect(bounds, '', uv, 'Bounds')
size=node(m,unreal.MaterialExpressionVectorParameter)
size.set_editor_property('parameter_name','PaintSize')
size.set_editor_property('default_value',unreal.LinearColor(19000,19000,0,0))
connect(size,'',uv,'Size')
tex = node(m, unreal.MaterialExpressionTextureSampleParameter2D)
tex.set_editor_property('parameter_name', 'BloodMask')
tex.set_editor_property('texture', unreal.load_asset('/Game/Art/Spells/T_BloodSplatter'))
connect(uv, '', tex, 'UVs')
norm = node(m, unreal.MaterialExpressionPixelNormalWS)
shade = node(m, unreal.MaterialExpressionCustom)
shade.set_editor_property('code', 'float a=saturate(Mask)*smoothstep(0.6,0.9,Normal.z); return lerp(float3(0.07,0.09,0.1),float3(0.19,0.003,0.008),a);')
shade.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
shade.set_editor_property('inputs', [custom_input('Mask'), custom_input('Normal')])
connect(tex, 'R', shade, 'Mask')
connect(norm, '', shade, 'Normal')
lib.connect_material_property(shade, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(m, unreal.MaterialExpressionLinearInterpolate)
rough.set_editor_property('const_a', 0.72)
rough.set_editor_property('const_b', 0.17)
connect(tex, 'R', rough, 'Alpha')
lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
save(m)
unreal.log('SPELL_ART_IMPORTED')