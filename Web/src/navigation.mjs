// Collision and walkable surfaces are shared by movement, visibility and navigation.
export const ARENA_WALLS=[[-35,-33,14,13,15],[36,-16.5,17,16,4.2],[44.5,12.5,13,14,3.6],[-46.5,25.5,10,18,23],[-26,43.5,16,10,16],[26.5,-44,16,10,18.5],[-12.5,-46.5,9.5,8.5,12.5],[-14,8,9,5,5],[34,-30,7,9,7]].map(([x,z,w,d,h])=>({x,z,w,d,h}));
export const ARENA_PLATFORMS=[
 {id:'east',x:24,z:6.5,w:11,d:15,h:3.2,ramp:{axis:'x',lo:6.5,hi:18.5,across:6.5,width:6}},
 {id:'shrine',x:0,z:37,w:20,d:18,h:2.2,ramp:{axis:'z',lo:10,hi:28,across:6,width:6.5},extraRamps:[{axis:'x',lo:-24,hi:-10,across:36,width:5}]},
 {id:'market',x:31,z:34,w:10,d:12,h:1.2,ramp:{axis:'x',lo:19,hi:26,across:34,width:6}},
 {id:'marketUpper',x:41,z:41,w:8,d:8,h:2,ramp:{axis:'z',lo:28,hi:37,across:41,width:5}},
 {id:'west',x:-32,z:15,w:12,d:16,h:5.6,ramp:{axis:'x',lo:-26,hi:-15,across:15,width:6,reverse:true}},
 // Flush connection to the overpass; the old ramp stopped 1.75m short and crossed a tower.
 {id:'overpass',x:0,z:-30,w:30,d:4.5,h:6.2,bridge:true,ramp:{axis:'z',lo:-27.75,hi:-14,across:12,width:5.5,reverse:true},extraRamps:[{axis:'x',lo:15,hi:29,across:-30,width:5,reverse:true}]}
];
const inside=(x,z,b,pad=0)=>Math.abs(x-b.x)<b.w/2+pad&&Math.abs(z-b.z)<b.d/2+pad;
const ramps=p=>[p.ramp,...(p.extraRamps||[])];
function rampHeight(p,x,z,pad=0,r=p.ramp){const a=r.axis==='x'?x:z,c=r.axis==='x'?z:x;if(a<r.lo-pad||a>r.hi+pad||Math.abs(c-r.across)>r.width/2+pad)return null;const t=Math.max(0,Math.min(1,(a-r.lo)/(r.hi-r.lo)));return p.h*(r.reverse?1-t:t);}
function interval(a,b,box){let lo=0,hi=1;for(const [s,d,min,max]of[[a.x,b.x-a.x,box.x-box.w/2,box.x+box.w/2],[a.z,b.z-a.z,box.z-box.d/2,box.z+box.d/2]]){if(Math.abs(d)<1e-9){if(s<min||s>max)return null;}else{let u=(min-s)/d,v=(max-s)/d;if(u>v)[u,v]=[v,u];lo=Math.max(lo,u);hi=Math.min(hi,v);if(lo>hi)return null;}}return[lo,hi];}
export class WalkableWorld{
 constructor({walls=ARENA_WALLS,platforms=ARENA_PLATFORMS,bounds=54}={}){this.walls=walls;this.platforms=platforms;this.bounds=bounds;}
 surfaces(x,z){let ground=true;const floors=[];for(const p of this.platforms){if(inside(x,z,p)){if(!p.bridge)ground=false;floors.push(p.h);}for(const r of ramps(p)){const h=rampHeight(p,x,z,0,r);if(h!==null){if(h>.025)ground=false;floors.push(h);}}}if(ground)floors.push(0);return [...new Set(floors)].sort((a,b)=>a-b);}
 height(x,z,current=0,step=.6){let floor=0;for(const h of this.surfaces(x,z))if(h<=current+step&&h>floor)floor=h;return floor;}
 collides(x,z,r=.4,y=0){if(Math.abs(x)>this.bounds-1-r||Math.abs(z)>this.bounds-1-r)return true;for(const b of this.walls)if(y<b.h-.05&&inside(x,z,b,r))return true;for(const p of this.platforms){if(!p.bridge&&y<p.h-.6&&inside(x,z,p,r))return true;for(const ramp of ramps(p)){const h=rampHeight(p,x,z,r,ramp);if(h!==null&&y<h-.6)return true;}}return false;}
 walkSegment(a,b,r=.5){const count=Math.max(1,Math.ceil(Math.hypot(b.x-a.x,b.z-a.z)/.2));let y=a.y;for(let i=1;i<=count;i++){const t=i/count,x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t,h=this.height(x,z,y);if(Math.abs(h-y)>.6||this.collides(x,z,r,h))return false;y=h;}return Math.abs(y-b.y)<.6;}
 move(entity,dx,dz){const p=entity.position,r=entity.radius??.32,steps=Math.max(1,Math.ceil(Math.hypot(dx,dz)/.16));for(let i=0;i<steps;i++)for(const [axis,delta]of[['x',dx/steps],['z',dz/steps]]){const x=p.x+(axis==='x'?delta:0),z=p.z+(axis==='z'?delta:0),h=this.height(x,z,p.y);if(h>p.y+.6||(entity.navigationAgent&&h<p.y-.6)||this.collides(x,z,r,entity.navigationAgent?h:p.y))continue;p.x=x;p.z=z;if(entity.navigationAgent)p.y=h;}}
 clear(a,b){const ay=(a.y??0)+.65,by=(b.y??0)+.65;const hits=(box,low,high)=>{const span=interval(a,b,box);if(!span)return false;const y0=ay+(by-ay)*span[0],y1=ay+(by-ay)*span[1];return Math.max(y0,y1)>low+.015&&Math.min(y0,y1)<high-.015;};for(const wall of this.walls)if(hits(wall,0,wall.h))return false;for(const p of this.platforms){if(hits(p,p.bridge?p.h-.45:0,p.h))return false;for(const r of ramps(p)){const box=r.axis==='x'?{x:(r.lo+r.hi)/2,z:r.across,w:r.hi-r.lo,d:r.width}:{x:r.across,z:(r.lo+r.hi)/2,w:r.width,d:r.hi-r.lo};const span=interval(a,b,box);if(!span)continue;for(const t of span){const x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t,y=ay+(by-ay)*t;if(y>0&&y<(rampHeight(p,x,z,0,r)??0)-.02)return false;}}}return true;}
}
const STEPS=[[1,0],[-1,0],[0,1],[0,-1],[1,1],[1,-1],[-1,1],[-1,-1]];
class MinHeap{
 constructor(){this.ids=[];this.costs=[];}
 reset(){this.ids.length=this.costs.length=0;}
 push(id,cost){let i=this.ids.length;this.ids.push(id);this.costs.push(cost);while(i){const p=(i-1)>>1;if(this.costs[p]<=cost)break;this.ids[i]=this.ids[p];this.costs[i]=this.costs[p];i=p;}this.ids[i]=id;this.costs[i]=cost;}
 pop(){const id=this.ids[0],cost=this.costs[0],last=this.ids.pop(),value=this.costs.pop();if(this.ids.length){let i=0;while(true){let c=i*2+1;if(c>=this.ids.length)break;if(c+1<this.ids.length&&this.costs[c+1]<this.costs[c])c++;if(this.costs[c]>=value)break;this.ids[i]=this.ids[c];this.costs[i]=this.costs[c];i=c;}this.ids[i]=last;this.costs[i]=value;}return[id,cost];}
}
/** One reverse Dijkstra field per rider cell/layer, shared by the entire crowd. */
export class LayeredNavigation{
 constructor(world,{cell=1,radius=.5}={}){
  this.world=world;this.cell=cell;this.radius=radius;this.N=Math.ceil(world.bounds*2/cell);this.origin=-world.bounds;this.cells=Array.from({length:this.N*this.N},()=>[]);this.nodes=[];this.goal=-1;this.rebuilds=0;this.heap=new MinHeap();
  for(let iz=0;iz<this.N;iz++)for(let ix=0;ix<this.N;ix++){const x=this.origin+(ix+.5)*cell,z=this.origin+(iz+.5)*cell;for(const y of world.surfaces(x,z))if(!world.collides(x,z,radius,y)){const id=this.nodes.length;this.nodes.push({x,y,z,ix,iz});this.cells[iz*this.N+ix].push(id);}}
  this.edges=new Int32Array(this.nodes.length*8).fill(-1);this.weights=new Float32Array(this.edges.length);this.distance=new Float64Array(this.nodes.length);this.next=new Int32Array(this.nodes.length);
  for(let i=0;i<this.nodes.length;i++){const a=this.nodes[i];for(let k=0;k<STEPS.length;k++){const[dx,dz]=STEPS[k],x=a.ix+dx,z=a.iz+dz;if(x<0||z<0||x>=this.N||z>=this.N)continue;for(const j of this.cells[z*this.N+x]){const b=this.nodes[j];if(Math.abs(a.y-b.y)>.55||!world.walkSegment(a,b,radius))continue;this.edges[i*8+k]=j;this.weights[i*8+k]=Math.hypot(a.x-b.x,a.z-b.z,a.y-b.y);break;}}}
 }
 cellAt(x,z){const ix=Math.max(0,Math.min(this.N-1,Math.floor((x-this.origin)/this.cell))),iz=Math.max(0,Math.min(this.N-1,Math.floor((z-this.origin)/this.cell)));return[ ix,iz ];}
 locate(p){const[ix,iz]=this.cellAt(p.x,p.z);let best=-1,cost=Infinity;for(let ring=0;ring<=2;ring++){for(let z=Math.max(0,iz-ring);z<=Math.min(this.N-1,iz+ring);z++)for(let x=Math.max(0,ix-ring);x<=Math.min(this.N-1,ix+ring);x++)for(const id of this.cells[z*this.N+x]){const n=this.nodes[id],dy=Math.abs(n.y-p.y),d=(n.x-p.x)**2+(n.z-p.z)**2+dy*dy*16;if(d<cost){best=id;cost=d;}}if(best>=0&&Math.abs(this.nodes[best].y-p.y)<.65)break;}return best;}
 update(goal){const first=this.locate(goal);this.target={x:goal.x,y:goal.y,z:goal.z};if(first===this.goal)return false;this.goal=first;this.rebuilds++;this.distance.fill(Infinity);this.next.fill(-1);this.heap.reset();if(first<0)return true;this.distance[first]=0;this.heap.push(first,0);while(this.heap.ids.length){const[i,d]=this.heap.pop();if(d!==this.distance[i])continue;for(let k=0;k<8;k++){const j=this.edges[i*8+k];if(j<0)continue;const nd=d+this.weights[i*8+k];if(nd>=this.distance[j])continue;this.distance[j]=nd;this.next[j]=i;this.heap.push(j,nd);}}return true;}
 direction(p,out={x:0,z:0}){const id=this.locate(p);let to=null;if(id>=0&&id===this.goal&&this.world.walkSegment(p,this.target,this.radius))to=this.target;else if(id>=0&&this.next[id]>=0)to=this.nodes[this.next[id]];let x=to?to.x-p.x:0,z=to?to.z-p.z:0;const l=Math.hypot(x,z);out.x=l>.001?x/l:0;out.z=l>.001?z/l:0;return out;}
 route(start,goal){this.update(goal);let id=this.locate(start),route=[];const seen=new Set();while(id>=0&&!seen.has(id)){seen.add(id);route.push(this.nodes[id]);if(id===this.goal)return route;id=this.next[id];}return [];}
}
