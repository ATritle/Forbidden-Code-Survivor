export const SAVE_KEY='forbidden-code-survivor.web.v1';
export function chooseStationLocations(points,origin,count=3,random=Math.random){const chosen=[];for(const p of sampleUnique(points.map(p=>({x:p.x,z:p.z,id:p.x+','+p.z})),points.length,random)){if(Math.hypot(p.x-origin.x,p.z-origin.z)<14||chosen.some(q=>Math.hypot(p.x-q.x,p.z-q.z)<15))continue;chosen.push(p);if(chosen.length===count)break;}return chosen;}
export function rng(seed=1739){let s=seed>>>0;return()=>{s+=0x6d2b79f5;let t=Math.imul(s^s>>>15,1|s);t^=t+Math.imul(t^t>>>7,61|t);return((t^t>>>14)>>>0)/4294967296;};}
export function sampleUnique(items,count,random=Math.random){const seen=new Set(),bag=items.filter(x=>{const id=x.id??x;if(seen.has(id))return false;seen.add(id);return true;});for(let i=bag.length-1;i>0;i--){const j=Math.floor(random()*(i+1));[bag[i],bag[j]]=[bag[j],bag[i]];}return bag.slice(0,Math.max(0,count));}
export function xpCost(level,p){const n=Math.max(0,level-1);return p.xpBase+p.xpLinear*n+p.xpQuadratic*n*n;}
export function tierForTime(enemies,time,random=Math.random){const weights=[34,30,20,16,12,10,4,3,2];const bag=enemies.filter((x,i)=>x.tier<3&&x.unlock<=time).map(x=>[x,weights[enemies.indexOf(x)]]);let pick=random()*bag.reduce((sum,x)=>sum+x[1],0);for(const[x,w]of bag){pick-=w;if(pick<0)return x;}return enemies[0];}
export function validateDeck(ids,cards){const valid=sampleUnique(ids.filter(id=>cards.some(c=>c.id===id)),2,()=>.999999);return valid.length&&valid.some(id=>id!=='TalismanGuard')?valid:['PulseDart'];}
export function readPreferences(storage,cards){let data={};try{data=JSON.parse(storage.getItem(SAVE_KEY)||'{}')??{};}catch{}const clamp=(x,min,max,fallback)=>Number.isFinite(x)?Math.min(max,Math.max(min,x)):fallback;return{version:1,deck:validateDeck(Array.isArray(data.deck)?data.deck:[],cards),music:clamp(data.music,0,1,.4),sfx:clamp(data.sfx,0,1,.65),intensity:clamp(data.intensity,0,1.5,1),zoom:clamp(data.zoom,22,58,34),quality:data.quality==='high'?'high':'low',adaptive:data.adaptive!==false};}
export function writePreferences(storage,prefs){try{storage.setItem(SAVE_KEY,JSON.stringify(prefs));return true;}catch{return false;}}
export const MUTATIONS=[{id:'nova',name:'NOVA PROTOCOL',description:'Recompile the attack into a radial energy release.'},{id:'explosive',name:'VOLATILE PAYLOAD',description:'Impacts release an energy blast that damages nearby cyborgs.'},{id:'pierce',name:'PHASE PENETRATION',description:'Attacks pass through two more targets; chain spells gain two jumps.'}];
export function createWeapon(card){return{card,rank:1,timer:0,mods:{nova:false,explosive:false,piercing:card.mode==='pierce'?3:0,extra:0,radius:1}};}
export function mutateWeapon(weapon,kind){weapon.rank++;if(kind==='nova')weapon.mods.nova=true;else if(kind==='explosive')weapon.mods.explosive=true;else if(kind==='pierce')weapon.mods.piercing+=2;else weapon.mods.extra+=2;}
export function makeDraft(cards,weapons,random=Math.random){return sampleUnique(cards,3,random).map(card=>{
 const owned=weapons.find(w=>w.card.id===card.id);
 if(!owned)return{card,mutation:null,title:'INSTALL SPELL',description:card.description};
 if(card.mode==='guard')return{card,mutation:'charges',title:'TALISMAN REINFORCEMENT',description:'Restore two protective talismans, up to sixteen. No automatic recharge.'};
 const options=MUTATIONS.filter(m=>m.id==='pierce'||!owned.mods[m.id]),m=options[Math.floor(random()*options.length)];
 let title=m.name,description=m.description;
 if(m.id==='pierce'&&['field','charge'].includes(card.mode)){title='RECURSIVE PULSE';description='Release two additional delayed area pulses from every field or charge.';}
 if(m.id==='pierce'&&card.mode==='rail'){title='SPLIT RAIL';description='Emit two additional parallel piercing rails.';}
 if(m.id==='nova'&&['field','charge'].includes(card.mode))description='Center an expanded radial blast field on your hoverboard.';
 if(m.id==='nova'&&card.mode==='chain')description='Chain impacts also release an energy nova around your hoverboard.';
 if(m.id==='explosive'&&['field','charge'].includes(card.mode))description='The final pulse detonates a larger secondary energy blast.';
 return{card,mutation:m.id,title,description};});}
