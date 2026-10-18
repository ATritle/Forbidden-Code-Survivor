import * as T from 'three';

// Low retains textures/emission and a cheap wet specular response, without PBR.
const lowMaterials=new Map(),lodByGeometry=new WeakMap();
export function qualityMaterial(material,low){
 const original=material.userData.fullMaterial??material;
 if(!low||!original.isMeshStandardMaterial)return original;
 if(lowMaterials.has(original.uuid))return lowMaterials.get(original.uuid);
 const m=new T.MeshPhongMaterial({name:original.name+' / low',color:original.color,map:original.map,emissive:original.emissive,emissiveMap:original.emissiveMap,emissiveIntensity:original.emissiveIntensity,shininess:45,specular:0x35475b,side:original.side,transparent:original.transparent,opacity:original.opacity,alphaTest:original.alphaTest,depthWrite:original.depthWrite,vertexColors:original.vertexColors});
 m.userData.fullMaterial=original;
 if(original.userData.worldPavement!==undefined){
  if(original.roughnessMap?.image){const image=original.roughnessMap.image,c=document.createElement('canvas');c.width=image.width;c.height=image.height;const g=c.getContext('2d');g.drawImage(image,0,0);g.globalCompositeOperation='difference';g.fillStyle='white';g.fillRect(0,0,c.width,c.height);m.specularMap=new T.CanvasTexture(c);m.specularMap.wrapS=m.specularMap.wrapT=T.RepeatWrapping;}
  const scale=original.userData.worldPavement?'.25':'.75';m.onBeforeCompile=shader=>{shader.vertexShader=shader.vertexShader.replace('#include <begin_vertex>',`#include <begin_vertex>\nvec4 pavementPosition=vec4(position,1.0);\n#ifdef USE_INSTANCING\npavementPosition=instanceMatrix*pavementPosition;\n#endif\nvec2 pavementXZ=(modelMatrix*pavementPosition).xz;\nvMapUv=pavementXZ*${scale};\n#ifdef USE_SPECULARMAP\nvSpecularMapUv=pavementXZ*.08;\n#endif`);};m.customProgramCacheKey=()=> 'low-pavement-'+scale;
 }
 lowMaterials.set(original.uuid,m);return m;
}
export function applyRenderQuality(root,quality){root.traverse(o=>{if(!o.isMesh)return;const lod=lodByGeometry.get(o.geometry);if(lod){const next=quality==='low'?lod.low:lod.full;if(o.geometry!==next){o.geometry=next;o.boundingSphere=null;o.boundingBox=null;}}if(Array.isArray(o.material))o.material=o.material.map(m=>qualityMaterial(m,quality==='low'));else o.material=qualityMaterial(o.material,quality==='low');});}

// Bone indices are remapped by name after export; animations keep the original rig.
export function attachLowGeometry(full,low){
 full.scene.updateMatrixWorld(true);low.scene.updateMatrixWorld(true);
 const fullMeshes=[],lowMeshes=[];full.scene.traverse(o=>{if(o.isSkinnedMesh)fullMeshes.push(o);});low.scene.traverse(o=>{if(o.isSkinnedMesh)lowMeshes.push(o);});
 if(fullMeshes.length!==lowMeshes.length)throw Error('LOD mesh count differs');
 for(let i=0;i<fullMeshes.length;i++){
  const mesh=fullMeshes[i],reduced=lowMeshes.find(o=>o.name===mesh.name)??lowMeshes[i],geometry=reduced.geometry.clone();
  const boneIndices=new Map(mesh.skeleton.bones.map((b,j)=>[b.name,j]));const remap=reduced.skeleton.bones.map(b=>{const j=boneIndices.get(b.name);if(j===undefined)throw Error('LOD bone missing: '+b.name);return j;});
  const indices=geometry.attributes.skinIndex;
  for(let j=0;j<indices.count;j++)indices.setXYZW(j,remap[indices.getX(j)],remap[indices.getY(j)],remap[indices.getZ(j)],remap[indices.getW(j)]);
  geometry.applyMatrix4(new T.Matrix4().copy(mesh.matrixWorld).invert().multiply(reduced.matrixWorld));
  const pair={full:mesh.geometry,low:geometry};lodByGeometry.set(pair.full,pair);lodByGeometry.set(pair.low,pair);
 }
}
