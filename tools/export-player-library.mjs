import fs from 'node:fs';
import assert from 'node:assert/strict';
import {CHARACTERS} from './player-art.mjs';
import {ANIMATION_ROWS,playerAnimations} from './player-animation-library.mjs';
import {writePng,spriteColumns} from './study-raster.mjs';
import {decodePng} from './assets.mjs';
const root=new URL('../artwork/players/roster/',import.meta.url);
const cell=32,columns=8,width=cell*columns,height=cell*ANIMATION_ROWS.length;
const font=decodePng(fs.readFileSync(new URL('../artwork/Fonts for GB Studio/light/default/ascii.png',import.meta.url)));
let total=0;const library=[];
for(const id of CHARACTERS){
 const c=playerAnimations(id),pixels=Array.from({length:height},()=>Array(width).fill('.'));
 const animations=c.animations.map(a=>({...a,frames:a.frames.map((f,i)=>{
  const h=f.rows.length,w=f.rows[0].length,ox=Math.floor((cell-w)/2),oy=cell-h;
  assert.ok(w<=cell&&h<=cell&&i<columns);
  for(const r of f.rows)assert.match(r,new RegExp(`^[.123]{${w}}$`));
  const x=i*cell+ox,y=a.sheetRow*cell+oy;
  f.rows.forEach((r,yy)=>[...r].forEach((v,xx)=>pixels[y+yy][x+xx]=v));
  total++;
  return {...f,rect:{x,y,width:w,height:h},origin:{x:Math.floor(w/2),y:h-1},cell:{x:i*cell,y:a.sheetRow*cell,width:cell,height:cell},spriteColumns8x16:spriteColumns(f.rows)};
 })}));
 const data={version:1,id,width,height,cellWidth:cell,cellHeight:cell,columns,palette:c.palette,paletteRed:c.palette.map((v,i)=>i===2?[232,64,72,255]:v),paletteBlue:c.palette.map((v,i)=>i===2?[40,120,232,255]:v),animations};
 library.push(data);
 for(const team of ['blue','red']){
  const palette=data[team==='blue'?'paletteBlue':'paletteRed'];
  writePng(new URL(`${id}/complete-${team}.png`,root),width,height,(x,y)=>palette[Number(pixels[y][x])||0]);
 }
 // Indexed tile archive in column-major order within each native frame.
 // This is not a single resident VRAM bank; offsets describe uploadable frames.
 const bytes=[];let offset=0;
 for(const a of animations)for(const f of a.frames){
  const w=f.rect.width,h=f.rect.height;
  f.tiles={byteOffset:offset,width:Math.ceil(w/8),height:Math.ceil(h/8)};
  for(let x=0;x<w;x+=8)for(let y=0;y<h;y+=8)for(let yy=0;yy<8;yy++){
   let lo=0,hi=0;for(let xx=0;xx<8;xx++){const v=Number(f.rows[y+yy]?.[x+xx])||0;lo|=(v&1)<<(7-xx);hi|=((v>>1)&1)<<(7-xx);}bytes.push(lo,hi);
  }
  offset=bytes.length;
 }
 fs.writeFileSync(new URL(`${id}/complete.2bpp`,root),Buffer.from(bytes));
 fs.writeFileSync(new URL(`${id}/complete.json`,root),JSON.stringify(data,null,2)+'\n');
 // Labeled review rows are separate from native atlas rectangles.
 writePng(new URL(`${id}/complete-preview.png`,root),width*3,44*ANIMATION_ROWS.length*3,(xx,yy)=>{
  const x=Math.floor(xx/3),py=Math.floor(yy/3),row=Math.floor(py/44),ly=py%44;
  if(ly<12){
   const text=ANIMATION_ROWS[row].toUpperCase()+(c.animations.some(a=>a.sheetRow===row)?'':' (NOT AUTHORED)'),char=text[Math.floor((x-4)/8)];
   if(x>=4&&char&&ly>=2&&ly<10){const code=char.charCodeAt(0)-32,p=((Math.floor(code/16)*8+ly-2)*font.width+(code%16)*8+(x-4)%8)*4;if(font.rgba[p]+font.rgba[p+1]+font.rgba[p+2]<300)return [248,240,208,255];}
   return [24,32,48,255];
  }
  const y=row*32+ly-12,v=Number(pixels[y][x])||0;
  return v?c.palette[v]:((Math.floor(x/4)+Math.floor(y/4))%2?[56,72,88,255]:[64,80,96,255]);
 });
}
fs.writeFileSync(new URL('complete-review.html',root),`<!doctype html><meta charset="utf-8"><title>Complete player animations</title><style>body{background:#182030;color:#f8ecd0;font:16px system-ui;margin:24px}select,button{font:inherit;padding:8px;margin:4px}article{display:inline-block;text-align:center;margin:12px}canvas,img{image-rendering:pixelated}canvas{background:#384858}img{max-width:100%}a{color:#8cd8ff}</style><h1>Complete player sprite library</h1><p>Existing moves plus generated wins, losses, slides and the spiky keeper's dives. Preview only; gameplay timing and VRAM uploads are configured separately.</p><select id="animation">${ANIMATION_ROWS.map(n=>`<option>${n}</option>`).join('')}</select><select id="team"><option>blue</option><option>red</option></select><label><input type="checkbox" id="mirror">Mirror directional moves</label><button id="pause">Pause</button><div>${CHARACTERS.map(id=>`<article><h2>${id}</h2><canvas id="${id}" width="192" height="240"></canvas><p id="${id}-phase"></p><a href="${id}/complete-blue.png">Blue sheet</a> · <a href="${id}/complete-red.png">Red sheet</a> · <a href="${id}/complete.json">Frame data</a></article>`).join('')}</div><h2>Sheet layout</h2><p>Each 32×32 cell preserves the native frame size, centered horizontally and aligned at the feet. Rows: ${ANIMATION_ROWS.join(', ')}. The last two rows are reserved for keeper dives and populated only on the spiky sheet.</p><script>const players=${JSON.stringify(library)},selector=document.getElementById('animation'),team=document.getElementById('team'),mirror=document.getElementById('mirror');let t=0,last=0,paused=false;document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Play':'Pause'};selector.onchange=()=>t=0;function draw(now){if(last&&!paused)t+=now-last;last=now;for(const p of players){const canvas=document.getElementById(p.id),ctx=canvas.getContext('2d'),a=p.animations.find(a=>a.name===selector.value);ctx.clearRect(0,0,192,240);const label=document.getElementById(p.id+'-phase');if(!a){label.textContent='Keeper animation not authored for this player';continue}const f=a.frames[Math.floor(t/140)%a.frames.length],pal=p[team.value==='red'?'paletteRed':'paletteBlue'],w=f.rows[0].length,h=f.rows.length,flip=mirror.checked&&a.mirrorForLeft;label.textContent=f.name;ctx.fillStyle='#182030';ctx.fillRect(66,211,60,6);f.rows.forEach((r,y)=>[...r].forEach((v,x)=>{if(v==='.')return;ctx.fillStyle='rgb('+pal[+v].slice(0,3).join(',')+')';ctx.fillRect((16-w/2+(flip?w-1-x:x))*6,(35-h+y-(f.lift||0))*6,6,6)}))}requestAnimationFrame(draw)}requestAnimationFrame(draw)</script>`);
console.log(`Exported ${total} native frames across four player libraries (blue/red PNG, JSON, 2bpp, preview).`);
