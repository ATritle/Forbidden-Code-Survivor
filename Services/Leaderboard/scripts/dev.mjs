// Local-only preview with a separate SQLite database. Never seeds hosted scores.
import http from 'node:http';
import {DatabaseSync} from 'node:sqlite';
import {readFile,readdir,mkdir} from 'node:fs/promises';
import worker from '../worker/index.js';
const root=new URL('../',import.meta.url);await mkdir(new URL('.sites-runtime/',root),{recursive:true});
const sqlite=new DatabaseSync(new URL('.sites-runtime/preview.sqlite',root));
sqlite.exec('CREATE TABLE IF NOT EXISTS preview_migrations(name TEXT PRIMARY KEY)');
for(const name of(await readdir(new URL('drizzle/',root))).filter(n=>n.endsWith('.sql')).sort()){if(sqlite.prepare('SELECT name FROM preview_migrations WHERE name=?').get(name))continue;sqlite.exec(await readFile(new URL('drizzle/'+name,root),'utf8'));sqlite.prepare('INSERT INTO preview_migrations VALUES(?)').run(name);}
const DB={prepare(sql){const statement=sqlite.prepare(sql);let args=[];return{bind(...values){args=values;return this;},async first(){return statement.get(...args)??null;},async all(){return{results:statement.all(...args),success:true};},async run(){return{meta:statement.run(...args),success:true};}};},async batch(statements){sqlite.exec('BEGIN');try{const result=[];for(const stmt of statements)result.push(await stmt.all());sqlite.exec('COMMIT');return result;}catch(error){sqlite.exec('ROLLBACK');throw error;}}};
http.createServer(async(req,res)=>{try{const chunks=[];let bytes=0;for await(const chunk of req){bytes+=chunk.length;if(bytes>4096){res.writeHead(413);res.end();return;}chunks.push(chunk);}const request=new Request('http://127.0.0.1:4175'+req.url,{method:req.method,headers:req.headers,body:bytes?Buffer.concat(chunks):undefined});const reply=await worker.fetch(request,{DB});res.writeHead(reply.status,Object.fromEntries(reply.headers));res.end(Buffer.from(await reply.arrayBuffer()));}catch(error){console.error(error.message);res.writeHead(500);res.end();}}).listen(4175,'127.0.0.1',()=>console.log('Isolated leaderboard preview: http://127.0.0.1:4175'));
