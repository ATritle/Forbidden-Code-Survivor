import {SCORE_SEASON,validateRun,validateLoadout,RIDER_IDS} from '../shared/score-policy.mjs';

const GAME_ORIGIN='https://atritle.github.io';
const LOCAL_ORIGINS=new Set(['http://127.0.0.1:4173','http://127.0.0.1:4174']);
const PUBLIC_FIELDS='id,username,score,kills,level,seconds,challenges,won,recorded_at,loadout_json';
const WINDOWS_RESET='windows-loadout-board-2026-10-20';
const WINDOWS_SEASON='WINDOWS / BUILD PROFILES 01';
function publicRow({loadout_json,...row}){return{...row,won:!!row.won,loadout:loadout_json?JSON.parse(loadout_json):null};}
// Explicit one-time Windows reset requested by the owner. Backup and clearing are atomic.
async function resetWindowsOnce(db){
 if(await db.prepare('SELECT id FROM board_resets WHERE id=?1').bind(WINDOWS_RESET).first())return;
 const absent='NOT EXISTS (SELECT 1 FROM board_resets WHERE id=?1)';
 await db.batch([
  db.prepare(`INSERT INTO score_archives(reset_id,score_id,payload) SELECT ?1,id,json_object('player_hash',player_hash,'id',id,'username',username,'score',score,'edition',edition,'kills',kills,'level',level,'seconds',seconds,'challenges',challenges,'won',won,'recorded_at',recorded_at,'run_id',run_id,'loadout_json',loadout_json) FROM scores WHERE edition='windows' AND ${absent}`).bind(WINDOWS_RESET),
  db.prepare(`DELETE FROM scores WHERE edition='windows' AND ${absent}`).bind(WINDOWS_RESET),
  db.prepare(`DELETE FROM runs WHERE edition='windows' AND ${absent}`).bind(WINDOWS_RESET),
  db.prepare(`INSERT INTO board_resets(id,edition,reset_at) VALUES(?1,'windows',?2) ON CONFLICT(id) DO NOTHING`).bind(WINDOWS_RESET,Date.now())
 ]);
}
const ORDER='score DESC,recorded_at ASC,id ASC';
const BOARD_LIMIT=10,PLAYER_LIMIT=3;
const tokenPattern=/^[a-f0-9]{64}$/;
const uuidPattern=/^[a-f0-9-]{36}$/;
function editionOf(value){const edition=value??'browser';if(edition!=='browser'&&edition!=='windows')throw new HttpError(400,'Invalid game edition.');return edition;}

class HttpError extends Error{constructor(status,message){super(message);this.status=status;}}
const randomToken=()=>Array.from(crypto.getRandomValues(new Uint8Array(32)),n=>n.toString(16).padStart(2,'0')).join('');
async function digest(value){return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(value))),n=>n.toString(16).padStart(2,'0')).join('');}
function originAllowed(request){const origin=request.headers.get('Origin');return origin===GAME_ORIGIN||origin===new URL(request.url).origin||LOCAL_ORIGINS.has(origin);}
function response(request,data,status=200){const headers={'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Vary':'Origin'};if(originAllowed(request))headers['Access-Control-Allow-Origin']=request.headers.get('Origin');return new Response(JSON.stringify(data),{status,headers});}
async function body(request){
 if(!request.headers.get('Content-Type')?.toLowerCase().startsWith('application/json'))throw new HttpError(415,'JSON is required.');
 const reader=request.body?.getReader();if(!reader)throw new HttpError(400,'A request body is required.');
 const chunks=[];let size=0;for(;;){const next=await reader.read();if(next.done)break;size+=next.value.length;if(size>4096){await reader.cancel();throw new HttpError(413,'Request is too large.');}chunks.push(next.value);}
 const bytes=new Uint8Array(size);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
 try{return JSON.parse(new TextDecoder().decode(bytes));}catch{throw new HttpError(400,'Invalid JSON.');}
}

