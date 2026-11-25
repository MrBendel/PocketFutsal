// Reduce the approved generated study into native, palette-indexed game pixels.
import fs from 'node:fs';
import {decodePng} from './assets.mjs';
import {encodePng} from '../harness/node.mjs';
const dir=new URL('../artwork/goals/v3/',import.meta.url);
const src=decodePng(fs.readFileSync(new URL('simplified-source.png',dir)));
const palette=[[0,0,0],[248,248,232],[136,152,168],[24,20,32]];
const crop=[231,148,1389,823],split=313,w=40,h=24;
function classify(x,y){const p=(Math.max(0,Math.min(src.height-1,y))*src.width+Math.max(0,Math.min(src.width-1,x)))*4;
 if(src.rgba[p+3]<192)return 0;
 let best=1,score=Infinity;
 for(let c=1;c<4;c++){const d=palette[c].reduce((a,v,k)=>a+(v-src.rgba[p+k])**2,0);if(d<score){score=d;best=c;}}
 return best;
}
const pixels=Array.from({length:h},()=>Array(w).fill(0));
for(let y=0;y<h;y++)for(let x=0;x<w/2;x++){
 const counts=[0,0,0,0],ya=y<8?crop[1]+y*(split-crop[1])/8:split+(y-8)*(crop[3]-split)/16;
 const yh=y<8?(split-crop[1])/8:(crop[3]-split)/16;
 for(const xx of [x,w-1-x])for(let sy=0;sy<5;sy++)for(let sx=0;sx<5;sx++){
  const px=Math.floor(crop[0]+(xx+(sx+.5)/5)*(crop[2]-crop[0])/w),py=Math.floor(ya+(sy+.5)*yh/5);
  counts[classify(px,py)]++;
 }
 const color=counts.indexOf(Math.max(...counts));pixels[y][x]=pixels[y][w-1-x]=color;
}
// Pixel cleanup: keep the front tubing bold at game size and align its lower
// white crossbar row with height 14. The reference supplies the rear geometry.
for(let x=1;x<39;x++){pixels[8][x]=3;pixels[9][x]=pixels[10][x]=1;}
for(let y=10;y<23;y++)for(const x of [1,2,37,38])pixels[y][x]=1;
for(let y=10;y<24;y++)pixels[y][0]=pixels[y][39]=3;
for(const x of [0,1,2,3,36,37,38,39])pixels[23][x]=3;
// Recover the thin central net/roof strands that area reduction drops.
for(let y=11;y<19;y++)pixels[y][19]=pixels[y][20]=2;
for(let x=4;x<36;x++)pixels[7][x]=0;
for(let y=2;y<8;y++)pixels[y][15]=pixels[y][24]=2;
const rows=pixels.map(r=>r.map(c=>c?String(c):'.').join(''));
fs.writeFileSync(new URL('native.json',dir),JSON.stringify({source:'simplified-source.png',width:w,height:h,crop,sourceFrontBar:split,palette:palette.slice(1),pixels:rows},null,2)+'\n');
const rgb=new Uint8Array(w*h*3);
for(let y=0;y<h;y++)for(let x=0;x<w;x++)rgb.set(pixels[y][x]?palette[pixels[y][x]]:[48,144,184],(y*w+x)*3);
for(const scale of [1,4,8])fs.writeFileSync(new URL(`native-${scale}x.png`,dir),encodePng(rgb,{width:w,height:h,scale}));
console.log(rows.join('\n'));
