"""Prepare compact, original-art browser assets without altering UE/Meshy sources.

Run with a Python environment containing Pillow. The Blender companion exports
rigs and animations. Outputs are ordinary GLB/WebP/MP3, usable from static hosting.
"""
from pathlib import Path
from PIL import Image, ImageChops, ImageEnhance
import json, shutil, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Web/assets'
CACHE = ROOT / 'Saved/BrowserAssetCache'

def image(source, target, size, quality=84):
    im = Image.open(source).convert('RGB')
    im.thumbnail((size, size), Image.Resampling.LANCZOS)
    target.parent.mkdir(parents=True, exist_ok=True)
    im.save(target, quality=quality, optimize=True)

def textures(asset, source, size=512):
    folder = CACHE / asset
    folder.mkdir(parents=True, exist_ok=True)
    for channel in ['base_color', 'normal']:
        image(source / (channel + '.png'), folder / (channel + '.jpg'), size)
    im = Image.open(source / 'base_color.png').convert('RGB')
    im.thumbnail((size, size), Image.Resampling.LANCZOS)
    pixels = list(im.getdata())
    def mask(c):
        hi, lo = max(c), min(c)
        strength = max(0, min(1, (hi / 255 - .40) / .38)) * max(0, min(1, ((hi - lo) / 255 - .20) / .35))
        return tuple(round(v * strength) for v in c)
    im.putdata([mask(c) for c in pixels])
    im.save(folder / 'emission.jpg', quality=88, optimize=True)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    roster = json.loads((ROOT / 'ContentSource/Enemies/CyberRoster/roster.json').read_text())
    for entry in roster:
        textures(entry['id'], ROOT / 'ContentSource/Meshy' / entry['id'] / 'model_textures')
    textures('NeonExorcist', ROOT / 'ContentSource/Meshy/NeonExorcist/rider_textures', 1024)
    textures('NeonHoverboard', ROOT / 'ContentSource/Meshy/NeonExorcist/board_textures')
    for name in ['ShoppingBlock', 'MarketKiosk', 'RooftopShrine']:
        textures(name, ROOT / 'ContentSource/City/Meshy' / name / 'model_textures')
    art = OUT / 'art'
    art.mkdir(parents=True, exist_ok=True)
    images = []
    for source in sorted((ROOT / 'ContentSource/UI/NeonEnergy').glob('*.png')) + sorted((ROOT / 'ContentSource/UI/NeonArsenal').glob('*.png')):
        if 'Atlas' in source.stem:
            continue
        target = art / (source.stem + '.webp')
        image(source, target, 512)
        images.append({'id': source.stem, 'path': str(target.relative_to(ROOT / 'Web')).replace('\\', '/'), 'source': str(source.relative_to(ROOT)).replace('\\', '/'), 'bytes': target.stat().st_size})
    source = ROOT / 'ContentSource/UI/NeonGlass/NeonDistrict-Menu.png'
    target = art / 'NeonDistrict-Menu.webp'
    image(source, target, 1920, 88)
    images.append({'id': 'NeonDistrict-Menu', 'path': 'assets/art/NeonDistrict-Menu.webp', 'source': str(source.relative_to(ROOT)).replace('\\', '/'), 'bytes': target.stat().st_size})
    # Preserve the user's exact approved attack/enemy mapping from the UE audio manifest.
    audio = OUT / 'audio'
    audio.mkdir(exist_ok=True)
    sounds = []
    for entry in json.loads((ROOT / 'ContentSource/Audio/manifest.json').read_text()):
        if entry.get('reserved_enemy_only'):
            continue
        source = ROOT / entry['source'].replace('\\', '/')
        if source.suffix.lower() == '.mp3' and source.exists():
            name={'Music_Menu':'MenuMusic','Music_Gameplay':'GameplayMusic'}.get(entry['asset'],entry['asset'])
            target = audio / (name + '.mp3')
            shutil.copy2(source, target)
            sounds.append({'id': name, 'path': 'assets/audio/' + target.name, 'seconds': entry['seconds'], 'bytes': target.stat().st_size, 'source': entry['source']})
    (OUT / 'art-manifest.json').write_text(json.dumps({'images': images, 'audio': sounds}, indent=2))
    arguments = ['--', '--environment-only'] if '--environment-only' in sys.argv else []
    result = subprocess.run([r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe', '--background', '--python', str(ROOT / 'Scripts/export_browser_models.py')] + arguments, cwd=ROOT)
    if result.returncode:
        raise SystemExit(result.returncode)
    print('BROWSER_ASSETS_READY', flush=True)

if __name__ == '__main__':
    main()
