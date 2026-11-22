import fs from 'node:fs';
import zlib from 'node:zlib';
import assert from 'node:assert/strict';
import {CHARACTERS,character,fullTiles,basicTiles} from './player-art.mjs';
const root=new URL('../artwork/players/roster/',import.meta.url);
const names=['idle','run-a','run-b','ready','kick','volley','header','bicycle','win','lose'];
function png(file,w,h,pixel) {
 const raw=Buffer.alloc((w*4+1)*h);
 for(let y=0;y<h;y++) for(let x=0;x<w;x++) raw.set(pixel(x,y),(w*4+1)*y+1+x*4);
 const crc=b=>{let c=0xffffffff;for(const v of b){c^=v;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;}return(c^0xffffffff)>>>0;};
 const chunk=(name,data)=>{const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(name,4);data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,-4)),b.length-4);return b;};
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=6;
 fs.writeFileSync(file,Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
const roster=CHARACTERS.map(character);
for(const c of roster) {
 assert.equal(fullTiles(c.id).length,224); assert.equal(basicTiles(c.id).length,64);
 for(const p of c.views.flat()) {assert.equal(p.length,24);for(const row of p){assert.equal(row.length,16);assert.match(row,/^[.123]+$/);}}
 for(const team of ['blue','red']) {
 const pal=c.palette.map(v=>[...v]);if(team==='red')pal[2]=[232,64,72,255];
 png(new URL(`${c.id}/full-${team}.png`,root),160,48,(x,y)=>pal[Number(c.views[y<24?0:1][Math.floor(x/16)][y%24][x%16].replace('.','0'))]);
 png(new URL(`${c.id}/transitions-${team}.png`,root),48,48,(x,y)=>pal[Number(c.transitions[y<24?0:1][Math.floor(x/16)][y%24][x%16].replace('.','0'))]);
 png(new URL(`${c.id}/slide-${team}.png`,root),24,16,(x,y)=>pal[Number(c.slide[y][x].replace('.','0'))]);
 }
 fs.writeFileSync(new URL(`${c.id}/moves.json`,root),JSON.stringify({names,front:c.views[0],back:c.views[1],transitions:c.transitions,slide:c.slide},null,2)+'\n');
}
png(new URL('roster-preview.png',root),704,896,(x,y)=>{
 const px=Math.floor(x/4),py=Math.floor(y/4),cx=Math.floor(px/88),cy=Math.floor(py/112),c=roster[cy*2+cx];
 const lx=px%88-4,ly=py%112-8;
 const bg=((px>>1)+(py>>1))&1?[80,104,120,255]:[96,120,136,255];
 if(lx<0||lx>=80||ly<0||ly>=96)return bg;
 const pose=Math.floor(ly/24)*5+Math.floor(lx/16),index=Number(c.views[pose<10?0:1][pose%10][ly%24][lx%16].replace('.','0'));
 if(!index)return bg;return cy===1&&index===2?[232,64,72,255]:c.palette[index];
});
const assets=Object.fromEntries(CHARACTERS.map(id=>[id,Object.fromEntries(['blue','red'].map(t=>[t,fs.readFileSync(new URL(`${id}/full-${t}.png`,root)).toString('base64')]))]));
fs.writeFileSync(new URL('review.html',root),`<!doctype html><meta charset="utf-8"><title>Pocket Futsal roster animations</title><style>body{background:#101830;color:#f8ecd0;font:16px system-ui;margin:32px}p{color:#bed0dd}button,select{font:inherit;padding:8px;margin:4px;background:#23405a;color:inherit}canvas{image-rendering:pixelated;background:#4098a8;margin:10px}article{display:inline-block;text-align:center}img{image-rendering:pixelated;max-width:100%}</style><h1>Pocket Futsal · four-player roster</h1><p>16×24 sprites · 10 poses in each view, plus 24×16 slide. Header and bicycle are native pixel adaptations of the approved character art.</p><select id="view"><option value="0">Front</option><option value="1">Back</option></select><select id="pose"><option value="run">Run cycle</option>${names.map((n,i)=>`<option value="${i}">${n}</option>`).join('')}</select><button id="pause">Pause</button><div>${CHARACTERS.map(id=>`<article><h2>${id}</h2><canvas id="${id}" width="128" height="192"></canvas><br><canvas id="${id}-native" width="16" height="24"></canvas></article>`).join('')}</div><p>All animations are available as full-blue.png / full-red.png and editable moves.json in each character folder.</p><img src="roster-preview.png" alt="All four characters, front and back move sets"><script>const data=${JSON.stringify(assets)},images={};for(const id in data){const im=new Image();im.src='data:image/png;base64,'+data[id][id==='crop'||id==='bandana'?'red':'blue'];images[id]=im}const pose=document.getElementById('pose'),view=document.getElementById('view');let t=0,last=0,paused=false;document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Play':'Pause'};function draw(now){if(last&&!paused)t+=now-last;last=now;const p=pose.value==='run'?[0,1,0,2][Math.floor(t/130)%4]:+pose.value;for(const id in images){const im=images[id];if(!im.complete||!im.naturalWidth)continue;for(const suffix of ['','-native']){const c=document.getElementById(id+suffix),ctx=c.getContext('2d');ctx.imageSmoothingEnabled=false;ctx.clearRect(0,0,c.width,c.height);ctx.drawImage(im,p*16,+view.value*24,16,24,0,0,c.width,c.height)}}requestAnimationFrame(draw)}requestAnimationFrame(draw)</script>`);
console.log('Verified 4 characters, 20 base + 6 transition poses + slide each; 224 full / 64 basic tiles per character.');

fs.writeFileSync(new URL('transition-review.html',root),`<!doctype html><meta charset="utf-8"><title>Pocket Futsal transition study</title><style>body{background:#101830;color:#f8ecd0;font:16px system-ui;margin:32px}canvas{image-rendering:pixelated;background:#4098a8}article{display:inline-block;margin:10px;text-align:center}select,button{font:inherit;padding:8px;margin:4px}p{color:#bed0dd}</style><h1>Move transitions</h1><p>Ground shadows stay fixed as the body rises. Timings here are slowed for inspection.</p><select id="move"><option>bicycle</option><option>slide</option></select><select id="view"><option value="0">front</option><option value="1">back</option></select><button id="pause">Pause</button><div>${CHARACTERS.map(id=>`<article><h2>${id}</h2><canvas id="${id}" width="144" height="240"></canvas></article>`).join('')}</div><script>
const roster=${JSON.stringify(roster)},move=document.getElementById('move'),view=document.getElementById('view');let t=0,last=0,paused=false;document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Play':'Pause'};
function draw(now){if(last&&!paused)t+=now-last;last=now;for(const c of roster){const v=+view.value,V=c.views[v],T=c.transitions[v];const seq=move.value==='bicycle'?[[V[3],0],[T[0],4],[V[7],12],[V[7],12],[T[0],8],[T[1],2],[V[3],0],[V[0],0]]:[[V[3],0],[c.slide,0],[c.slide,0],[c.slide,0],[T[2],0],[V[3],0],[V[0],0]];const [p,lift]=seq[Math.floor(t/120)%seq.length],canvas=document.getElementById(c.id),ctx=canvas.getContext('2d');ctx.clearRect(0,0,144,240);ctx.fillStyle='#206070';ctx.beginPath();ctx.ellipse(72,204,36,9,0,0,Math.PI*2);ctx.fill();const w=p[0].length,h=p.length;for(let y=0;y<h;y++)for(let x=0;x<w;x++){const k=+p[y][x];if(!k)continue;let color=c.palette[k];if((c.id==='crop'||c.id==='bandana')&&k===2)color=[232,64,72];ctx.fillStyle='rgb('+color.slice(0,3).join(',')+')';ctx.fillRect((12-w/2+x)*6,(34-h+y-lift)*6,6,6)}}requestAnimationFrame(draw)}requestAnimationFrame(draw);
</script>`);
// Native-size comparison is the primary readability check; 8x is for pixel QA.
for(const scale of [1,8]) png(new URL(`slide-heads-${scale}x.png`,root),96*scale,16*scale,(x,y)=>{
 const px=Math.floor(x/scale),py=Math.floor(y/scale),i=Math.floor(px/24),c=roster[i];
 const color=Number(c.slide[py][px%24].replace('.','0'));
 return color?(i>=2&&color===2?[232,64,72,255]:c.palette[color]):[48,144,184,255];
});
fs.copyFileSync(new URL('slide-heads-8x.png',root),new URL('slide-tackle-preview.png',root));
