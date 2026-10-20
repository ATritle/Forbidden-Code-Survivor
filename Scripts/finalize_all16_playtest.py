"""Collect the opt-in all-card playtest's real draft/combat receipts."""
from pathlib import Path
import hashlib,json,shutil
from datetime import datetime,timezone
r=Path(__file__).resolve().parents[1]
out=r/'Docs/All16-Playtest-Validation';out.mkdir(parents=True,exist_ok=True)
for label,folder in [('Wide','All16Shipping'),('Compact','All16ShippingCompact')]:
 for mode in ['Normal','Expanded']:
  source=r/'Saved/QA'/folder/mode/'Saved/Review/All16'
  report=json.loads((source/'report.json').read_text(encoding='utf-8-sig'))
  assert report['result']=='ALL16_PASS'
  if mode=='Expanded':
   assert report['capacity']==16 and report['weapons']==16 and len(report['damageByCard'])>=16 and all(v>0 for v in report['damageByCard'].values())
   for image in ['All16-Combat.png','All16-Inspect.png']:
    assert (source/image).stat().st_size>10000
    shutil.copy2(source/image,out/(label+'-'+image))
  else: assert report['capacity']==6
  shutil.copy2(source/'report.json',out/(label+'-'+mode+'.json'))
old=json.loads((r/'Saved/Review/All16-Regular-Preserved.json').read_text())
assert old['result']=='REGULAR_BUILD_PRESERVED'
shutil.copy2(r/'Saved/Review/All16-Regular-Preserved.json',out/'Regular-Preserved.json')
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(4*1024*1024),b''):h.update(block)
 return h.hexdigest()
package=r/'Artifacts/Windows-All16-Playtest'
manifest={'result':'ALL16_PLAYTEST_PASS','createdUTC':datetime.now(timezone.utc).isoformat(),'baseVersion':'0.4.5','launcher':'Play-All-16-Cards.cmd','flag':'-AllCardsPlaytest','startingCards':2,'draftChoices':3,'runCatalog':16,'activeCapacity':16,'automaticFullLoadout':False,'publicScoreSubmission':False,'userDirectory':'Saved/All16Playtest','regularBuildPreserved':True,'packageFiles':[]}
for p in sorted(package.rglob('*')):
 if p.is_file() and p.suffix!='.pdb' and not p.name.startswith('Manifest_DebugFiles'):
  manifest['packageFiles'].append({'path':p.relative_to(package).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)})
(out/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('ALL16_PLAYTEST_PASS: actual RNG drafts, 16 retained attacks, 16 damage sources, two screen sizes; regular build preserved')
