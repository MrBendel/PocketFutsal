// Source-derived bicycle poses; no rotated or squeezed standing sprites.
import fs from 'node:fs';
import {decodePng} from './assets.mjs';
import {character} from './player-art.mjs';
import {silhouettes,reducePose,writePng,writeGif,spriteColumns} from './study-raster.mjs';
const dir=new URL('../artwork/players/bicycle/v1/',import.meta.url);
const im=decodePng(fs.readFileSync(new URL('generated-source.png',dir)));
const {labels,frames}=silhouettes(im,4,6,240),ids=['spiky','cap','crop','bandana'];
const phases=['tuck','strike','recover'];
let compact;
for(const size of [24,32]){
 const scale=Math.min(size/Math.max(...frames.map(f=>f.right-f.left+1)),(size-1)/Math.max(...frames.map(f=>f.bottom-f.top+1)));
 const characters=ids.map((id,i)=>{
  const palette=[[0,0,0,0],i===2?[184,120,72,255]:[248,200,136,255],i<2?[40,120,232,255]:[232,64,72,255],[24,32,48,255]];
  const poses=frames.filter(f=>f.row===i).map(source=>{
   const rows=reducePose(im,labels,source,size,size,scale,i>=2);
   return {phase:phases[source.col%3],view:source.col<3?'front':'back',lift:[5,11,0][source.col%3],rows,spriteColumns8x16:spriteColumns(rows),source};
  });return {id,palette,front:poses.slice(0,3),back:poses.slice(3)};
 });
 const data={version:1,width:size,height:size,alphaThreshold:240,sourceScale:scale,phases,characters};if(size===24)compact=data;
 fs.writeFileSync(new URL(`native-${size}.json`,dir),JSON.stringify(data,null,2)+'\n');
 writePng(new URL(`native-${size}.png`,dir),size*6,size*4,(x,y)=>{const c=characters[Math.floor(y/size)],f=[...c.front,...c.back][Math.floor(x/size)];return c.palette[Number(f.rows[y%size][x%size])||0];});
 const cw=size+4,s=5;
 writePng(new URL(`native-${size}-preview.png`,dir),cw*3*s,cw*8*s,(xx,yy)=>{
  const x=Math.floor(xx/s),y=Math.floor(yy/s),row=Math.floor(y/cw),c=characters[Math.floor(row/2)],f=c[row%2?'back':'front'][Math.floor(x/cw)],lx=x%cw-2,ly=y%cw-2;
  const v=lx>=0&&ly>=0&&lx<size&&ly<size?Number(f.rows[ly][lx])||0:0;
  return v?c.palette[v]:((Math.floor(x/4)+Math.floor(y/4))%2?[56,72,88,255]:[64,80,96,255]);
 });
}
const colors=[[40,56,72],[248,200,136],[184,120,72],[40,120,232],[232,64,72],[24,32,48],[248,248,232],[88,112,128]];
const w=96,h=152,lift=[0,5,11,11,0,0],delay=[22,14,14,16,20,20];
const animation=delay.map((d,phase)=>{
 const pixels=new Uint8Array(w*h);
 compact.characters.forEach((c,row)=>{
  for(let view=0;view<2;view++){
   const baseline=row*38+34,cx=24+48*view,ready=character(c.id).views[view][3];
   const rows=phase===0||phase===5?ready:c[view?'back':'front'][phase===1?0:phase===4?2:1].rows;
   for(let x=cx-5;x<cx+5;x++)pixels[(baseline+1)*w+x]=5;
   rows.forEach((r,y)=>[...r].forEach((v,x)=>{if(v==='.')return;const xx=cx-Math.floor(rows[0].length/2)+x,yy=baseline-rows.length+1+y-lift[phase];if(yy>=0&&yy<h)pixels[yy*w+xx]=v==='1'?(row===2?2:1):v==='2'?(row<2?3:4):5;}));
  }
 });return {pixels,delay:d};
});
writeGif(new URL('bicycle-loop.gif',dir),w,h,colors,animation,4);
console.log('Reduced 24 front/back bicycle poses to 24x24 and 32x32; generated animation preview.');
