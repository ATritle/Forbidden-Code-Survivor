"""Import the five approved fixed-pose rider illustrations for the combined menu."""
from pathlib import Path
import json
import unreal
root=Path(unreal.Paths.project_dir())/'ContentSource/UI/LoadoutWorkbench'
folder='/Game/UI/LoadoutWorkbench'
tools=unreal.AssetToolsHelpers.get_asset_tools()
report=[]
for rider in ['NeonExorcist','Vex','Rook','Zero','Mira']:
    task=unreal.AssetImportTask(); task.filename=str(root/f'Rider-{rider}-v1.png')
    task.destination_path=folder;task.destination_name=f'T_Rider_{rider}'
    task.automated=True;task.replace_existing=True;task.save=True
    tools.import_asset_tasks([task])
    asset=unreal.load_asset(folder+'/'+task.destination_name)
    if not asset: raise RuntimeError('Missing '+rider)
    asset.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    asset.set_editor_property('srgb',True);asset.set_editor_property('never_stream',True)
    asset.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);asset.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    report.append({'rider':rider,'asset':asset.get_path_name(),'source':task.filename})
(root/'import-manifest.json').write_text(json.dumps(report,indent=2))
