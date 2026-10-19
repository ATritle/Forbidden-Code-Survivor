export const SCORE_SEASON='ENERGY / SEASON 01';
export function normalizeUsername(value){return typeof value==='string'?value.normalize('NFKC').trim().replace(/\s+/g,' '):'';}
export function validUsername(value){return /^[A-Za-z0-9][A-Za-z0-9 _.\-]{2,19}$/.test(value);}
export function scoreRun({kills=0,level=1,seconds=0,challenges=0,won=false}){return Math.floor(seconds)*10+kills*100+(level-1)*500+challenges*1500+(won?10000:0);}
export function validateRun(data,wallSeconds){
 if(!data||typeof data!=='object')throw Error('Invalid run.');
 for(const [key,min,max]of[['kills',0,300000],['level',1,200],['seconds',2,7200],['challenges',0,3]])if(!Number.isInteger(data[key])||data[key]<min||data[key]>max)throw Error('Invalid '+key+'.');
 if(typeof data.won!=='boolean')throw Error('Invalid outcome.');
 if(data.seconds>wallSeconds+5)throw Error('Run time exceeds the registered session.');
 if(data.kills>data.seconds*40+90)throw Error('Kill count exceeds the run limit.');
 if(data.won&&(data.seconds<355||data.kills<1))throw Error('Victory requires reaching the boss.');
 const username=normalizeUsername(data.username);if(!validUsername(username))throw Error('Use 3–20 letters, numbers, spaces, underscores, dots or hyphens.');
 return{username,kills:data.kills,level:data.level,seconds:data.seconds,challenges:data.challenges,won:data.won,score:scoreRun(data)};
}
