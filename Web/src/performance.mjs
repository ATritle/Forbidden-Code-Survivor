// Budgets affect presentation only; enemy counts, damage and movement stay shared.
export const QUALITY_PROFILES=Object.freeze({
 low:Object.freeze({maxPixels:960*540,maxDpr:1,particles:900,rain:450,animationHz:20,steeringHz:10,lamps:0,flashes:0,shadows:false,bloom:false}),
 high:Object.freeze({maxPixels:1920*1080,maxDpr:1.25,particles:3000,rain:1500,animationHz:30,steeringHz:15,lamps:4,flashes:2,shadows:true,bloom:true})
});
export function renderPixelRatio(width,height,dpr,quality,scale=1){const p=QUALITY_PROFILES[quality]??QUALITY_PROFILES.low;return Math.min(Math.max(.1,dpr),p.maxDpr,Math.sqrt(p.maxPixels/Math.max(1,width*height)))*Math.max(.6,Math.min(1,scale));}
export class AdaptiveResolution{
 constructor(){this.reset();}
 reset(){this.scale=1;this.elapsed=0;this.frames=0;this.fast=0;}
 sample(seconds){if(!Number.isFinite(seconds)||seconds<=0||seconds>.25)return false;this.elapsed+=seconds;this.frames++;if(this.elapsed<1)return false;const ms=this.elapsed*1000/this.frames;this.elapsed=0;this.frames=0;let next=this.scale;if(ms>20.5){next=Math.max(.6,this.scale*.85);this.fast=0;}else if(ms<17.5){if(++this.fast>=5){next=Math.min(1,this.scale+.05);this.fast=0;}}else this.fast=0;if(Math.abs(next-this.scale)<.001)return false;this.scale=next;return true;}
}
export function segmentHitsRect(ax,az,bx,bz,rect,padding=.02){let lo=0,hi=1;for(const[start,delta,center,half]of[[ax,bx-ax,rect.x,rect.w/2+padding],[az,bz-az,rect.z,rect.d/2+padding]]){if(Math.abs(delta)<1e-9){if(start<center-half||start>center+half)return false;continue;}let a=(center-half-start)/delta,b=(center+half-start)/delta;if(a>b)[a,b]=[b,a];lo=Math.max(lo,a);hi=Math.min(hi,b);if(lo>hi)return false;}return true;}

// Opt-in repeatable render/CPU measurement; never enabled in normal play.
export class FrameProbe {
 constructor(enabled,quality){this.enabled=enabled;this.quality=quality;this.start=0;this.previous=0;this.samples=[];this.done=false;if(enabled){this.element=document.createElement('pre');this.element.id='performance-report';this.element.style.cssText='position:fixed;left:8px;bottom:8px;z-index:30;margin:0;padding:12px;background:#06121fed;color:#82ffe0;font:11px monospace;pointer-events:none;max-width:95vw';this.element.textContent='PERFORMANCE TEST / loading';document.body.append(this.element);}}
 begin(now,active){this.cpuStart=performance.now();if(!this.enabled||this.done||!active)return;if(!this.start)this.start=now;this.elapsed=now-this.start;this.interval=this.previous?now-this.previous:0;this.previous=now;}
 end(renderer,activeEnemies){if(!this.enabled||this.done||!this.start)return;const cpu=performance.now()-this.cpuStart;if(this.elapsed>5000)this.samples.push({frame:this.interval,cpu,calls:renderer.info.render.calls,triangles:renderer.info.render.triangles});this.element.textContent=`PERFORMANCE TEST / ${Math.min(15,this.elapsed/1000).toFixed(1)}s / ${this.quality}\n${renderer.info.render.calls} draw calls / ${activeEnemies} enemies`;
 if(this.elapsed>=15000){const mean=key=>this.samples.reduce((n,s)=>n+s[key],0)/this.samples.length;const sorted=this.samples.map(s=>s.frame).sort((a,b)=>a-b);this.result={quality:this.quality,samples:this.samples.length,fps:1000/mean('frame'),frameMs:mean('frame'),p95FrameMs:sorted[Math.floor(sorted.length*.95)],cpuMs:mean('cpu'),drawCalls:mean('calls'),triangles:mean('triangles'),activeEnemies,renderWidth:renderer.domElement.width,renderHeight:renderer.domElement.height};this.element.textContent=JSON.stringify(this.result,null,2);this.done=true;}}
}
