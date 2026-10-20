"""Copy the reviewable native edition into the existing clean publishing checkout.
No deletions, Git operations, browser files, build caches, credentials, or live service data.
"""
from pathlib import Path
import hashlib
import shutil

root=Path(__file__).resolve().parents[1]
destination=root/'Artifacts/GitHubPublish'
if not (destination/'.git').exists():raise RuntimeError('Expected publishing checkout is missing')
blocked={'.git','.codex','.agents','Binaries','Intermediate','Saved','DerivedDataCache','node_modules','.sites-runtime','dist','__pycache__'}
folders=['Source','Plugins','Config','Content','ContentSource','Docs','GameData','Packaging','Scripts','SFX','Services/Leaderboard']
files=[]
for folder in folders:
    source=root/folder
    for path in source.rglob('*'):
        if not path.is_file() or any(part in blocked for part in path.relative_to(source).parts):continue
        if path.name in {'meshy-downloads.json','.env','.env.local'} or path.suffix in {'.pyc','.db','.sqlite','.sqlite3'}:continue
        if path.suffix.startswith('.blend') and path.suffix!='.blend':continue
        files.append(path)
files += [root/name for name in ['DeckBuilder.uproject','Play-Forbidden-Routines.cmd','Play-All-16-Cards.cmd','Play-Windows-0.3.0.cmd','Play-Windows-0.3.1.cmd','Play-Windows-0.3.2.cmd','Play-Windows-0.3.3.cmd','Play-Windows-0.3.4.cmd','Play-Windows-0.3.5.cmd','Play-Windows-0.3.6.cmd','Play-Windows-0.3.7.cmd','Play-Windows-0.3.8.cmd','Play-Windows-0.3.9.cmd','Play-Windows-0.4.0.cmd','Play-Windows-0.4.1.cmd','Play-Windows-0.4.2.cmd','Play-Windows-0.4.3.cmd', 'Play-Windows-0.4.4.cmd','Play-Windows-0.4.5.cmd','Play-Windows-0.4.6.cmd','Play-All-16-Cards-0.4.5.cmd','Play-Original-Windows.cmd','Restore-Original-Windows.cmd']]
changed=0
for path in files:
    target=destination/path.relative_to(root)
    if target.exists() and target.stat().st_size==path.stat().st_size and hashlib.sha256(target.read_bytes()).digest()==hashlib.sha256(path.read_bytes()).digest():continue
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target);changed+=1
print(f'NATIVE_REVIEW_CHECKOUT_SYNCED {changed} changed files; browser source untouched')
