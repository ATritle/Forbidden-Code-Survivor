"""Validate and resave complete animation packages after skeleton import and LOD generation."""
import json,unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
for entry in json.loads((root/'ContentSource/Enemies/CyberRoster/roster.json').read_text()):
    folder='/Game/Art/Characters/'+entry['id'];mesh=unreal.load_asset(folder+'/SK_'+entry['id'])
    skeleton=mesh.get_editor_property('skeleton')
    if not skeleton:raise RuntimeError('Missing skeleton '+entry['id'])
    for action in ['Walk','Run']:
        clip=unreal.load_asset(folder+'/A_'+action)
        if clip.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Mismatched skeleton '+entry['id'])
        unreal.EditorAssetLibrary.save_loaded_asset(clip,only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(folder,only_if_is_dirty=False,recursive=True)
unreal.log('CYBER_ANIMATION_PACKAGES_VALIDATED')
