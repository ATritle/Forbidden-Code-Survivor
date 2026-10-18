"""Import the approved Meshy 9-credit sheet's nine cropped UI textures."""
import unreal
from pathlib import Path
source = Path(unreal.Paths.project_dir()) / 'ContentSource/Meshy/CodexUI'
names = ['BoneLance','EmberFan','FrostNova','RaiseSkeletons','School_Necromancy','School_Fire','School_Blood','School_Curses','School_Frost']
for name in names:
    task = unreal.AssetImportTask()
    task.filename = str(source / (name + '.png'))
    task.destination_path = '/Game/UI/Codex'
    task.destination_name = 'T_' + name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset('/Game/UI/Codex/T_' + name)
    if not texture:
        raise RuntimeError('Missing UI texture ' + name)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('srgb', True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
unreal.log('CODEX_UI_ART_READY')
