"""Generate review notes from the native tuning tables; never used as a second runtime database."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
cards=[]
for line in (root/'Source/DeckBuilder/MVPGame.cpp').read_text().splitlines():
    if '    AddCard(TEXT(' not in line:continue
    names=re.findall(r'TEXT\("((?:[^"\\]|\\.)*)"\)',line)
    numbers=re.search(r'EMagicSchool::(\w+),([\d.]+)f,([\d.]+)f,',line)
    if names and numbers:cards.append((names[0],names[1],names[2].replace('\\n',' '),numbers[1],float(numbers[2]),float(numbers[3])))
for line in (root/'Source/DeckBuilder/NeonArsenal.cpp').read_text().splitlines():
    if '{TEXT(' not in line:continue
    names=re.findall(r'TEXT\("((?:[^"\\]|\\.)*)"\)',line)
    numbers=re.search(r'EMagicSchool::(\w+),([\d.]+),([\d.]+)f?,',line)
    if len(names)==3 and numbers:cards.append((names[0],names[1],names[2].replace('\\n',' '),numbers[1],float(numbers[2]),float(numbers[3])))
branches={}
for line in (root/'Source/DeckBuilder/ForbiddenPrograms.cpp').read_text().splitlines():
    if '{TEXT(' in line:
        names=re.findall(r'TEXT\("((?:[^"\\]|\\.)*)"\)',line)
        if len(names)==5:branches[names[0]]=names[1:]
assert len(cards)==16 and len(branches)==16,(len(cards),len(branches))
text=['WINDOWS 0.3.0 / PROGRAM TUNING AND EVOLUTIONS','Generated from native source tables. Damage is per hit/pulse/projectile, not total volley DPS.','Rank 2: base damage x1.12, cooldown x0.96. Rank 3: x1.24 and x0.92.','One evolution follows rank 3. Its authored structural rules replace the indicated pattern; branches never stack.','Global contract/perk bonuses and target armor are applied separately. Exact branch geometry/caps live in source and are included in rendered validation.','']
for ident,name,desc,school,damage,cooldown in cards:
    text += [name+' / '+school,('Consumable contact ward; event-driven rather than an automatic firing cooldown.' if ident=='TalismanGuard' else f'Base: {damage:g} damage / {cooldown:g}s. Rank 3 before evolution: {damage*1.24:.2f} / {cooldown*.92:.3f}s.'),desc]
    a,ad,b,bd=branches[ident];text += ['A: '+a+' / '+ad,'B: '+b+' / '+bd,'']
text+=['CYBORG ROSTER','Health / height cm / speed cm/s / first eligible second / contact damage / essence. Boss timing is objective-controlled.','']
for line in (root/'Source/DeckBuilder/CyberEnemyRoster.cpp').read_text().splitlines():
    names=re.findall(r'TEXT\("([^"\n]+)"\)',line)
    values=re.search(r'\),([\d,]+)\}',line)
    if len(names)==2 and values:
        tier,hp,height,speed,unlock,contact,radius,essence=map(int,values[1].split(','))
        text.append(f'{names[1]} / tier {tier}: {hp} HP / {height}cm / {speed}cm/s / {unlock}s / {contact} contact / {essence} essence / {radius}cm collision radius')
(root/'Docs/WindowsOverhaul/CARDS-AND-ENEMIES.txt').write_text('\n'.join(text)+'\n',encoding='utf-8')
print('CATALOG_READY 16 programs, 32 exclusive evolutions, 10 enemies')
