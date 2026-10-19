"""Package existing card/enemy art and real game stills for the illustrated field guide."""
from pathlib import Path
import shutil, hashlib, json
import unreal
root=Path(unreal.Paths.project_dir()); source=root/'ContentSource/UI/Help'; source.mkdir(parents=True,exist_ok=True)
captures=root/'Saved/QA/RampGuide/Saved/Review/RampGuide'
images={
 'Combat':root/'Saved/Review/OverhaulFunctional/02-Evolved-Combat.png',
 'Draft':root/'Saved/Review/OverhaulFunctional/03-Draft.png',
 'Warning':root/'Saved/Screenshots/LowHealth-SilentWarning.png',
 'Menu':root/'Saved/Review/OverhaulFunctional/01-Menu.png',
 'Sign':root/'Saved/Screenshots/Challenge-Sign.png',
 'Settings':root/'Saved/Screenshots/Settings-ImpactIntensity.png',
 'Testing':root/'Saved/Screenshots/TestingMulti-AllAttacks.png',
 'Leaderboard':root/'Saved/QA/PackagedSystems/Saved/Review/OverhaulSystems/WindowsLeaderboard.png',
 'Ramp':root/'Saved/Review/OverhaulFunctional/Route-1.png',
 'Relay':root/'Saved/Review/OverhaulFunctional/Route-1.png',
 'Cache':root/'Saved/Review/OverhaulFunctional/Route-3.png',
 'Clinic':root/'Saved/Screenshots/StreetLighting-Plaza-After.png',
 'Ward':root/'Saved/Review/OverhaulFunctional/Route-2.png',
 'Subway':root/'Saved/Screenshots/NeonCity-Subway.png',
 'Spirit':root/'Saved/Screenshots/NeonEnergy-Combat.png',
}
for name in ['OverloadOrb','UnstableEcho','HostileBroadcast','DeadmanBattery','GlassProtocol','JackpotCourier','LockdownDuel']:
    filename='Broadcast' if name=='HostileBroadcast' else name
    images[name]=root/'Saved/Screenshots'/('Challenge-'+filename+'.png')
for path in (root/'ContentSource/Enemies/CyberRoster').glob('*.png'): images[path.stem]=path
menu_still=root/'Saved/QA/MenuAudio/Saved/Review/MenuAudio/MainMenu.png'
if menu_still.exists():images['Menu']=menu_still
if '-HelpMenuOnly' in unreal.SystemLibrary.get_command_line():images={'Menu':images['Menu']}
tools=unreal.AssetToolsHelpers.get_asset_tools()
report={entry['id']:entry for entry in json.loads((source/'provenance.json').read_text())} if (source/'provenance.json').exists() else {}
for name,original in images.items():
    # A refreshed still is preferred after the native scene-capture pass.
    fresh=captures/(name+'.png'); original=fresh if fresh.exists() and not (name=='Menu' and menu_still.exists()) else original
    if not original.exists(): original=source/(name+'.png')
    assert original.exists(), original
    dest=source/(name+'.png')
    if original.resolve()!=dest.resolve():shutil.copy2(original,dest)
    task=unreal.AssetImportTask();task.filename=str(dest);task.destination_path='/Game/UI/Help';task.destination_name='T_'+name
    task.automated=True;task.replace_existing=True;task.save=True;tools.import_asset_tasks([task])
    texture=unreal.load_asset('/Game/UI/Help/T_'+name);assert texture
    texture.set_editor_property('max_texture_size',1536)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb',True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
    report[name]={'id':name,'source':str(original.relative_to(root)),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
(source/'provenance.json').write_text(json.dumps(list(report.values()),indent=2))
unreal.log('HELP_ART_PASS: '+str(len(report))+' existing illustrations/stills; no paid generation')
