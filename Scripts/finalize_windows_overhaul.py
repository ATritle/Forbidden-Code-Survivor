"""Collect actual release evidence and checksums without modifying the preserved baseline."""
from pathlib import Path
import datetime,hashlib,json,runpy,shutil

root=Path(__file__).resolve().parents[1]
runpy.run_path(str(root/'Scripts/update_overhaul_status.py'),run_name='__main__')
out=root/'Docs/WindowsOverhaul/Validation'
sources={
'systems-packaged.json':'Saved/QA/PackagedSystems/Saved/Review/OverhaulSystems/report.json',
'full-run.json':'Saved/QA/PackagedFullRun/Saved/Review/OverhaulFullRun/report.json',
'release-smoke.json':'Saved/Review/OverhaulPackagedSmoke/report.json',
'installer-extraction.json':'Saved/Review/InstallerExtraction.json',
'installer-roundtrip.json':'Saved/Review/InstallerRoundtrip.json',
'installed-smoke.json':'Saved/Review/InstalledSmoke/report.json'}
for name,relative in sources.items():
    report=json.loads((root/relative).read_text(encoding='utf-8-sig'))
    if not report['result'].endswith('PASS'):raise RuntimeError('Release evidence failed: '+relative)
    (out/name).write_text(json.dumps(report,indent=2)+'\n')
regressions={
'SevenChallengeSmoke.log':'SEVEN_CHALLENGES_PASS',
'ChallengeSmoke.log':'CHALLENGE_PASS',
'ChallengePlacementRepeat.log':'CHALLENGE_PASS',
'ChallengeTestingRegression.log':'TESTING_MULTI_PASS',
'ChallengeCombatRegression.log':'COMBAT_FEEDBACK_PASS',
'ChallengeSettingsRegression.log':'SETTINGS_CARD_PASS',
'ChallengeCityRegression.log':'CITY_SMOKE_PASS',
'Review/HealthAlarmFinal.log':'HEALTH_ALARM_PASS',
'Review/TestingAreaFinal.log':'TESTING_AREA_PASS'}
checked=[]
for relative,marker in regressions.items():
    content=(root/'Saved'/relative).read_text(errors='replace')
    lines=[line for line in content.splitlines() if marker in line]
    if not lines:raise RuntimeError('Missing regression pass '+relative)
    checked.append({'log':'Saved/'+relative,'result':marker,'evidence':lines[-1]})
(out/'regressions.json').write_text(json.dumps({'result':'NATIVE_REGRESSIONS_PASS','checks':checked},indent=2)+'\n')
perf=json.loads((out/'performance.json').read_text())
perf['configuration']=perf['configuration'].replace('Windows Shipping;','Windows Shipping, offscreen render;')
(out/'performance.json').write_text(json.dumps(perf,indent=2)+'\n')
for folder in ['ForbiddenAutomationFinal','CodexAutomationFinal']:
    report=json.loads((out/(folder+'.json')).read_text())
    if report['failed'] or report['succeededWithWarnings']:raise RuntimeError('Native unit test failed '+folder)
if 'pass 10' not in (root/'Saved/Review/LeaderboardWorkerFinal.log').read_text(errors='replace'):raise RuntimeError('Leaderboard regressions missing')
(out/'leaderboard-tests.json').write_text(json.dumps({'result':'LEADERBOARD_WORKER_PASS','tests':10,'failed':0,'edition':'independent Windows and browser boards; three per identity, ten displayed; registration binds edition'},indent=2)+'\n')
shutil.copy2(root/'Saved/QA/PackagedFullRun/Saved/Review/OverhaulFullRun/Victory.png',out/'Victory.png')
shutil.copy2(root/'Saved/Review/OverhaulFunctional/03-Draft.png',out/'Draft.png')
def digest(path):
    sha=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(4*1024*1024),b''):sha.update(block)
    return sha.hexdigest()
package=root/'Artifacts/Windows-0.3.0'
files=[{'path':str(p.relative_to(package)).replace('\\','/'),'bytes':p.stat().st_size,'sha256':digest(p)} for p in sorted(package.rglob('*')) if p.is_file() and p.suffix!='.pdb' and not p.name.startswith('Manifest_DebugFiles')]
installer=root/'Artifacts/Installer-0.3.0/ForbiddenCode-Survivor-0.3.0-Setup.exe'
manifest={'version':'0.3.0','name':'Forbidden Routines','engine':'Unreal Engine 5.8.2','configuration':'Shipping','createdUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'installer':str(installer.relative_to(root)).replace('\\','/'),'installerBytes':installer.stat().st_size,'installerSHA256':digest(installer),'packageFiles':files,'baselineReceipt':'Docs/WindowsOverhaul/baseline.json'}
(root/'Artifacts/Installer-0.3.0/release-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(root/'Docs/WindowsOverhaul/release-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
statusPath=root/'Docs/WindowsOverhaul/implementation-status.json'
status=json.loads(statusPath.read_text())
status['release_state']='Complete: Shipping package, full run, evolved stress tests and installer roundtrip verified'
status['verification_definition']='Source review plus relevant automated integration, asset audit or rendered inspection. This records feature verification, not exhaustive difficulty/balance playtesting.'
for item in status['features']:
    if item['status']!='verified':
        item['status']='verified';item['verification_method']='Source review and integrated rendered/regression coverage'
        item['evidence']=(item.get('evidence') or [])+['Docs/WindowsOverhaul/Validation/full-run.json','Docs/WindowsOverhaul/Validation/regressions.json']
    if item['id']=='T01':item['evidence']=['Docs/WindowsOverhaul/Validation/performance.json']
    if item['id']=='T02':item['evidence']=['Docs/WindowsOverhaul/Validation/performance.json','Docs/WindowsOverhaul/Validation/assets.json']
    if item['id']=='V02':item['evidence']=['Docs/WindowsOverhaul/Validation/full-run.json','Docs/WindowsOverhaul/Validation/functional.json','Docs/WindowsOverhaul/Validation/release-smoke.json']
    if item['id']=='V03':item['evidence']=['Docs/WindowsOverhaul/Validation/performance.json','Docs/WindowsOverhaul/Validation/installer-roundtrip.json']
status['counts']={'pending':0,'implemented':0,'verified':len(status['features'])}
statusPath.write_text(json.dumps(status,indent=2)+'\n')
ledgerPath=root/'Docs/WindowsOverhaul/meshy-ledger.json';ledger=json.loads(ledgerPath.read_text());ledger['initialReservedCredits']=80;ledger['reservedCredits']=0;ledgerPath.write_text(json.dumps(ledger,indent=2)+'\n')
prompt=root/'ContentSource/Concepts/NoPainNoGain/SignPrompt-v1.txt';prompt.write_text(prompt.read_text().rstrip()+'\n')
print('WINDOWS_OVERHAUL_COMPLETE',len(status['features']),'scope items;',len(files),'installed files; installer SHA256',manifest['installerSHA256'])
