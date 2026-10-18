"""Reproducible SoundWave import, streaming music, short inlined mono effects."""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir())/'ContentSource/Audio'
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []
for entry in json.loads((root/'manifest.json').read_text()):
    name = entry['asset']
    music = name.startswith('Music_')
    task = unreal.AssetImportTask()
    task.filename = str(root/(name+'.wav'))
    task.destination_path = '/Game/Audio'
    task.destination_name = 'SW_'+name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.factory = unreal.SoundFactory()
    tools.import_asset_tasks([task])
    wave = unreal.load_asset('/Game/Audio/SW_'+name)
    if not isinstance(wave, unreal.SoundWave):
        raise RuntimeError('Audio import failed: '+name)
    wave.set_editor_property('looping', music)
    if music:
        wave.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    wave.set_editor_property('sound_group', unreal.SoundGroup.SOUNDGROUP_MUSIC if music else unreal.SoundGroup.SOUNDGROUP_EFFECTS)
    wave.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.RETAIN_ON_LOAD if music else unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    wave.set_editor_property('compression_quality', 85 if music else 80)
    unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False)
    report.append(dict(asset=wave.get_path_name(), seconds=wave.get_editor_property('duration'),
                       looping=wave.get_editor_property('looping'), channels=wave.get_editor_property('num_channels')))
(root/'import-report.json').write_text(json.dumps(report, indent=2))
unreal.log('AUDIO_IMPORT_PASS: '+str(len(report))+' SoundWaves, two looping music tracks')
