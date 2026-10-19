"""Import original generated raster assets; sprites use UV regions, not edited PNGs."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())/'ContentSource/UI/SignalDock'
tools=unreal.AssetToolsHelpers.get_asset_tools()
for path in root.glob('*.png'):
    task=unreal.AssetImportTask();task.filename=str(path);task.destination_path='/Game/UI/SignalDock'
    task.destination_name='T_'+path.stem;task.automated=True;task.replace_existing=True;task.save=True
    tools.import_asset_tasks([task])
    texture=unreal.load_asset('/Game/UI/SignalDock/T_'+path.stem)
    if not isinstance(texture,unreal.Texture2D):raise RuntimeError(path.stem)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('never_stream',True)
    texture.set_editor_property('max_texture_size',2048 if path.stem.startswith('Signal') else 512)
    unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
unreal.log('SIGNAL_DOCK_GRAPHICS_IMPORT_PASS six generated textures')