export default{
 async fetch(request,env){
  try{
   const url=new URL(request.url),path=url.pathname;
   if(request.method==='OPTIONS'){
    if(!originAllowed(request))return response(request,{error:'Origin is not allowed.'},403);
    const reply=response(request,{});return new Response(null,{status:204,headers:{...Object.fromEntries(reply.headers),'Access-Control-Allow-Methods':'GET, POST, OPTIONS','Access-Control-Allow-Headers':'Content-Type','Access-Control-Max-Age':'600'}});
   }
   if(request.method==='GET'&&path==='/')return response(request,{service:'Forbidden Code: Survivor leaderboard',season:SCORE_SEASON,game:'https://atritle.github.io/Forbidden-Code-Survivor/'});
   if(!env.DB)throw new HttpError(503,'Score storage is unavailable.');
   await resetWindowsOnce(env.DB);
   if(request.method==='GET'&&path==='/v1/leaderboard'){
    const edition=editionOf(url.searchParams.get('edition')??undefined);
    const result=await env.DB.prepare(`SELECT ${PUBLIC_FIELDS} FROM scores WHERE edition=?1 ORDER BY ${ORDER} LIMIT ${BOARD_LIMIT}`).bind(edition).all();
    return response(request,{season:edition==='windows'?WINDOWS_SEASON:SCORE_SEASON,edition,limit:BOARD_LIMIT,perPlayerLimit:PLAYER_LIMIT,entries:result.results.map((row,i)=>({...publicRow(row),rank:i+1}))});
   }
   if(request.method!=='POST')throw new HttpError(404,'Route not found.');
   if(!originAllowed(request))throw new HttpError(403,'Origin is not allowed.');
   const input=await body(request),now=Date.now();
   if(path==='/v1/runs'){
    if(!tokenPattern.test(input?.playerToken??''))throw new HttpError(400,'Invalid player identity.');
    const edition=editionOf(input.edition);
    if((edition==='windows'||input.riderId!==undefined)&&!RIDER_IDS.includes(input.riderId))throw new HttpError(400,'Update the Windows game to register a rider profile.');
    const playerHash=await digest(input.playerToken),id=crypto.randomUUID(),token=randomToken(),tokenHash=await digest(token),expiresAt=now+2*60*60*1000;
    const inserted=await env.DB.prepare('INSERT INTO runs(id,player_hash,token_hash,created_at,expires_at,edition,rider_id) SELECT ?1,?2,?3,?4,?5,?6,?8 WHERE (SELECT COUNT(*) FROM runs WHERE player_hash=?2 AND created_at>?7)<6 RETURNING id').bind(id,playerHash,tokenHash,now,expiresAt,edition,now-60000,input.riderId??null).first();
    if(!inserted)throw new HttpError(429,'Too many new runs. Wait a minute and try again.');
    // Expired sessions are separate from durable high scores.
    await env.DB.prepare('DELETE FROM runs WHERE expires_at<?1').bind(now-86400000).run();
    return response(request,{runId:id,runToken:token,expiresAt},201);
   }
   if(path==='/v1/scores'){
    if(!uuidPattern.test(input?.runId??'')||!tokenPattern.test(input?.runToken??''))throw new HttpError(400,'Invalid run credentials.');
    let run=await env.DB.prepare('SELECT * FROM runs WHERE id=?1').bind(input.runId).first();
    if(!run||run.token_hash!==await digest(input.runToken))throw new HttpError(403,'This run is not registered.');
    if(!run.submitted_at){
     if(run.expires_at<now)throw new HttpError(410,'This run has expired.');
     let stats,loadout=null;try{stats=validateRun(input,(now-run.created_at)/1000,run.edition,!!run.rider_id);if(run.edition==='windows'||run.rider_id){const bonus=input.bonusDrafts??0;if(!Number.isInteger(bonus)||bonus<0||bonus>stats.challenges+(input.wardComplete===true?1:0))throw Error('Invalid objective draft count.');loadout=validateLoadout(input,stats.level,bonus);if(loadout.riderId!==run.rider_id)throw Error('The rider differs from the registered run.');}}catch(error){throw new HttpError(400,error.message);}
     await env.DB.batch([
      env.DB.prepare('UPDATE runs SET submitted_at=?1,username=?2,score=?3,kills=?4,level=?5,seconds=?6,challenges=?7,won=?8,loadout_json=?10 WHERE id=?9 AND submitted_at IS NULL').bind(now,stats.username,stats.score,stats.kills,stats.level,stats.seconds,stats.challenges,stats.won?1:0,run.id,loadout?JSON.stringify(loadout):null),
      env.DB.prepare(`INSERT INTO scores(player_hash,id,username,score,kills,level,seconds,challenges,won,recorded_at,run_id,edition,loadout_json) SELECT player_hash,?1,username,score,kills,level,seconds,challenges,won,submitted_at,id,edition,loadout_json FROM runs WHERE id=?2 AND submitted_at IS NOT NULL ON CONFLICT(run_id) DO NOTHING`).bind(crypto.randomUUID(),run.id),
      env.DB.prepare(`DELETE FROM scores WHERE player_hash=?1 AND edition=?2 AND id NOT IN (SELECT id FROM scores WHERE player_hash=?1 AND edition=?2 ORDER BY ${ORDER} LIMIT ${PLAYER_LIMIT})`).bind(run.player_hash,run.edition)
     ]);
     run=await env.DB.prepare('SELECT * FROM runs WHERE id=?1').bind(run.id).first();
    }
    const personal=await env.DB.prepare(`SELECT ${PUBLIC_FIELDS},run_id,rank FROM (SELECT *,ROW_NUMBER() OVER (ORDER BY ${ORDER}) AS rank FROM scores WHERE edition=?2) WHERE player_hash=?1 ORDER BY ${ORDER}`).bind(run.player_hash,run.edition).all();
    const personalScores=personal.results.map(({run_id,...entry},i)=>({...publicRow(entry),personalRank:i+1,onLeaderboard:entry.rank<=BOARD_LIMIT}));
    const personalIndex=personal.results.findIndex(entry=>entry.run_id===run.id);
    return response(request,{accepted:true,score:run.score,personalBest:personalScores[0],improved:personalIndex===0,personalScores,retained:personalIndex>=0,personalRank:personalIndex>=0?personalIndex+1:null,entry:personalScores[personalIndex]??null,limit:BOARD_LIMIT,perPlayerLimit:PLAYER_LIMIT});
   }
   throw new HttpError(404,'Route not found.');
  }catch(error){if(!(error instanceof HttpError))console.error('Leaderboard storage request failed.');return response(request,{error:error instanceof HttpError?error.message:'The score service is temporarily unavailable.'},error.status??503);}
 }
};
