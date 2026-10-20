export const SCORE_SEASON='ENERGY / SEASON 01';
export function normalizeUsername(value){return typeof value==='string'?value.normalize('NFKC').trim().replace(/\s+/g,' '):'';}
export function validUsername(value){return /^[A-Za-z0-9][A-Za-z0-9 _.\-]{2,19}$/.test(value);}
export function scoreRun({kills=0,level=1,seconds=0,challenges=0,won=false}){return Math.floor(seconds)*10+kills*100+(level-1)*500+challenges*1500+(won?10000:0);}
export function validateRun(data,wallSeconds,edition='browser',objectiveBuild=edition==='windows'){
 if(!data||typeof data!=='object')throw Error('Invalid run.');
 for(const [key,min,max]of[['kills',0,300000],['level',1,200],['seconds',2,7200],['challenges',0,3]])if(!Number.isInteger(data[key])||data[key]<min||data[key]>max)throw Error('Invalid '+key+'.');
 if(typeof data.won!=='boolean')throw Error('Invalid outcome.');
 if(data.seconds>wallSeconds+5)throw Error('Run time exceeds the registered session.');
 if(data.kills>data.seconds*40+90)throw Error('Kill count exceeds the run limit.');
 if(data.won&&(data.seconds<(objectiveBuild?175:355)||data.kills<1))throw Error('Victory requires reaching the boss.');
 const username=normalizeUsername(data.username);if(!validUsername(username))throw Error('Use 3â€“20 letters, numbers, spaces, underscores, dots or hyphens.');
 return{username,kills:data.kills,level:data.level,seconds:data.seconds,challenges:data.challenges,won:data.won,score:scoreRun(data)};
}

export const RIDER_IDS=['NeonExorcist','Vex','Rook','Zero','Mira'];
export const CARD_IDS=['PulseDart','PrismBurst','HaloNova','KineticCards','PhotonLance','TalismanGuard','ArcRelay','GhostLink','NeonCataclysm','OrbitRazor','RicochetSeal','StrobeRail','EchoPetals','EventHorizon','ShatterSigil','SatelliteChoir'];
export function validateLoadout(data,playerLevel,bonusDrafts=0){
 if(!RIDER_IDS.includes(data?.riderId))throw Error('Update the game to submit a rider profile.');
 if(!Array.isArray(data.cards)||data.cards.length<1||data.cards.length>16)throw Error('A run requires 1–16 card snapshots.');
 const seen=new Set();let picks=0;
 const cards=data.cards.map(card=>{
  if(!card||!CARD_IDS.includes(card.id)||seen.has(card.id)||!Number.isInteger(card.level)||card.level<1||card.level>3||!Number.isInteger(card.evolution)||card.evolution<0||card.evolution>2||(card.evolution>0&&card.level!==3))throw Error('Invalid card snapshot.');
  seen.add(card.id);picks+=card.level+(card.evolution>0?1:0);
  return{id:card.id,level:card.level,evolution:card.evolution};
 });
 if(picks>playerLevel+1+bonusDrafts)throw Error('Card levels exceed the available run drafts.');
 return{riderId:data.riderId,cards};
}
