import {normalizeUsername,validUsername} from './score-policy.mjs?v=0.2.2';
export const SCORE_API='https://forbidden-code-survivor-scores.brisk-cloud-6489.chatgpt.site';
const ID_KEY='forbidden-code-survivor.leaderboard.identity.v1',NAME_KEY='forbidden-code-survivor.leaderboard.name.v1';
export class LeaderboardClient{
 constructor({storage,endpoint=SCORE_API,fetcher=fetch,cryptoLike=crypto}={}){this.storage=storage;this.endpoint=endpoint;this.fetcher=(...args)=>fetcher(...args);try{this.playerToken=storage.getItem(ID_KEY);}catch{}if(!/^[a-f0-9]{64}$/.test(this.playerToken??'')){this.playerToken=Array.from(cryptoLike.getRandomValues(new Uint8Array(32)),n=>n.toString(16).padStart(2,'0')).join('');try{storage.setItem(ID_KEY,this.playerToken);}catch{}}}
 get username(){try{return normalizeUsername(this.storage.getItem(NAME_KEY)||'');}catch{return'';}}
 async request(path,data){let reply;try{reply=await this.fetcher(this.endpoint+path,{method:data?'POST':'GET',mode:'cors',credentials:'omit',headers:data?{'Content-Type':'application/json'}:{},body:data?JSON.stringify(data):undefined,signal:AbortSignal.timeout(12000)});}catch{throw Error('The score service could not be reached. Check your connection and retry.');}let result;try{result=await reply.json();}catch{throw Error('The score service returned an unexpected response. Please retry.');}if(!reply.ok)throw Error(result.error??'The score service is temporarily unavailable.');return result;}
 begin(eligible=true){const session={eligible,result:null,error:null};session.ready=eligible?this.request('/v1/runs',{playerToken:this.playerToken}).catch(error=>{session.error=error;return null;}):Promise.resolve(null);return session;}
 async list(){return this.request('/v1/leaderboard');}
 async submit(session,username,stats){username=normalizeUsername(username);if(!validUsername(username))throw Error('Use 3–20 letters, numbers, spaces, underscores, dots or hyphens.');if(!session?.eligible)throw Error('Testing and diagnostic runs are not ranked.');if(session.result)return session.result;if(session.pending)return session.pending;
  session.pending=(async()=>{const registered=await session.ready;if(!registered)throw Error('This run could not be registered when it started. Please start a new run while connected.');const result=await this.request('/v1/scores',{...stats,username,runId:registered.runId,runToken:registered.runToken});session.result=result;try{this.storage.setItem(NAME_KEY,username);}catch{}return result;})();
  try{return await session.pending;}finally{session.pending=null;}
 }
}
