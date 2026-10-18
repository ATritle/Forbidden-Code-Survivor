"""Export the UE edition's authored tuning into a versioned browser rules snapshot."""
from pathlib import Path
import re,json,hashlib
root=Path(__file__).resolve().parents[1]
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
description='Forbidden Code: Survivor is a cyberpunk deckbuilding survivor. Ride a hoverboard through rain-soaked neon streets, combine forbidden energy spells, draft powerful mutations, and battle escalating cyborg hordes. Take high-risk challenges for dangerous rewards. Play in your browser or enjoy the high-fidelity Unreal Engine 5 Windows edition.'
assert len(description)<=350
for folder in ['GameData','Web/data']:
    output=root/folder;output.mkdir(parents=True,exist_ok=True);(output/'gameplay.json').write_text(json.dumps(rules,indent=2),encoding='utf-8')
(root/'GameData/repository-description.txt').write_text(description+'\n',encoding='utf-8')
print(f'RULES_EXPORTED cards={len(cards)} enemies={len(enemies)} challenges={len(challenges)} descriptionCharacters={len(description)}')
