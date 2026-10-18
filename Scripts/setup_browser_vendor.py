"""Vendor pinned MIT-licensed Three.js from the public npm registry; no runtime CDN dependency."""
from pathlib import Path
import urllib.request,tarfile,io,json,hashlib,base64
root=Path(__file__).resolve().parents[1]
version='0.186.1'
meta=json.load(urllib.request.urlopen('https://registry.npmjs.org/three/'+version))
data=urllib.request.urlopen(meta['dist']['tarball']).read()
integrity='sha512-'+base64.b64encode(hashlib.sha512(data).digest()).decode()
if integrity!=meta['dist']['integrity']:raise RuntimeError('Three.js archive integrity mismatch')
destination=root/'Web/vendor/three';destination.mkdir(parents=True,exist_ok=True)
with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as archive:
    for entry in archive.getmembers():
        relative=Path(entry.name).relative_to('package')
        if not entry.isfile() or not (str(relative).replace('\\','/').startswith('examples/jsm/') or str(relative).replace('\\','/') in ['build/three.module.js','build/three.core.js','LICENSE']):continue
        target=(destination/relative).resolve()
        if not target.is_relative_to(destination.resolve()):raise RuntimeError('Unsafe archive member')
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.extractfile(entry).read())
(destination/'version.json').write_text(json.dumps({'name':'three','version':version,'integrity':integrity,'license':'MIT'},indent=2))
print('BROWSER_VENDOR_READY three@'+version)
