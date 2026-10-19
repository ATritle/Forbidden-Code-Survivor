import {SCORE_SEASON,validateRun} from '../shared/score-policy.mjs';

const GAME_ORIGIN='https://atritle.github.io';
const LOCAL_ORIGINS=new Set(['http://127.0.0.1:4173','http://127.0.0.1:4174']);
const PUBLIC_FIELDS='id,username,score,kills,level,seconds,challenges,won,recorded_at';
const ORDER='score DESC,recorded_at ASC,id ASC';
const tokenPattern=/^[a-f0-9]{64}$/;
const uuidPattern=/^[a-f0-9-]{36}$/;

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
   if(request.method==='GET'&&path==='/v1/leaderboard'){
    const result=await env.DB.prepare(`SELECT ${PUBLIC_FIELDS} FROM scores ORDER BY ${ORDER} LIMIT 50`).all();
    return response(request,{season:SCORE_SEASON,entries:result.results.map((row,i)=>({...row,rank:i+1,won:!!row.won}))});
   }
   if(request.method!=='POST')throw new HttpError(404,'Route not found.');
   if(!originAllowed(request))throw new HttpError(403,'Origin is not allowed.');
   const input=await body(request),now=Date.now();
   if(path==='/v1/runs'){
    if(!tokenPattern.test(input?.playerToken??''))throw new HttpError(400,'Invalid player identity.');
    const playerHash=await digest(input.playerToken),id=crypto.randomUUID(),token=randomToken(),tokenHash=await digest(token),expiresAt=now+2*60*60*1000;
    const inserted=await env.DB.prepare('INSERT INTO runs(id,player_hash,token_hash,created_at,expires_at) SELECT ?1,?2,?3,?4,?5 WHERE (SELECT COUNT(*) FROM runs WHERE player_hash=?2 AND created_at>?6)<6 RETURNING id').bind(id,playerHash,tokenHash,now,expiresAt,now-60000).first();
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
     let stats;try{stats=validateRun(input,(now-run.created_at)/1000);}catch(error){throw new HttpError(400,error.message);}
     await env.DB.batch([
      env.DB.prepare('UPDATE runs SET submitted_at=?1,username=?2,score=?3,kills=?4,level=?5,seconds=?6,challenges=?7,won=?8 WHERE id=?9 AND submitted_at IS NULL').bind(now,stats.username,stats.score,stats.kills,stats.level,stats.seconds,stats.challenges,stats.won?1:0,run.id),
      env.DB.prepare(`INSERT INTO scores(player_hash,id,username,score,kills,level,seconds,challenges,won,recorded_at,run_id) SELECT player_hash,?1,username,score,kills,level,seconds,challenges,won,submitted_at,id FROM runs WHERE id=?2 AND submitted_at IS NOT NULL ON CONFLICT(player_hash) DO UPDATE SET username=excluded.username,score=excluded.score,kills=excluded.kills,level=excluded.level,seconds=excluded.seconds,challenges=excluded.challenges,won=excluded.won,recorded_at=excluded.recorded_at,run_id=excluded.run_id WHERE excluded.score>scores.score`).bind(crypto.randomUUID(),run.id)
     ]);
     run=await env.DB.prepare('SELECT * FROM runs WHERE id=?1').bind(run.id).first();
    }
    const best=await env.DB.prepare(`SELECT ${PUBLIC_FIELDS},run_id FROM scores WHERE player_hash=?1`).bind(run.player_hash).first();
    const place=await env.DB.prepare('SELECT 1+COUNT(*) AS rank FROM scores WHERE score>?1 OR (score=?1 AND recorded_at<?2) OR (score=?1 AND recorded_at=?2 AND id<?3)').bind(best.score,best.recorded_at,best.id).first();
    const {run_id,...entry}=best;
    return response(request,{accepted:true,score:run.score,personalBest:{...entry,won:!!best.won,rank:place.rank},improved:run_id===run.id});
   }
   throw new HttpError(404,'Route not found.');
  }catch(error){if(!(error instanceof HttpError))console.error('Leaderboard storage request failed.');return response(request,{error:error instanceof HttpError?error.message:'The score service is temporarily unavailable.'},error.status??503);}
 }
};
