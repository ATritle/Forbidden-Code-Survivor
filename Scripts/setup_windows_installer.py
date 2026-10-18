"""Fetch the portable, open-source NSIS compiler from its official SourceForge distribution."""
from pathlib import Path
import urllib.request,zipfile,io,hashlib,json
root=Path(__file__).resolve().parents[1];destination=root/'Tools/nsis';destination.mkdir(parents=True,exist_ok=True)
url='https://downloads.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13.zip'
data=urllib.request.urlopen(url,timeout=90).read()
with zipfile.ZipFile(io.BytesIO(data)) as archive:
    for item in archive.infolist():
        target=(destination/item.filename).resolve()
        if not target.is_relative_to(destination.resolve()):raise RuntimeError('Unsafe NSIS archive entry')
        if item.is_dir():target.mkdir(parents=True,exist_ok=True)
        else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.read(item))
(destination/'source.json').write_text(json.dumps({'url':url,'sha256':hashlib.sha256(data).hexdigest()},indent=2))
print('NSIS_COMPILER_READY '+str(destination/'nsis-3.13/makensis.exe'))
