// Technical reduction of the generated sheet: binary alpha, one silhouette,
// a shared palette, area-vote sampling, and integer-scale review images.
import fs from 'node:fs';
import zlib from 'node:zlib';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
const root=new URL('../artwork/ball/v1/',import.meta.url);
const source=decodePng(fs.readFileSync(new URL('generated-source.png',root)));
const palette=[[0,0,0,0],[248,248,232,255],[136,152,168,255],[24,32,48,255]];
const cells=[];
for(let frame=0;frame<6;frame++){
 const ox=frame%3*source.width/3,oy=Math.floor(frame/3)*source.height/2;
 let left=Infinity,top=Infinity,right=0,bottom=0;
 for(let y=oy;y<oy+source.height/2;y++)for(let x=ox;x<ox+source.width/3;x++){
  if(source.rgba[(y*source.width+x)*4+3]<220)continue;
  left=Math.min(left,x);right=Math.max(right,x);top=Math.min(top,y);bottom=Math.max(bottom,y);
 }
 assert.ok(Number.isFinite(left));
 cells.push({cx:(left+right+1)/2,cy:(top+bottom+1)/2,width:right-left+1,height:bottom-top+1});
}
const diameter=Math.max(...cells.flatMap(c=>[c.width,c.height]));
function reduce(size){
 const r=size/2-.1,inside=(x,y)=>x>=0&&y>=0&&x<size&&y<size&&(x-(size-1)/2)**2+(y-(size-1)/2)**2<=r*r;
 return cells.map(cell=>Array.from({length:size},(_,y)=>Array.from({length:size},(_,x)=>{
  if(!inside(x,y))return '.';
  if(!inside(x-1,y)||!inside(x+1,y)||!inside(x,y-1)||!inside(x,y+1))return '3';
  const votes=[0,0,0,0];
  const xa=Math.floor(cell.cx-diameter/2+x*diameter/size),xb=Math.ceil(cell.cx-diameter/2+(x+1)*diameter/size);
  const ya=Math.floor(cell.cy-diameter/2+y*diameter/size),yb=Math.ceil(cell.cy-diameter/2+(y+1)*diameter/size);
  for(let sy=ya;sy<yb;sy++)for(let sx=xa;sx<xb;sx++){
   const p=(sy*source.width+sx)*4;if(source.rgba[p+3]<220)continue;
   let best=1,error=Infinity;
   for(let k=1;k<4;k++){let e=0;for(let c=0;c<3;c++)e+=(source.rgba[p+c]-palette[k][c])**2;if(e<error){error=e;best=k;}}
   votes[best]++;
  }
  return String(votes[1]>=votes[2]&&votes[1]>=votes[3]?1:votes[2]>=votes[3]?2:3);
 }).join('')));
}
function png(name,w,h,pixel){
 const raw=Buffer.alloc((w*4+1)*h);for(let y=0;y<h;y++)for(let x=0;x<w;x++)raw.set(pixel(x,y),(w*4+1)*y+1+x*4);
 const crc=b=>{let c=0xffffffff;for(const v of b){c^=v;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;}return(c^0xffffffff)>>>0;};
 const chunk=(name,data)=>{const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(name,4);data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,-4)),b.length-4);return b;};
 const header=Buffer.alloc(13);header.writeUInt32BE(w);header.writeUInt32BE(h,4);header[8]=8;header[9]=6;
 fs.writeFileSync(new URL(name,root),Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',header),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
// At 8px the generated panel seams merge into H-shaped blobs. Retain its
// white/gray/navy motif but simplify to one 2x2 panel travelling over the face,
// smaller side panels, and a fixed round silhouette. No resampled seams.
function cleanNative(){
 const mask=['...##...','.######.','.######.','########','########','.######.','.######.','...##...'];
 return [3,4,5,0,1,2].map((panelY,phase)=>mask.map((row,y)=>[...row].map((c,x)=>{
  if(c==='.')return '.';
  if(x>=3&&x<=4&&y>=panelY&&y<panelY+2)return '3';
  if((x===1&&y===(phase+1)%6+1)||(x===6&&y===(phase+4)%6+1))return '3';
  const edge=x===0||x===7||y===0||y===7||mask[y-1]?.[x]!== '#'||mask[y+1]?.[x]!== '#';
  return edge&&x+y>=7?'2':'1';
 }).join('')));
}
const sizes=[8,12,16],studies=sizes.map(size=>({size,frames:size===8?cleanNative():reduce(size)}));
const raw=reduce(8);png('sheet-8-raw.png',48,8,(x,y)=>palette[+raw[Math.floor(x/8)][y][x%8].replace('.','0')]);
for(const {size,frames}of studies){
 const masks=frames.map(p=>p.join('').replace(/[123]/g,'#'));assert.equal(new Set(masks).size,1);
 png(`sheet-${size}.png`,size*6,size,(x,y)=>palette[+(frames[Math.floor(x/size)][y][x%size].replace('.','0'))]);
}
png('scaling-preview.png',576,240,(x,y)=>{
 const row=Math.floor(y/80),{size,frames}=studies[row],scale=row===0?8:row===1?5:4;
 const lx=Math.floor((x%96-(96-size*scale)/2)/scale),ly=Math.floor((y%80-(80-size*scale)/2)/scale);
 const k=lx>=0&&ly>=0&&lx<size&&ly<size?+frames[Math.floor(x/96)][ly][lx].replace('.','0'):0;
 return k?palette[k]:[48,144,184,255];
});
const frames=studies[0].frames;
fs.writeFileSync(new URL('ball.json',root),JSON.stringify({frameWidth:8,frameHeight:8,palette,frames:frames.map((rows,index)=>({index,rows})),sourceCells:cells},null,2)+'\n');
fs.writeFileSync(new URL('review.html',root),`<!doctype html><meta charset="utf-8"><title>Soccer ball spin study</title><style>body{background:#101830;color:#f8ecd0;font:16px system-ui;margin:32px}canvas{image-rendering:pixelated;background:#3090b8;margin:12px}article{display:inline-block;text-align:center}input{width:220px}</style><h1>Soccer ball · six-frame spin study</h1><p>Fixed silhouette and center. Three colors plus transparency. Top comparison row: 8px; middle: 12px; bottom: 16px.</p><label>Spin speed <input id="speed" type="range" min="0" max="120" value="45"></label><button id="pause">Pause</button><div>${sizes.map(s=>`<article><h2>${s}×${s}</h2><canvas id="ball-${s}" width="160" height="120"></canvas><br><canvas id="native-${s}" width="${s}" height="${s}"></canvas></article>`).join('')}</div><script>const studies=${JSON.stringify(studies)},pal=${JSON.stringify(palette)};let phase=0,last=0,paused=false;document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Play':'Pause'};function draw(now){if(last&&!paused)phase+=(now-last)*+document.getElementById('speed').value/6000;last=now;const f=Math.floor(phase)%6;for(const{size,frames}of studies)for(const prefix of ['ball-','native-']){const c=document.getElementById(prefix+size),ctx=c.getContext('2d'),scale=prefix==='native-'?1:6,ox=(c.width-size*scale)/2,oy=(c.height-size*scale)/2;ctx.clearRect(0,0,c.width,c.height);for(let y=0;y<size;y++)for(let x=0;x<size;x++){const k=+frames[f][y][x].replace('.','0');if(!k)continue;ctx.fillStyle='rgb('+pal[k].slice(0,3).join(',')+')';ctx.fillRect(ox+x*scale,oy+y*scale,scale,scale)}}requestAnimationFrame(draw)}requestAnimationFrame(draw)</script>`);
console.log('Exported six aligned 8/12/16px frames, native palette data, scaling comparison and animation review.');
