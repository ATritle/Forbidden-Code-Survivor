import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFile,readdir} from 'node:fs/promises';
import worker from '../worker/index.js';
const schema=await Promise.all((await readdir(new URL('../drizzle/',import.meta.url))).filter(p=>p.endsWith('.sql')).sort().map(p=>readFile(new URL('../drizzle/'+p,import.meta.url),'utf8')));
function database(){const sqlite=new DatabaseSync(':memory:');for(const sql of schema)sqlite.exec(sql);const env={DB:{prepare(sql){const statement=sqlite.prepare(sql);let args=[];return{bind(...values){args=values;return this;},async first(){return statement.get(...args)??null;},async all(){return{results:statement.all(...args),success:true};},async run(){return{meta:statement.run(...args),success:true};}};},async batch(statements){sqlite.exec('BEGIN');try{const result=[];for(const stmt of statements)result.push(await stmt.all());sqlite.exec('COMMIT');return result;}catch(error){sqlite.exec('ROLLBACK');throw error;}}}};return{env,sqlite};}
async function call(env,path,data,origin='https://atritle.github.io',method=data?'POST':'GET'){const reply=await worker.fetch(new Request('https://scores.test'+path,{method,headers:{Origin:origin,'Content-Type':'application/json'},body:data?JSON.stringify(data):undefined}),env);return{reply,status:reply.status,data:reply.status===204?null:await reply.json()};}
async function run(db,key='a'){const result=await call(db.env,'/v1/runs',{playerToken:key.repeat(64)});assert.equal(result.status,201);db.sqlite.prepare('UPDATE runs SET created_at=? WHERE id=?').run(Date.now()-60000,result.data.runId);return result.data;}
const stats={username:'Neo Fox',kills:10,level:2,seconds:15,challenges:0,won:false,riderId:'Zero',cards:[{id:'PulseDart',level:1,evolution:0},{id:'KineticCards',level:1,evolution:0}]};

