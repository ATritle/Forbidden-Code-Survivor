"""Collect real playback/installer receipts and verify preserved Windows releases."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
out = root/'Docs/Windows-0.4.6-Validation'
out.mkdir(parents=True, exist_ok=True)

def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4*1024*1024), b''):
            value.update(chunk)
    return value.hexdigest()

reports = {
 'Guide': ('Saved/QA/RarityEconomyGuideShipping/Saved/Review/RampGuide/report.json','RAMP_GUIDE_PASS'),
 'Quit-MainMenu': ('Saved/QA/RarityEconomyGuideShipping/Saved/Review/RampGuide/QuitMenuSmoke-report.json','CLOSE_GAME_PASS'),
 'Quit-Settings': ('Saved/QA/RarityEconomyGuideShipping/Saved/Review/RampGuide/QuitSettingsSmoke-report.json','CLOSE_GAME_PASS'),
 'MenuAudio': ('Saved/QA/RarityEconomyAudioShipping/Saved/Review/MenuAudio/report.json','MENU_AUDIO_PASS'),
 'Package': ('Saved/Review/RarityEconomyPackageSmoke/report.json','PACKAGED_RELEASE_PASS'),
 'Installer': ('Saved/Review/InstallerRoundtrip-0.4.6.json','INSTALLER_ROUNDTRIP_PASS'),
 'Installer-Hashes': ('Saved/Review/InstallerExtraction-0.4.6.json','INSTALLER_EXTRACTION_HASH_PASS')}
for size in ['Wide','Compact','Large']:
 for file in ['report','reload']:
  reports['Workbench-'+size+'-'+file]=('Saved/QA/RarityEconomyShipping/'+size+'/Saved/Review/Workbench/'+file+'.json','WORKBENCH_PASS')
 reports['Hover-'+size]=('Saved/Review/RarityEconomyHover-'+size+'.json','WORKBENCH_HOVER_PASS')
for size in ['Wide','Compact']:
 reports['Navigation-'+size]=('Saved/QA/RarityEconomyNavigationShipping/'+size+'/Saved/Review/MenuNavigation/report.json','MENU_NAVIGATION_PASS')
 reports['Settings-'+size]=('Saved/QA/RarityEconomySettingsShipping/'+size+'/Saved/Review/SettingsReview/report.json','SETTINGS_REVIEW_PASS')
reports['Damage'] = ('Saved/QA/RarityEconomySystemsShipping/Damage/Saved/Review/DamageAudit/report.json','DAMAGE_AUDIT_PASS')
for case in ['Fresh','FreshReload']:
 reports['Profile-'+case]=('Saved/QA/RarityEconomySystemsShipping/Fresh/Saved/Review/Profile/'+case+'.json','PROFILE_PASS')
for case in ['Economy','EconomyReload']:
 reports['Profile-'+case]=('Saved/QA/RarityEconomySystemsShipping/Economy/Saved/Review/Profile/'+case+'.json','PROFILE_PASS')
for version in [3,4]:
 for case in ['PrepareV'+str(version),'MigrationV'+str(version),'ReloadV'+str(version),'migration-receipt']:
  reports['Profile-V'+str(version)+'-'+case]=('Saved/QA/RarityEconomySystemsShipping/MigrationV'+str(version)+'/Saved/Review/Profile/'+case+'.json','PROFILE_MIGRATION_PASS' if case=='migration-receipt' else 'PROFILE_PASS')
for size in ['Wide','Compact']:
 for mode in ['Normal','Expanded']:
  reports['All16-'+size+'-'+mode]=('Saved/QA/RarityEconomyAll16Shipping/'+size+'/'+mode+'/Saved/Review/All16/report.json','ALL16_PASS')
for name,(relative,expected) in reports.items():
 path=root/relative;report=json.loads(path.read_text(encoding='utf-8-sig'))
 assert report['result']==expected,(name,report)
 shutil.copy2(path,out/(name+'.json'))
for size in ['Wide','Compact','Large']:
 for name in ['Workbench-Deck.png','Workbench-Library.png','Workbench-Saved.png','Workbench-Card-Hover.png','Workbench-Rider-Hover.png','Workbench-Unlock.png']:
  capture=root/'Saved/QA/RarityEconomyShipping'/size/'Saved/Review/Workbench'/name
  assert capture.stat().st_size>10000
  shutil.copy2(capture,out/(size+'-'+name))
for size in ['Wide','Compact']:
 for name in ['Menu.png','Tooltip-00.png','Tooltip-07.png','Tooltip-08.png','Tooltip-15.png','Deferred-Draft.png']:
  shutil.copy2(root/'Saved/QA/RarityEconomyNavigationShipping'/size/'Saved/Review/MenuNavigation'/name,out/(size+'-'+name))
ledger=json.loads((root/'ContentSource/Meshy/AlternateRiders/submitted-jobs.json').read_text())
assert ledger['spentCredits']==248 and ledger['paidRetries']==0

rollback=json.loads((root/'Saved/Review/RarityEconomy-Rollback.json').read_text())
assert rollback['result']=='PRESERVED_RELEASES_PASS' and rollback['originalBaselineVerified']
preserved=rollback['versions'];assert len(preserved)==16 and preserved[-1]=='0.4.5' and rollback['all16ExperimentPreserved']
shutil.copy2(root/'Saved/Review/RarityEconomy-Rollback.json',out/'Preserved-Releases.json')

installer = root/'Artifacts/Installer-0.4.6/ForbiddenCode-Survivor-0.4.6-Setup.exe'
package = root/'Artifacts/Windows-0.4.6'
if not (package/'ForbiddenCodeSurvivor.exe').exists():
    package = package/'Windows'
manifest = {
    'result': 'RIDER_ECONOMY_RELEASE_PASS', 'version': '0.4.6',
    'createdUTC': datetime.now(timezone.utc).isoformat(),
    'configuration': 'Shipping', 'engine': 'UE 5.8.2',
    'installer': installer.relative_to(root).as_posix(),
    'installerBytes': installer.stat().st_size, 'installerSHA256': sha(installer),
    'previousPackagesVerified': preserved, 'originalBaselineVerified': True,
    'meshyCredits':0, 'imagegenPortraits':0, 'paidRetries':0, 'reports': list(reports), 'musicSources': [], 'packageFiles': [], 'all16ExperimentPreserved':True}
from PIL import Image
unitPath=root/'Saved/Review/RarityEconomyAutomation/index.json'
unit=json.loads(unitPath.read_text(encoding='utf-16' if unitPath.read_bytes().startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'))
assert unit['succeeded']==5 and unit['failed']==0 and unit['notRun']==0
shutil.copy2(root/'Saved/Review/RarityEconomyAutomation/index.json',out/'Native-Automation.json')
damage=json.loads((out/'Damage.json').read_text(encoding='utf-8-sig'))
assert len(damage['checks'])==70 and all(row['pass'] for row in damage['checks'])
manifest['loadouts']={'riders':5,'presetsPerRider':3,'maxStarters':2,'freshChoicesPerRider':4,'starterUnlockCosts':{'Common':75,'Rare':150,'Epic':300,'LegendaryReserved':500},'autosaveIncomplete':True,'runDraftCatalog':16,'activeCapacity':16,'choicesPerHand':3,'preRunUnlocksGateDraft':False}
manifest['profile']={'version':5,'previousVersions':[4,3],'oneWayImport':True,'oldSaveUnchanged':True,'malformedHeaderRecoveryVerified':True,'previousPurchasesPreserved':True,'riderSpecificWallets':True,'legacyBalance':'one-time transfer to last selected rider; no duplication','runAndCaptureAttribution':'run rider ID; independent of menu selection','freshProcessReloadVerified':True}
manifest['damageAudit']={'numericChecks':70,'allPassed':True,'damageTuningChanged':False,'all16TimerCombatAtTwoResolutions':True,'notClaimed':'exhaustive DPS proof for every possible multi-card/rider/contract combination'}
for size in ['Wide','Compact']:
 for mode in ['Normal','Expanded']:
  report=json.loads((out/('All16-'+size+'-'+mode+'.json')).read_text(encoding='utf-8-sig'))
  assert report['capacity']==16 and report['weapons']==16
  for image in ['All16-Combat.png','All16-Inspect.png']:
   shutil.copy2(root/'Saved/QA/RarityEconomyAll16Shipping'/size/mode/'Saved/Review/All16'/image,out/(size+'-'+mode+'-'+image))
manifest['navigation']={'back':'topmost window only; safe cancel/decline','draftEscape':'defer same hand; Tab resumes','tooltips':'full description and base stats'}
manifest['navigation']={'back':'topmost window only; safe cancel/decline actions','rootEscape':'no quit or Settings popup','draftEscape':'defer same hand, resume combat','resume':'Tab','tooltip':'complete description and base stats on hover; no grid expansion'}
manifest['compactLayout']={'logicalResolution':[1600,900],'cardSize':[210,200],'previousCardSize':[251,244],'grid':[4,2],'descriptions':'full hover details','actions':'below grid','scaleScope':'main workbench only'}
manifest['layoutReference']='ContentSource/Concepts/CombinedLoadoutMenu/A-Loadout-Workbench-v2.png'
manifest['layoutReferenceSHA256']=sha(root/manifest['layoutReference'])
manifest['riderArtwork']=[]
for rider in ['NeonExorcist','Vex','Rook','Zero','Mira']:
 source=root/'ContentSource/UI/LoadoutWorkbench'/('Rider-'+rider+'-v1.png')
 image=Image.open(source)
 assert image.mode=='RGBA' and image.size==(1024,1536) and image.getextrema()[3][0]==0
 manifest['riderArtwork'].append(dict(rider=rider,source=source.relative_to(root).as_posix(),sha256=sha(source),size=list(image.size),generation='reused built-in imagegen artwork from 0.4.1',menuAnimation='fixed pose'))
for entry in json.loads((root/'ContentSource/Audio/manifest.json').read_text()):
    if not entry['asset'].startswith('Music_'):
        continue
    source = root/entry['source']
    manifest['musicSources'].append(dict(asset=entry['asset'], source=source.relative_to(root).as_posix(),
                                        seconds=entry['seconds'], sha256=sha(source)))
for path in sorted(package.rglob('*')):
    if not path.is_file() or path.suffix == '.pdb' or path.name.startswith('Manifest_DebugFiles'):
        continue
    manifest['packageFiles'].append(dict(path=path.relative_to(package).as_posix(),
                                         bytes=path.stat().st_size, sha256=sha(path)))
(out/'release-manifest.json').write_text(json.dumps(manifest, indent=2))
(installer.parent/'release-manifest.json').write_text(json.dumps(manifest, indent=2))
print('RIDER_ECONOMY_RELEASE_PASS', manifest['installerSHA256'], 'previous releases and original baseline unchanged')
