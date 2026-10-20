"""Export the UE edition's authored tuning into a versioned browser rules snapshot."""
from pathlib import Path
import re,json,hashlib,sys
root=Path(__file__).resolve().parents[1]
pinned=root/'GameData/browser-native-source.json'
if pinned.exists() and '--from-native' not in sys.argv:
    rules=json.loads(pinned.read_text(encoding='utf-8'))
    assert rules['schemaVersion']==2 and len(rules['cards'])==16 and len(rules['riders'])==5
    assert all(len(c['evolutions'])==2 for c in rules['cards'])
    for folder in ['GameData','Web/data']:
        output=root/folder;output.mkdir(parents=True,exist_ok=True)
        (output/'gameplay.json').write_text(json.dumps(rules,indent=2),encoding='utf-8')
    print('PINNED_NATIVE_RULES_EXPORTED Windows='+rules['nativeVersion']+' Browser='+rules['browserVersion'])
    sys.exit(0)

def read(name):return (root/'Source/DeckBuilder'/name).read_text(encoding='utf-8-sig')
def text(value):return json.loads('"'+value+'"')
def number(value):return float(value.rstrip('f'))
cards=[]
pattern=r'AddCard\(TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("((?:\\.|[^"\\])*)"\),EMagicSchool::(\w+),([\d.]+)f,([\d.]+)f,'
modes=['bolt','spread','nova','card','pierce','guard']
colors=['#36ff93','#ff258c','#a449ff','#ffb52f','#22baff','#20ffca']
for i,m in enumerate(re.finditer(pattern,read('MVPGame.cpp'))):
    ident,name,desc,school,damage,cooldown=m.groups()
    cards.append(dict(id=ident,name=name,description=text(desc),school=school,damage=number(damage),cooldown=number(cooldown),range=11,radius=2.3 if modes[i]=='nova' else 0,count=[1,3,8,1,1,3][i],mode=modes[i],color=colors[i],art='assets/art/'+ident+'.webp'))
pattern=r'\{TEXT\("(\w+)"\),TEXT\("([^"]+)"\),TEXT\("((?:\\.|[^"\\])*)"\),EMagicSchool::(\w+),([\d.]+f?),([\d.]+f?),([\d.]+f?),([\d.]+f?),(\d+),FLinearColor\(([^)]+)\)\}'
modes=['chain','chain','charge','return','ricochet','rail','petal','field','shatter','homing']
for i,m in enumerate(re.finditer(pattern,read('NeonArsenal.cpp'))):
    ident,name,desc,school,damage,cooldown,distance,radius,count,rgb=m.groups()
    values=[round(number(v.strip())*255) for v in rgb.split(',')]
    cards.append(dict(id=ident,name=name,description=text(desc),school=school,damage=number(damage),cooldown=number(cooldown),range=number(distance)/100,radius=number(radius)/100,count=int(count),mode=modes[i],color='#'+''.join(f'{v:02x}' for v in values),art='assets/art/'+ident+'.webp'))
enemies=[]
pattern=r'\{TEXT\("(\w+)"\),TEXT\("([^"]+)"\),(\d+),([\d.]+),([\d.]+),([\d.]+),([\d.]+),([\d.]+),([\d.]+),([\d.]+)\}'
for m in re.finditer(pattern,read('CyberEnemyRoster.cpp')):
    ident,name,tier,hp,height,speed,unlock,contact,radius,xp=m.groups()
    enemies.append(dict(id=ident,name=name,tier=int(tier),health=float(hp),height=float(height)/100,speed=float(speed)/100,unlock=float(unlock),contactDamage=float(contact),radius=float(radius)/100,essence=int(xp),model='assets/models/'+ident+'.glb'))
challenges=[]
pattern=r'Make\(ENeonChallenge::(\w+),TEXT\("([^"]+)"\),TEXT\("((?:\\.|[^"\\])*)"\),\s*TEXT\("((?:\\.|[^"\\])*)"\),([\d.]+)'
for m in re.finditer(pattern,read('NeonChallenge.cpp')):
    ident,title,reward,risk,duration=m.groups();challenges.append(dict(id=ident,title=title,reward=text(reward),risk=text(risk),duration=float(duration)))