test('shared board starts empty and supports cross-origin preflight',async()=>{const db=database();const empty=await call(db.env,'/v1/leaderboard');assert.deepEqual(empty.data.entries,[]);const preflight=await call(db.env,'/v1/scores',null,'https://atritle.github.io','OPTIONS');assert.equal(preflight.status,204);assert.equal(preflight.reply.headers.get('Access-Control-Allow-Origin'),'https://atritle.github.io');assert.equal((await call(db.env,'/v1/scores',{},'https://hostile.test')).status,403);});
test('server computes points, retries are idempotent, public rows hide credentials',async()=>{const db=database(),session=await run(db);const first=await call(db.env,'/v1/scores',{...session,...stats,score:99999999});assert.equal(first.status,200);assert.equal(first.data.score,1650);assert.equal(first.data.improved,true);const again=await call(db.env,'/v1/scores',{...session,...stats,kills:99999});assert.equal(again.data.score,1650);const board=(await call(db.env,'/v1/leaderboard')).data.entries;assert.equal(board.length,1);assert.equal(board[0].username,'Neo Fox');assert.equal(board[0].rank,1);for(const key of ['player_hash','token_hash','runToken','run_id'])assert.ok(!(key in board[0]));});
test('each browser keeps its three highest distinct runs and changing names cannot bypass the cap',async()=>{
 const db=database();const saved=[];
 for(const [kills,username]of [[10,'First'],[0,'Lower Run'],[20,'Highest'],[15,'Third Alias']]){
  const session=await run(db);const result=await call(db.env,'/v1/scores',{...session,...stats,kills,username});
  assert.equal(result.status,200);saved.push({session,result:result.data});
 }
 const last=saved.at(-1).result;assert.equal(last.improved,false);assert.equal(last.retained,true);assert.equal(last.personalRank,2);
 assert.deepEqual(last.personalScores.map(e=>e.score),[2650,2150,1650]);
 assert.equal(last.personalBest.username,'Highest');
 assert.equal(db.sqlite.prepare('SELECT COUNT(*) AS n FROM scores').get().n,3);
 const retry=await call(db.env,'/v1/scores',{...saved[1].session,...stats,kills:99999});
 assert.equal(retry.data.score,650);assert.equal(retry.data.retained,false);assert.equal(retry.data.personalRank,null);
 assert.equal(db.sqlite.prepare('SELECT COUNT(*) AS n FROM scores').get().n,3);
 const board=(await call(db.env,'/v1/leaderboard')).data;assert.equal(board.entries.length,3);
 assert.equal(board.limit,10);assert.equal(board.perPlayerLimit,3);
 for(const entry of last.personalScores)for(const key of ['player_hash','token_hash','runToken','run_id'])assert.ok(!(key in entry));
});
test('global board caps at ten rows, at most three per player, with correct global submission ranks',async()=>{
 const db=database();let last;
 for(const key of ['a','b','c','d','e'])for(const kills of [10,20,30,5]){
  const session=await run(db,key);last=await call(db.env,'/v1/scores',{...session,...stats,kills,username:'Player '+key});assert.equal(last.status,200);
 }
 assert.equal(db.sqlite.prepare('SELECT COUNT(*) AS n FROM scores').get().n,15);
 const entries=(await call(db.env,'/v1/leaderboard')).data.entries;
 assert.equal(entries.length,10);assert.deepEqual(entries.map(e=>e.rank),[1,2,3,4,5,6,7,8,9,10]);
 for(const name of new Set(entries.map(e=>e.username)))assert.ok(entries.filter(e=>e.username===name).length<=3);
 assert.ok(last.data.personalScores.some(e=>!e.onLeaderboard&&e.rank>10));
 for(const e of last.data.personalScores){const expected=db.sqlite.prepare('SELECT 1+COUNT(*) AS rank FROM scores WHERE score>? OR (score=? AND recorded_at<?) OR (score=? AND recorded_at=? AND id<?)').get(e.score,e.score,e.recorded_at,e.score,e.recorded_at,e.id);assert.equal(e.rank,expected.rank);}
});
test('equal-score runs are distinct, older runs win ties, and retry never adds a duplicate',async()=>{
 const db=database();const sessions=[];
 for(let i=0;i<4;i++){
  const session=await run(db);sessions.push(session);
  const result=await call(db.env,'/v1/scores',{...session,...stats});assert.equal(result.status,200);
  if(i===0)db.sqlite.prepare('UPDATE scores SET recorded_at=1 WHERE run_id=?').run(session.runId);
 }
 const before=(await call(db.env,'/v1/leaderboard')).data.entries;
 assert.equal(before.length,3);assert.equal(before[0].recorded_at,1);
 await call(db.env,'/v1/scores',{...sessions[0],...stats});
 assert.deepEqual((await call(db.env,'/v1/leaderboard')).data.entries,before);
});
test('schema upgrade preserves existing personal bests and permits multiple rows per identity',()=>{
 const sqlite=new DatabaseSync(':memory:');sqlite.exec(schema[0]);
 const row=['player','existing-id','Existing Player',5000,20,3,100,1,0,123,'existing-run'];
 sqlite.prepare('INSERT INTO scores VALUES(?,?,?,?,?,?,?,?,?,?,?)').run(...row);
 for(const sql of schema.slice(1))sqlite.exec(sql);
 const migrated=sqlite.prepare('SELECT * FROM scores').get();assert.deepEqual(Object.values(migrated).slice(0,row.length),row);assert.equal(migrated.edition,'browser');
 sqlite.prepare('INSERT INTO scores(player_hash,id,username,score,kills,level,seconds,challenges,won,recorded_at,run_id) VALUES(?,?,?,?,?,?,?,?,?,?,?)').run('player','new-id','Existing Player',4000,10,2,100,1,0,456,'new-run');
 assert.equal(sqlite.prepare('SELECT COUNT(*) AS n FROM scores').get().n,2);sqlite.close();
});
test('forged credentials, implausible timing and unsafe names are rejected',async()=>{const db=database(),session=await run(db);assert.equal((await call(db.env,'/v1/scores',{...session,...stats,runToken:'b'.repeat(64)})).status,403);assert.equal((await call(db.env,'/v1/scores',{...session,...stats,seconds:500})).status,400);assert.equal((await call(db.env,'/v1/scores',{...session,...stats,username:'<script>alert(1)</script>'})).status,400);assert.equal((await call(db.env,'/v1/scores',{...session,...stats,won:true})).status,400);assert.equal((await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64),extra:'x'.repeat(5000)})).status,413);});
test('run creation is bounded and expired sessions cannot submit',async()=>{const db=database();for(let i=0;i<6;i++)assert.equal((await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64)})).status,201);assert.equal((await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64)})).status,429);const session=await run(db,'b');db.sqlite.prepare('UPDATE runs SET expires_at=? WHERE id=?').run(Date.now()-1,session.runId);assert.equal((await call(db.env,'/v1/scores',{...session,...stats})).status,410);});
test('Windows has an independent top three per identity and does not displace browser scores',async()=>{
 const db=database();
 for(const edition of ['browser','windows'])for(const kills of [5,10,20,30]){
  const session=await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64),edition,riderId:'Zero'});assert.equal(session.status,201);
  db.sqlite.prepare('UPDATE runs SET created_at=? WHERE id=?').run(Date.now()-60000,session.data.runId);
  assert.equal((await call(db.env,'/v1/scores',{...session.data,...stats,kills,username:edition})).status,200);
 }
 assert.equal(db.sqlite.prepare('SELECT COUNT(*) n FROM scores').get().n,6);
 const browser=(await call(db.env,'/v1/leaderboard')).data,windows=(await call(db.env,'/v1/leaderboard?edition=windows')).data;
 assert.equal(browser.edition,'browser');assert.equal(windows.edition,'windows');
 assert.equal(browser.entries.length,3);assert.equal(windows.entries.length,3);
 assert.ok(browser.entries.every(e=>e.username==='browser'));assert.ok(windows.entries.every(e=>e.username==='windows'));
 assert.equal((await call(db.env,'/v1/leaderboard?edition=unknown')).status,400);
});
test('only registered Windows runs may submit an early relay-boss victory',async()=>{
 const db=database();
 for(const edition of ['browser','windows']){
  const session=await call(db.env,'/v1/runs',{playerToken:'b'.repeat(64),edition,riderId:'Zero'});assert.equal(session.status,201);
  db.sqlite.prepare('UPDATE runs SET created_at=? WHERE id=?').run(Date.now()-200000,session.data.runId);
  const result=await call(db.env,'/v1/scores',{...session.data,...stats,seconds:180,won:true,edition:'windows'});
  assert.equal(result.status,edition==='windows'?200:400);
 }
});

