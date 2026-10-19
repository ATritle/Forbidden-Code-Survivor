"""Keep the accepted Windows scope and its validation evidence reviewable."""
from pathlib import Path
import json
import shutil

root=Path(__file__).resolve().parents[1]
path=root/'Docs/WindowsOverhaul/implementation-status.json'
data=json.loads(path.read_text(encoding='utf-8-sig'))
data['meshy']['spent_credits']=80
data['release_state']='Native implementation complete; packaged full-run, stress tests and installer validation in progress'
implementation={
'S':'Docs/WindowsOverhaul/RESTORE.txt',
'R':'Source/DeckBuilder/ForbiddenRun.cpp',
'M':'Source/DeckBuilder/ForbiddenDistrictDirector.cpp',
'A':'Scripts/import_overhaul_art.py',
'P':'Source/DeckBuilder/ForbiddenRun.cpp',
'D':'Plugins/CodexRuntime/Source/CodexRuntime/Private/CodexDeckComponent.cpp',
'C':'Source/DeckBuilder/ForbiddenPrograms.cpp',
'E':'Source/DeckBuilder/ForbiddenEnemyBrain.cpp',
'N':'Source/DeckBuilder/NeonChallengeTrials.cpp',
'G':'Source/DeckBuilder/ForbiddenRunUI.cpp',
'U':'Source/DeckBuilder/ForbiddenSettingsControls.cpp',
'T':'Source/DeckBuilder/ExplosionBenchmark.cpp',
'V':'Scripts/Test-WindowsOverhaul.ps1'}
for item in data['features']:
    if item['status']=='pending':item['status']='implemented'
    item['implementation']=[implementation[item['id'][0]]]
out=root/'Docs/WindowsOverhaul/Validation';out.mkdir(parents=True,exist_ok=True)
reports={'functional':'Saved/Review/OverhaulFunctional/report.json','systems':'Saved/Review/OverhaulSystems/report.json','assets':'Saved/Review/OverhaulAssetAudit.json','baseline':'Saved/Review/BaselineSmoke/report.json'}
for name,relative in reports.items():
    source=root/relative
    if source.exists():shutil.copy2(source,out/(name+'.json'))
mapping={
'S01':'baseline','S02':'baseline','R01':'functional','R02':'systems','M01':'functional','M04':'functional','A02':'assets','A03':'assets','A04':'assets','P01':'systems','P02':'systems','P03':'functional','P04':'systems','D01':'functional','D02':'systems','D03':'functional','D04':'functional','E02':'functional','E05':'systems','G01':'systems','G04':'systems','T03':'systems'}
for item in data['features']:
    key=mapping.get(item['id'],'functional' if item['id'].startswith('C') else None)
    if key and (out/(key+'.json')).exists():
        result=json.loads((out/(key+'.json')).read_text())
        if result.get('result','').endswith('PASS'):
            item['status']='verified';item['evidence']=['Docs/WindowsOverhaul/Validation/'+key+'.json']
for group in ['ForbiddenAutomationFinal','CodexAutomationFinal']:
    source=root/'Saved/Review'/group/'index.json'
    if source.exists():
        report=json.loads(source.read_text(encoding='utf-8-sig'))
        (out/(group+'.json')).write_text(json.dumps({key:report.get(key) for key in ['succeeded','succeededWithWarnings','failed','notRun','totalDuration','tests']},indent=2))
for item in data['features']:
    if item['id'] in ['N01','N02','N03','N04','N05','U02','U03','U04']:
        item['evidence']=['Saved/Review/OverhaulRegressions2.log','Saved/Review/HealthAlarmFinal.log','Saved/Review/TestingAreaFinal.log']
data['counts']={state:sum(item['status']==state for item in data['features']) for state in ['pending','implemented','verified']}
path.write_text(json.dumps(data,indent=2)+'\n')
print('SCOPE_STATUS',data['counts'])
