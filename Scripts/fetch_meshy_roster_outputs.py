"""Download FBX URLs supplied by Meshy's connector; never discovers or calls task APIs."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from urllib.request import urlopen
from urllib.parse import urlparse
import json
import sys
import shutil

ROOT = Path(__file__).resolve().parents[1]
jobs = json.loads((ROOT / (sys.argv[1] if len(sys.argv) > 1 else 'Saved/Review/Roster-DownloadJobs.json')).read_text(encoding='utf-8-sig'))

def download(job):
    destination = (ROOT / job['path']).resolve()
    destination.relative_to((ROOT / 'ContentSource/Meshy').resolve())
    parsed = urlparse(job['url'])
    if parsed.scheme != 'https' or not parsed.hostname.endswith('.meshy.ai'):
        raise ValueError('Unexpected asset host')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.stat().st_size < 1024:
        temporary = destination.with_suffix(destination.suffix + '.download')
        with urlopen(job['url'], timeout=180) as source, temporary.open('wb') as output:
            shutil.copyfileobj(source, output, 1024 * 1024)
        if temporary.stat().st_size < 1024:
            raise RuntimeError('Empty asset: ' + destination.name)
        temporary.replace(destination)
    print('FBX_READY', destination.relative_to(ROOT).as_posix(), destination.stat().st_size, flush=True)

with ThreadPoolExecutor(max_workers=4) as pool:
    list(pool.map(download, jobs))