if len(cards)!=16 or len(enemies)!=10 or len(challenges)!=7:raise RuntimeError(f'Incomplete export: cards={len(cards)} enemies={len(enemies)} challenges={len(challenges)}')
sources=['MVPGame.cpp','NeonArsenal.cpp','CyberEnemyRoster.cpp','NeonChallenge.cpp']
rules=dict(schemaVersion=1,title='Forbidden Code: Survivor',source='UE5 authored C++ tuning; regenerate with Scripts/export_browser_rules.py',sourceHashes={p:hashlib.sha256(read(p).encode()).hexdigest() for p in sources},cards=cards,enemies=enemies,challenges=challenges,progression=dict(xpBase=18,xpLinear=8,xpQuadratic=2,waveInterval=1.4,waveBase=8,waveStepSeconds=30,waveStepCount=2,waveMax=22),browser=dict(enemyCap=90,projectileCap=384,sparkCap=3000),world=dict(bounds=54,roads=[dict(x=0,z=-4.5,w=103,d=9),dict(x=0,z=-12,w=9,d=78),dict(x=-32.5,z=-10.5,w=9,d=31),dict(x=23.5,z=-22.5,w=6.2,d=31)]))
if '--from-native' in sys.argv:
    # Run this in the complete Windows source workspace, then port the snapshot to main.
    def option(name, fallback=None):
        return sys.argv[sys.argv.index(name)+1] if name in sys.argv else fallback
    native_commit=option('--native-commit')
    native_version=option('--native-version','0.4.7')
    if not native_commit or not re.fullmatch(r'[0-9a-f]{40}',native_commit):
        raise RuntimeError('--from-native requires --native-commit with the full Windows release SHA')
    pat=r'\{TEXT\("(\w+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),ECardRarity::(\w+)\}'
    for ident,a,ad,b,bd,rarity in re.findall(pat,read('ForbiddenPrograms.cpp')):
     c=next(c for c in rules['cards'] if c['id']==ident);c.update(rarity=rarity,unlockCost={'Common':75,'Rare':150,'Epic':300}[rarity],evolutions=[dict(name=a,description=ad),dict(name=b,description=bd)])
    assert all(len(c.get('evolutions',[]))==2 for c in rules['cards'])
    rules['cards'][5]['description']='Three consumable wards. Basic: 1 charge; medium: 2; elite: 3. Bosses cost 4 charges for 8% max HP. Rank-ups replenish charges; cap 8.'
    pat=r'\{"(\w+)",TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),FLinearColor\(([^)]+)\),([\d.]+)\}'
    rules['riders']=[]
    free={'NeonExorcist':['PulseDart','PrismBurst','KineticCards','HaloNova'],'Vex':['ArcRelay','GhostLink','RicochetSeal','PulseDart'],'Rook':['TalismanGuard','HaloNova','EventHorizon','PulseDart'],'Zero':['PhotonLance','StrobeRail','KineticCards','PulseDart'],'Mira':['SatelliteChoir','GhostLink','TalismanGuard','PrismBurst']}
    for ident,name,role,desc,p1,p2,trade,rgb,height in re.findall(pat,read('ForbiddenRiderRoster.cpp')):
     rgb=[round(float(x.rstrip('f'))*255) for x in rgb.split(',')];rules['riders'].append(dict(id=ident,name=name,role=role,description=desc,perks=[p1,p2,trade],color='#'+''.join(f'{v:02x}' for v in rgb),free=free[ident],portrait=f'assets/riders/{ident}.webp',model=f'assets/models/{ident}.glb'))
    assert len(rules['riders'])==5
    rules.update(schemaVersion=2,nativeVersion=native_version,browserVersion='0.5.0',nativeSourceCommit=native_commit,source='Pinned Windows 0.4.7 gameplay snapshot; regenerate using Scripts/export_browser_rules.py',meta=dict(historyLimit=32,commonCost=75,rareCost=150,epicCost=300,legendaryCost=500,killFragmentDivisor=20,contractFragments=4,victoryFragments=25,maxRunFragments=200,rankCap=3,activeCardCap=16,rerolls=2,banishes=1))
    for name in ['ForbiddenPrograms.cpp','ForbiddenProfile.cpp','ForbiddenRiderRoster.cpp','ForbiddenEnemyBrain.cpp','ForbiddenDistrictDirector.cpp','ForbiddenRiderSubsystem.cpp']:
     rules['sourceHashes'][name]=hashlib.sha256(read(name).encode()).hexdigest()
    pinned.write_text(json.dumps(rules,indent=2),encoding='utf-8')

description='Forbidden Code: Survivor is a cyberpunk deckbuilding survivor. Ride a hoverboard through rain-soaked neon streets, combine forbidden energy spells, draft powerful mutations, and battle escalating cyborg hordes. Take high-risk challenges for dangerous rewards. Play in your browser or enjoy the high-fidelity Unreal Engine 5 Windows edition.'
assert len(description)<=350
for folder in ['GameData','Web/data']:
    output=root/folder;output.mkdir(parents=True,exist_ok=True);(output/'gameplay.json').write_text(json.dumps(rules,indent=2),encoding='utf-8')
(root/'GameData/repository-description.txt').write_text(description+'\n',encoding='utf-8')
print(f'RULES_EXPORTED cards={len(cards)} enemies={len(enemies)} challenges={len(challenges)} descriptionCharacters={len(description)}')
