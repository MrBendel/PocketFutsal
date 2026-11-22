// Technical export of the generated player study: fixed cells, shared palette,
// binary alpha, nearest-neighbor previews, and the engine's column-major 2bpp.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
const dir=process.argv[2] ? path.resolve(process.argv[2]) : fileURLToPath(new URL('../artwork/players/v1/',import.meta.url));
const skin=process.argv[3]==='dark'?[184,120,72,255]:[248,200,136,255];
const source=decodePng(fs.readFileSync(path.join(dir,'generated-source.png')));
const colors=[[0,0,0,0],skin,[40,120,232,255],[24,32,48,255]];
const frames=[];
function colorAt(x,y) {
 const p=(y*source.width+x)*4;
 if(source.rgba[p+3]<192) return 0;
 let best=3, error=Infinity;
 for(let c=1;c<=3;c++) {
  let e=0; for(let k=0;k<3;k++) e+=(source.rgba[p+k]-colors[c][k])**2;
  if(e<error) {error=e;best=c;}
 }
 return best;
}
for(let row=0;row<2;row++) for(let col=0;col<4;col++) {
 const x0=Math.round(col*source.width/4),x1=Math.round((col+1)*source.width/4);
 const y0=Math.round(row*source.height/2),y1=Math.round((row+1)*source.height/2);
 let left=x1,right=x0,top=y1,bottom=y0;
 for(let y=y0;y<y1;y++) for(let x=x0;x<x1;x++) if(colorAt(x,y)) {
  left=Math.min(left,x);right=Math.max(right,x);top=Math.min(top,y);bottom=Math.max(bottom,y);
 }
 assert.ok(right>left && bottom>top);
 frames.push({left,top,width:right-left+1,height:bottom-top+1});
}
// One common scale preserves head/body proportions between animation frames.
const scale=Math.min(16/Math.max(...frames.map(f=>f.width)),23/Math.max(...frames.map(f=>f.height)));
for(const f of frames) {
 const w=Math.max(1,Math.round(f.width*scale)),h=Math.max(1,Math.round(f.height*scale));
 const ox=Math.floor((16-w)/2),oy=23-h;
 f.pixels=new Uint8Array(16*24);
 for(let y=0;y<h;y++) for(let x=0;x<w;x++) {
  const votes=[0,0,0,0];
  const xa=Math.floor(f.left+x*f.width/w),xb=Math.ceil(f.left+(x+1)*f.width/w);
  const ya=Math.floor(f.top+y*f.height/h),yb=Math.ceil(f.top+(y+1)*f.height/h);
  for(let sy=ya;sy<yb;sy++) for(let sx=xa;sx<xb;sx++) votes[colorAt(sx,sy)]++;
  const total=votes.reduce((a,b)=>a+b,0);
  if(votes[0]>total*0.6) continue;
  const c=votes[1]>=votes[2] && votes[1]>=votes[3]?1:votes[2]>=votes[3]?2:3;
  f.pixels[(oy+y)*16+ox+x]=c;
 }
}
function crc(buf) {let c=0xffffffff;for(const b of buf){c^=b;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;}return(c^0xffffffff)>>>0;}
function png(file,w,h,pixel) {
 const raw=Buffer.alloc((w*4+1)*h);
 for(let y=0;y<h;y++) for(let x=0;x<w;x++) raw.set(pixel(x,y),(w*4+1)*y+1+x*4);
 const chunk=(name,data)=>{const out=Buffer.alloc(data.length+12);out.writeUInt32BE(data.length);out.write(name,4);data.copy(out,8);out.writeUInt32BE(crc(out.subarray(4,-4)),out.length-4);return out;};
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=6;
 fs.writeFileSync(path.join(dir,file),Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
const poses=['idle','run-a','run-b','kick'];
for(const team of ['blue','red']) {
 const pal=colors.map(c=>[...c]); if(team==='red') pal[2]=[232,64,72,255];
 png(`${team}-sheet.png`,64,48,(x,y)=>pal[frames[Math.floor(y/24)*4+Math.floor(x/16)].pixels[(y%24)*16+x%16]]);
 frames.forEach((f,i)=>png(`${team}-${i<4?'front':'back'}-${poses[i%4]}.png`,16,24,(x,y)=>pal[f.pixels[y*16+x]]));
}
// Four rows: blue front/back, then red front/back; 8x integer magnification.
png('preview-8x.png',640,896,(x,y)=>{
 const px=Math.floor(x/8),py=Math.floor(y/8),col=Math.floor(px/20),row=Math.floor(py/28);
 const lx=px%20-2,ly=py%28-2;
 const bg=((Math.floor(px/2)+Math.floor(py/2))&1)?[92,108,120,255]:[108,124,136,255];
 if(lx<0||lx>=16||ly<0||ly>=24)return bg;
 const c=frames[(row%2)*4+col].pixels[ly*16+lx];
 if(!c)return bg;return row>=2&&c===2?[232,64,72,255]:colors[c];
});
const tiles=[];
for(const f of frames) for(let col=0;col<2;col++) for(let tileY=0;tileY<4;tileY++) for(let y=0;y<8;y++) {
 let lo=0,hi=0;
 for(let x=0;x<8;x++) {const sy=tileY*8+y,c=sy<24?f.pixels[sy*16+col*8+x]:0;lo|=(c&1)<<(7-x);hi|=(c>>1)<<(7-x);}
 tiles.push(lo,hi);
}
assert.equal(tiles.length,8*128);
fs.writeFileSync(path.join(dir,'player.2bpp'),Buffer.from(tiles));
const metadata={frameWidth:16,frameHeight:24,sheetWidth:64,sheetHeight:48,columns:4,rows:2,transparentIndex:0,palette:colors,redTeamColor:[232,64,72,255],tileLayout:'8 tiles per frame: left top/middle/bottom/blank, right top/middle/bottom/blank',frames:frames.map((f,i)=>({name:`${i<4?'front':'back'}-${poses[i%4]}`,index:i,rows:Array.from({length:24},(_,y)=>Array.from(f.pixels.slice(y*16,y*16+16),n=>n?String(n):'.').join(''))}))};
fs.writeFileSync(path.join(dir,'player.json'),JSON.stringify(metadata,null,2)+'\n');
// Validate actual files, not just the intended dimensions.
for(const team of ['blue','red']) {
 const p=decodePng(fs.readFileSync(path.join(dir,`${team}-sheet.png`)));
 assert.equal(p.width,64);assert.equal(p.height,48);
 const distinct=new Set();for(let i=0;i<p.rgba.length;i+=4)distinct.add(p.rgba.subarray(i,i+4).toString('hex'));
 assert.equal(distinct.size,4);
}
for(let i=0;i<8;i++) for(let y=0;y<24;y++) for(let x=0;x<16;x++) {
 const offset=i*128+Math.floor(x/8)*64+Math.floor(y/8)*16+(y%8)*2,bit=7-x%8;
 assert.equal(((tiles[offset]>>bit)&1)|(((tiles[offset+1]>>bit)&1)<<1),frames[i].pixels[y*16+x]);
}
console.log('Exported 8 frames, 16x24 each; blue/red sheets; 3 opaque colors + binary transparency; 2bpp round-trip verified.');
// Self-contained local review, including true native-size rendering.
const blue=fs.readFileSync(path.join(dir,'blue-sheet.png')).toString('base64');
const red=fs.readFileSync(path.join(dir,'red-sheet.png')).toString('base64');
fs.writeFileSync(path.join(dir,'review.html'),`<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Pocket Futsal — 16×24 player study</title><style>
body{margin:32px;background:#101830;color:#f8ecd0;font:16px system-ui;max-width:920px}h1{font-size:26px}p{color:#b7c8d8;line-height:1.6}button,select{font:inherit;padding:8px;margin:4px;background:#223653;color:inherit;border:1px solid #647589;border-radius:4px}canvas{image-rendering:pixelated;background:#4098a8;margin:12px;vertical-align:bottom}figure{display:inline-block;margin:8px}figcaption{color:#b7c8d8}.sheet{width:512px;max-width:100%;image-rendering:pixelated;background:#607080}a{color:#80b8ff}
</style><h1>Pocket Futsal · player study v1</h1><p>Actual 16×24 pixels · three visible colors plus transparency · front and back views.<br>First pass for review; the live game's sprites have not been replaced.</p>
<label>Team <select id="team"><option>blue</option><option>red</option></select></label>
<label>View <select id="view"><option value="0">front</option><option value="1">back</option></select></label>
<label>Pose <select id="pose"><option value="run">run cycle</option><option value="0">idle</option><option value="1">run A</option><option value="2">run B</option><option value="3">kick</option></select></label>
<button id="pause">Pause</button><div><figure><canvas id="native" width="16" height="24"></canvas><figcaption>Native · 1×</figcaption></figure><figure><canvas id="large" width="128" height="192"></canvas><figcaption>Enlarged · 8×</figcaption></figure></div>
<p>Columns: idle / run A / run B / kick. Rows: front / back. Pass and shot share this first kick pose; slide and additional directions are still to draw.</p><img class="sheet" id="sheet" alt="Blue player sheet"><p><a href="blue-sheet.png">Blue PNG</a> · <a href="red-sheet.png">Red PNG</a> · <a href="player.json">Frame data</a> · <a href="player.2bpp">Game Boy tiles</a> · <a href="prompt.txt">Generation prompt</a></p><script>
const images={blue:new Image(),red:new Image()};images.blue.src='data:image/png;base64,${blue}';images.red.src='data:image/png;base64,${red}';
const team=document.getElementById('team'),view=document.getElementById('view'),pose=document.getElementById('pose');let time=0,paused=false,previous=0;
document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Play':'Pause'};
function draw(now){if(previous&&!paused)time+=now-previous;previous=now;const frame=pose.value==='run'?[0,1,0,2][Math.floor(time/130)%4]:+pose.value;const image=images[team.value];if(image.complete&&image.naturalWidth){for(const id of ['native','large']){const c=document.getElementById(id),ctx=c.getContext('2d');ctx.imageSmoothingEnabled=false;ctx.clearRect(0,0,c.width,c.height);ctx.drawImage(image,frame*16,+view.value*24,16,24,0,0,c.width,c.height)}document.getElementById('sheet').src=image.src}requestAnimationFrame(draw)}requestAnimationFrame(draw);
</script></html>`);
