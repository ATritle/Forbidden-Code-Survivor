import test from 'node:test';
import assert from 'node:assert/strict';
import {QUALITY_PROFILES,renderPixelRatio,AdaptiveResolution,segmentHitsRect} from '../src/performance.mjs';
import {readFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';

test('low and high enforce pixel budgets on 4K and high-DPI displays',()=>{
 for(const quality of ['low','high'])for(const [w,h,dpr]of [[3840,2160,2],[1920,1080,1],[642,930,3]]){
  const ratio=renderPixelRatio(w,h,dpr,quality);
  assert.ok(w*h*ratio*ratio<=QUALITY_PROFILES[quality].maxPixels+1);
  assert.ok(ratio<=QUALITY_PROFILES[quality].maxDpr);
 }
 assert.equal(QUALITY_PROFILES.low.lamps,0);assert.equal(QUALITY_PROFILES.low.flashes,0);
 assert.ok(QUALITY_PROFILES.low.particles<QUALITY_PROFILES.high.particles);
});
test('adaptive resolution lowers GPU load with a floor and restores it slowly',()=>{
 const controller=new AdaptiveResolution();
 for(let i=0;i<1000;i++)controller.sample(1/25);
 assert.equal(controller.scale,.6);
 for(let i=0;i<600;i++)controller.sample(1/60);
 assert.ok(controller.scale>.6&&controller.scale<1);
 const saved=controller.scale;controller.sample(10);assert.equal(controller.scale,saved);
 controller.reset();assert.equal(controller.scale,1);
});
test('exact sight segments reject walls without skipping thin blockers',()=>{
 const wall={x:0,z:0,w:.1,d:4};
 assert.equal(segmentHitsRect(-10,0,10,0,wall),true);
 assert.equal(segmentHitsRect(10,0,-10,0,wall),true);
 assert.equal(segmentHitsRect(-10,3,10,3,wall),false);
 assert.equal(segmentHitsRect(-2,-5,-2,5,wall),false);
 assert.equal(segmentHitsRect(0,-5,0,5,wall),true);
 assert.equal(segmentHitsRect(0,0,0,0,wall),true);
 assert.equal(segmentHitsRect(2,2,2,2,wall),false);
});
test('low LODs preserve each original source and compatible skeletons',async()=>{
 const manifest=JSON.parse(await readFile(new URL('../assets/low-lod-manifest.json',import.meta.url)));
 assert.equal(manifest.models.length,11);
 const parse=b=>JSON.parse(b.toString('utf8',20,20+b.readUInt32LE(12)));
 for(const entry of manifest.models){
  const fullBytes=await readFile(new URL('../'+entry.sourcePath,import.meta.url));
  assert.equal(createHash('sha256').update(fullBytes).digest('hex'),entry.sourceSha256);
  const full=parse(fullBytes),low=parse(await readFile(new URL('../'+entry.path,import.meta.url)));
  assert.ok(!low.animations?.length);assert.ok(low.skins.length);
  assert.ok(entry.triangles<(entry.id==='NeonExorcist'?3500:entry.id==='BroadcastSovereign'?2100:1100));
  const names=d=>new Set(d.skins.flatMap(s=>s.joints.map(j=>d.nodes[j].name)));
  assert.deepEqual(names(low),names(full));
 }
});
