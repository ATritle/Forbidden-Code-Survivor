"""Convert existing approved Windows art/audio for browser publication; no paid generation."""
from pathlib import Path
from PIL import Image
import json, runpy, shutil, hashlib

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Web/assets'
helpers=runpy.run_path(str(ROOT/'Scripts/prepare_browser_assets.py'))
records=[]

def record(source,target,kind,enemy_only=False):
    records.append(dict(source=str(source.relative_to(ROOT)).replace('\\','/'),path=str(target.relative_to(ROOT/'Web')).replace('\\','/'),kind=kind,enemyOnly=enemy_only,bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))

for name in ['riders','music','audio']:(OUT/name).mkdir(parents=True,exist_ok=True)
for ident in ['NeonExorcist','Vex','Rook','Zero','Mira']:
    source=ROOT/f'ContentSource/UI/LoadoutWorkbench/Rider-{ident}-v1.png'
    target=OUT/f'riders/{ident}.webp';im=Image.open(source);im.thumbnail((512,768));im.save(target,quality=88);record(source,target,'portrait')
    if ident!='NeonExorcist':
        helpers['textures'](ident,ROOT/f'ContentSource/Meshy/{ident}/model_textures',768)
        helpers['textures'](ident+'Board',ROOT/f'ContentSource/Meshy/{ident}/board_textures',512)
for ident in ['DigitalSpirit','Shopfront','ServiceCabinet']:
    helpers['textures'](ident,ROOT/f'ContentSource/WindowsOverhaul/{ident}/model_textures',512)
helpers['textures']('ChallengeSign',ROOT/'ContentSource/City/Meshy/ChallengeSign/model_textures',512)
for source in (ROOT/'ContentSource/UI/SignalDock').glob('*.png'):
    target=OUT/'music'/source.with_suffix('.webp').name;Image.open(source).save(target,quality=92);record(source,target,'music-art')
source=ROOT/'ContentSource/UI/SignalDock/SignalControls.png';im=Image.open(source)
for name,x,y,w,h in [('previous',148,185,194,190),('play',523,135,285,285),('pause',967,135,288,284),('next',1436,187,196,186),('speaker',125,571,240,200),('muted',548,541,246,248),('tracks',847,552,519,228)]:
    target=OUT/f'music/{name}.webp';im.crop((x,y,x+w,y+h)).save(target,quality=94);record(source,target,'music-control')
source=ROOT/'ContentSource/UI/SignalDock/SignalFrame.png';target=OUT/'music/dock.webp';Image.open(source).crop((26,238,2147,453)).save(target,quality=94);record(source,target,'music-frame')
for entry in json.loads((ROOT/'ContentSource/Audio/manifest.json').read_text(encoding='utf-8')):
    if entry['asset'].startswith(('Music_','UI_')) or entry['asset'] in ['Enemy_Metal','Enemy_Glitch']:
        source=ROOT/entry['source'];target=OUT/'audio'/(entry['asset']+'.mp3');shutil.copy2(source,target);record(source,target,'audio',entry.get('reserved_enemy_only',False))
(OUT/'parity-manifest.json').write_text(json.dumps({'nativeVersion':'0.4.7','assets':records},indent=2),encoding='utf-8')
print('APPROVED_BROWSER_PARITY_ART_READY',len(records))
