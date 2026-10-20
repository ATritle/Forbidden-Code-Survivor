"""Decode the user's MP3 library with Blender's aud/FFmpeg; keep originals intact."""
from pathlib import Path
import aud
import json
import numpy as np
import wave

root = Path(__file__).resolve().parents[1]
out = root / 'ContentSource/Audio'
out.mkdir(parents=True, exist_ok=True)
sources = {
    'Music_Gameplay': ('music', 'humanstudioed*', False),
    'Music_Menu': ('music', 'alex-morgan*', False),
    'Music_Monume': ('music', 'monume*', False),
    'Music_Synthwave': ('music', 'the_mountain*', False),
    'Pulse': ('weapons', '*pulse*', True),
    'Plasma': ('weapons', '*plasma-pp-05*', True),
    'Impact': ('weapons', '*plasma-pp-01*', True),
    'Laser': ('weapons', '*laser*', True),
    'Blaster': ('weapons', '*blaster*', True),
    'Enemy_Metal': ('enemy', '*mechanical*', True),
    'Enemy_Glitch': ('enemy', '*glitch*', True),
    'UI_Deny': ('menu', '*deny*', False),
    'UI_Confirm': ('menu', '*positive*', False),
    'UI_Select': ('menu', '*medium*', False),
    'UI_Click': ('menu', '*small-01*', False),
    'LowHealth': ('menu', '*warning-01-230492*', False),
}
report = []
previous = {entry['asset']: entry for entry in json.loads((out/'manifest.json').read_text())} if (out/'manifest.json').exists() else {}
for name, (folder, pattern, mono) in sources.items():
    source = next((root / 'SFX' / folder).glob(pattern + '.mp3'))
    if name in previous and previous[name]['source'] == str(source.relative_to(root)) and (out/(name+'.wav')).exists():
        report.append(previous[name])
        continue
    sound = aud.Sound(str(source)).rechannel(1 if mono else 2).resample(48000).volume(0.85)
    samples = sound.cache().data()
    # PCM writing avoids FFmpeg encoder failures at the tail of some VBR MP3s.
    with wave.open(str(out / (name + '.wav')), 'wb') as target:
        target.setnchannels(1 if mono else 2)
        target.setsampwidth(2)
        target.setframerate(48000)
        target.writeframes((np.clip(samples, -1, 32767/32768)*32768).astype('<i2').tobytes())
    pcm = np.clip(samples, -1, 32767/32768)
    peak = float(np.max(np.abs(pcm)))
    rms = float(np.sqrt(np.mean(np.square(pcm))))
    report.append(dict(asset=name, source=str(source.relative_to(root)), seconds=len(samples)/48000,
                       channels=samples.shape[1], peak=peak, rms=rms))
    print(name, report[-1])

# The two mechanical recordings are reserved assets, never weapon/impact sources.
for entry in report:
    entry['reserved_enemy_only']=entry['asset'] in ['Enemy_Metal','Enemy_Glitch']
(out/'manifest.json').write_text(json.dumps(report, indent=2))
print('AUDIO_DECODE_PASS', len(report))
