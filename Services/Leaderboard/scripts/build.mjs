import {readFile,writeFile,mkdir,cp} from 'node:fs/promises';
const root=new URL('../',import.meta.url),out=new URL('dist/',root);
await mkdir(new URL('server/',out),{recursive:true});await mkdir(new URL('.openai/',out),{recursive:true});
const policy=await readFile(new URL('shared/score-policy.mjs',root),'utf8');
const worker=await readFile(new URL('worker/index.js',root),'utf8');
await writeFile(new URL('server/index.js',out),policy.replaceAll('export ','')+'\n'+worker.replace("import {SCORE_SEASON,validateRun,validateLoadout,RIDER_IDS} from '../shared/score-policy.mjs';",''));
await cp(new URL('.openai/hosting.json',root),new URL('.openai/hosting.json',out));
await cp(new URL('drizzle/',root),new URL('.openai/drizzle/',out),{recursive:true});
console.log('LEADERBOARD_WORKER_BUILT');
