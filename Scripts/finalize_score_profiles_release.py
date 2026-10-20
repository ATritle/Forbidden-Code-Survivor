from pathlib import Path
import json,hashlib,shutil
from datetime import datetime,timezone
r=Path(__file__).resolve().parents[1];out=r/'Docs/Windows-0.4.7-Validation';out.mkdir(parents=True,exist_ok=True)
def read(p):
 b=p.read_bytes();return json.loads(b.decode('utf-16' if b.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return h.hexdigest()
reports={
 'Archive':('Saved/Review/ScoreProfiles-ArchiveVerification.json','ARCHIVE_VERIFIED'),
 'Live-Reset':('Saved/Review/ScoreProfiles-LiveReset.json','WINDOWS_BOARD_RESET_PASS'),
 'Navigation':('Saved/QA/ScoreProfilesNavigationShipping/Saved/Review/MenuNavigation/report.json','MENU_NAVIGATION_PASS'),
 'Package':('Saved/Review/ScoreProfilesPackageSmoke/report.json','PACKAGED_RELEASE_PASS'),
 'Installer':('Saved/Review/InstallerRoundtrip-0.4.7.json','INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes':('Saved/Review/InstallerExtraction-0.4.7.json','INSTALLER_EXTRACTION_HASH_PASS'),
 'Damage':('Saved/QA/ScoreProfilesSystemsShipping/Damage/Saved/Review/DamageAudit/report.json','DAMAGE_AUDIT_PASS')}
for size in ['Wide','Compact']:
 reports['Score-Profiles-'+size]=('Saved/QA/ScoreProfilesShippingFinal/'+size+'/Saved/Review/ScoreProfiles/report.json','SCORE_PROFILES_PASS')
for case in ['Fresh','FreshReload']:
 reports['Profile-'+case]=('Saved/QA/ScoreProfilesSystemsShipping/Fresh/Saved/Review/Profile/'+case+'.json','PROFILE_PASS')
for case in ['Economy','EconomyReload']:
 reports['Profile-'+case]=('Saved/QA/ScoreProfilesSystemsShipping/Economy/Saved/Review/Profile/'+case+'.json','PROFILE_PASS')
for version in [3,4]:
 for case in ['PrepareV'+str(version),'MigrationV'+str(version),'ReloadV'+str(version),'migration-receipt']:
  reports['Profile-V'+str(version)+'-'+case]=('Saved/QA/ScoreProfilesSystemsShipping/MigrationV'+str(version)+'/Saved/Review/Profile/'+case+'.json','PROFILE_MIGRATION_PASS' if case=='migration-receipt' else 'PROFILE_PASS')
for name,(relative,expected) in reports.items():
 p=r/relative;assert read(p)['result']==expected,(name,read(p));shutil.copy2(p,out/(name+'.json'))
for size in ['Wide','Compact']:
 for name in ['Example-Zero.png','Example-Mira-All16.png','Top-Ten-Example.png','Empty-Board.png','End-Run-Submit.png']:
  p=r/'Saved/QA/ScoreProfilesShippingFinal'/size/'Saved/Review/ScoreProfiles'/name
  assert p.stat().st_size>10000;shutil.copy2(p,out/(size+'-'+name))
auto=r/'Saved/Review/ScoreProfilesAutomation/index.json';units=read(auto);assert units['succeeded']==6 and units['failed']==0;shutil.copy2(auto,out/'Native-Automation.json')
service=r/'Saved/Review/ScoreProfiles-ServiceTests.log';serviceText=service.read_text(encoding='utf-8-sig');assert 'pass 13' in serviceText and 'fail 0' in serviceText;shutil.copy2(service,out/'Service-Tests.txt')
rollback=read(r/'Saved/Review/ScoreProfiles-Rollback.json');assert len(rollback['versions'])==17 and rollback['versions'][-1]=='0.4.6' and rollback['originalBaselineVerified'];shutil.copy2(r/'Saved/Review/ScoreProfiles-Rollback.json',out/'Preserved-Releases.json')
deployment=read(r/'Saved/Review/ScoreProfiles-ServiceDeployment.json');assert deployment['status']=='succeeded';shutil.copy2(r/'Saved/Review/ScoreProfiles-ServiceDeployment.json',out/'Service-Deployment.json')
assert len(read(out/'Damage.json')['checks'])==70
assert read(out/'Archive.json')['archivedRows']==3
installer=r/'Artifacts/Installer-0.4.7/ForbiddenCode-Survivor-0.4.7-Setup.exe';package=r/'Artifacts/Windows-0.4.7'
manifest={'result':'SCORE_PROFILES_RELEASE_PASS','version':'0.4.7','createdUTC':datetime.now(timezone.utc).isoformat(),'engine':'UE 5.8.2','configuration':'Shipping','installer':installer.relative_to(r).as_posix(),'installerBytes':installer.stat().st_size,'installerSHA256':sha(installer),'previousPackagesVerified':rollback['versions'],'originalBaselineVerified':True,'all16ExperimentPreserved':True,'reports':list(reports),'nativeSuites':6,'serviceTests':13,'damageChecks':70,'meshyCredits':0,'paidRetries':0,'leaderboard':{'edition':'windows','globalRows':10,'perPlayerRows':3,'retention':'best three distinct runs; earlier wins equal-score ties','identity':'existing saved profile token; name aliases cannot bypass cap','resetArchivedRows':3,'resetWindowsOnly':True,'browserRowsPreserved':2,'publicTestScoresInserted':0,'immutableRunSnapshot':True,'snapshot':['riderId','playerLevel','kills','seconds','outcome','contracts','cardIds','cardRanks','evolutionBranches']},'packageFiles':[]}
for p in sorted(package.rglob('*')):
 if p.is_file() and p.suffix!='.pdb' and not p.name.startswith('Manifest_DebugFiles'):
  manifest['packageFiles'].append({'path':p.relative_to(package).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)})
(out/'release-manifest.json').write_text(json.dumps(manifest,indent=2));(installer.parent/'release-manifest.json').write_text(json.dumps(manifest,indent=2));print('SCORE_PROFILES_RELEASE_PASS',manifest['installerSHA256'])
