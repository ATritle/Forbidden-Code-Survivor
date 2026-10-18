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
    'Music_Menu': ('music', 'monume*', False),
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
for name, (folder, pattern, mono) in sources.items():
    source = next((root / 'SFX' / folder).glob(pattern + '.mp3'))
    sound = aud.Sound(str(source)).volume(0.85)
    sound.write(str(out / (name + '.wav')), rate=48000, channels=1 if mono else 2,
                format=aud.FORMAT_S16, container=aud.CONTAINER_WAV, codec=aud.CODEC_PCM)
    samples = aud.Sound(str(out / (name + '.wav'))).cache().data()
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(np.square(samples))))
    report.append(dict(asset=name, source=str(source.relative_to(root)), seconds=len(samples)/48000,
                       channels=samples.shape[1], peak=peak, rms=rms))
    print(name, report[-1])

# The two mechanical recordings are reserved assets, never weapon/impact sources.
for entry in report:
    entry['reserved_enemy_only']=entry['asset'] in ['Enemy_Metal','Enemy_Glitch']
(out/'manifest.json').write_text(json.dumps(report, indent=2))
print('AUDIO_DECODE_PASS', len(report))