test('Windows metadata is immutable on retry, validated and public without credentials',async()=>{
 const db=database();const registered=await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64),edition:'windows',riderId:'Zero'});
 db.sqlite.prepare('UPDATE runs SET created_at=? WHERE id=?').run(Date.now()-60000,registered.data.runId);
 const input={...registered.data,...stats,level:5,cards:[{id:'PulseDart',level:3,evolution:1},{id:'KineticCards',level:1,evolution:0}]};
 for(const bad of [{cards:[]},{cards:[input.cards[0],input.cards[0]]},{cards:[{id:'Unknown',level:1,evolution:0}]},{riderId:'Mira'},{cards:[{id:'PulseDart',level:4,evolution:0}]},{cards:[{id:'PulseDart',level:1,evolution:1}]},{level:1}])assert.equal((await call(db.env,'/v1/scores',{...input,...bad})).status,400);
 assert.equal((await call(db.env,'/v1/scores',input)).status,200);
 await call(db.env,'/v1/scores',{...input,riderId:'Mira',cards:[]});
 const row=(await call(db.env,'/v1/leaderboard?edition=windows')).data.entries[0];assert.deepEqual(row.loadout,{riderId:'Zero',cards:input.cards});assert.ok(!('loadout_json' in row));assert.ok(!('player_hash' in row));
 assert.equal((await call(db.env,'/v1/runs',{playerToken:'b'.repeat(64),edition:'windows'})).status,400);
});
test('requested reset archives and clears only Windows once, invalidating old sessions',async()=>{
 const db=database();
 const insert=db.sqlite.prepare('INSERT INTO scores(player_hash,id,username,score,kills,level,seconds,challenges,won,recorded_at,run_id,edition) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)');
 insert.run('p','old-score','Legacy Win',5000,20,3,100,1,0,123,'old-run','windows');insert.run('b','browser-score','Browser Player',999,1,1,10,0,0,123,'browser-run','browser');
 db.sqlite.prepare('INSERT INTO runs(id,player_hash,token_hash,edition,created_at,expires_at) VALUES(?,?,?,?,?,?)').run('legacy-session','p','hash','windows',1,999999);
 assert.deepEqual((await call(db.env,'/v1/leaderboard?edition=windows')).data.entries,[]);assert.equal(db.sqlite.prepare('SELECT COUNT(*) n FROM runs').get().n,0);
 const archived=JSON.parse(db.sqlite.prepare('SELECT payload FROM score_archives').get().payload);assert.equal(archived.id,'old-score');assert.equal(archived.player_hash,'p');assert.equal((await call(db.env,'/v1/leaderboard')).data.entries[0].username,'Browser Player');
 const session=await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64),edition:'windows',riderId:'Zero'});db.sqlite.prepare('UPDATE runs SET created_at=? WHERE id=?').run(Date.now()-60000,session.data.runId);await call(db.env,'/v1/scores',{...session.data,...stats});
 for(let i=0;i<3;i++)assert.equal((await call(db.env,'/v1/leaderboard?edition=windows')).data.entries.length,1);
 assert.equal(db.sqlite.prepare('SELECT COUNT(*) n FROM score_archives').get().n,1);assert.equal(db.sqlite.prepare('SELECT COUNT(*) n FROM board_resets').get().n,1);
});
test('Windows replaces the weakest personal run with its own rider and card snapshot',async()=>{
 const db=database();for(const [kills,riderId,id]of[[30,'Zero','PhotonLance'],[20,'Vex','ArcRelay'],[10,'Mira','GhostLink'],[25,'Rook','HaloNova'],[1,'NeonExorcist','PulseDart']]){
  const session=await call(db.env,'/v1/runs',{playerToken:'a'.repeat(64),edition:'windows',riderId});db.sqlite.prepare('UPDATE runs SET created_at=? WHERE id=?').run(Date.now()-60000,session.data.runId);
  const result=await call(db.env,'/v1/scores',{...session.data,...stats,kills,riderId,cards:[{id,level:1,evolution:0}]});assert.equal(result.status,200);
 }
 const board=(await call(db.env,'/v1/leaderboard?edition=windows')).data.entries;assert.deepEqual(board.map(r=>r.kills),[30,25,20]);assert.deepEqual(board.map(r=>r.loadout.riderId),['Zero','Rook','Vex']);assert.deepEqual(board.map(r=>r.loadout.cards[0].id),['PhotonLance','HaloNova','ArcRelay']);
});
