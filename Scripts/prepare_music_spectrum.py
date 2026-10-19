"""Bake genuine track spectra; runtime samples them at the audio component playhead."""
import json
from pathlib import Path
import unreal

report=[]
for name in ['Music_Menu','Music_Gameplay','Music_Monume','Music_Synthwave']:
    wave=unreal.load_asset('/Game/Audio/SW_'+name)
    if not isinstance(wave,unreal.SoundWave): raise RuntimeError(name)
    wave.set_editor_property('enable_baked_fft_analysis',False)
    wave.set_editor_property('fft_size',unreal.SoundWaveFFTSize.VERY_LARGE_2048)
    wave.set_editor_property('fft_analysis_frame_size',1024)
    wave.set_editor_property('fft_analysis_attack_time',10)
    wave.set_editor_property('fft_analysis_release_time',90)
    wave.set_editor_property('frequencies_to_analyze',[50*320**(i/31) for i in range(32)])
    wave.set_editor_property('enable_baked_fft_analysis',True)
    unreal.EditorAssetLibrary.save_loaded_asset(wave,only_if_is_dirty=False)
    # Cooked arrays are not Python-exposed; native playback QA verifies their contents.
    report.append(dict(asset=name,bands=32,fft=2048,hop=1024))
out=Path(unreal.Paths.project_dir())/'ContentSource/Audio/spectrum-report.json'
out.write_text(json.dumps(report,indent=2))
unreal.log('MUSIC_SPECTRUM_BAKE_PASS '+str(report))
