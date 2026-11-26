// Native tile reconstruction of the approved indoor-arena concept.
// Review art only: the live Shootout still imports mg_shootout.mjs.
import assert from 'node:assert/strict';
import {canvas} from './common.mjs';
import {BG as previous} from './mg_shootout.mjs';

export const PALETTES=[
 [[40,152,168],[32,144,160],[24,48,72],[248,240,216]], // court
 [[24,48,72],[48,72,96],[16,32,48],[232,216,176]], // wall/roof
 [[24,48,72],[224,152,40],[248,192,64],[136,88,40]], // seats
 [[24,48,72],[64,112,120],[248,248,232],[144,168,168]], // goal
 [[20,22,40],[232,236,255],[80,92,136],[254,196,52]], // score/lights
 [[192,128,64],[160,96,48],[224,160,80],[248,224,176]], // wood
];
export const GEOMETRY={goal:{left:40,right:119,top:34,line:88,rearLeft:48,rearRight:111,rearTop:28,rearBottom:80},spot:{x:80,y:118},targets:{x:[52,80,108],y:[82,52]},hudY:136};
export const TEXTURES={
 'court-a':{pal:0,rows:['00000000','00000000','00000000','00000000','00000000','00000000','00000000','00000000']},
 'court-b':{pal:0,rows:['11111111','11111111','11111111','11111111','11111111','11111111','11111111','11111111']},
 'roof-panel':{pal:1,rows:['11111110','11111110','11111110','11111110','11111110','11111110','11111110','00000000']},
 'roof-beam':{pal:1,rows:['33111110','03311110','00331110','00033110','00003310','00000330','00000033','00000003']},
 'wall-panel':{pal:1,rows:['11111111','00000001','00000001','00000001','00000001','00000001','00000001','00000001']},
 'wall-top':{pal:1,rows:['11111111','00000000','00000000','00000000','00000000','00000000','00000000','00000000']},
 'wall-edge':{pal:1,rows:['00000001','00000001','00000001','00000001','00000001','00000001','00000001','00000001']},
 'wall-fill':{pal:1,rows:Array(8).fill('00000000')},
 'seat':{pal:2,rows:['00000000','00222200','02111120','02111120','02111120','03111130','00333300','00033000']},
 'wood-a':{pal:5,rows:['22222221','00000001','00000001','00000001','00000001','00000001','00000001','11111111']},
 'wood-b':{pal:5,rows:['00000001','00000001','11111111','22222221','00000001','00000001','00000001','00000001']},
 'net':{pal:3,rows:['10000000','10000000','10000000','10000000','10000000','10000000','10000000','11111111']},
 'light':{pal:4,rows:['00022000','00022000','02222220','23333332','23111132','23333332','02222220','00000000']},
};
const cv=canvas(20,18),attrs=Array(360).fill(0);
const {rect,plot,hline,vline}=cv;
function tile(name,tx,ty,flip=false){
 const t=TEXTURES[name];attrs[ty*20+tx]=t.pal;
 t.rows.forEach((r,y)=>[...r].forEach((v,x)=>plot(tx*8+x,ty*8+y,+r[flip?7-x:x])));
}
function area(name,x,y,w,h){for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)tile(name,xx,yy);}
function line(x0,y0,x1,y1,c){const n=Math.max(Math.abs(x1-x0),Math.abs(y1-y0));for(let i=0;i<=n;i++)plot(x0+(x1-x0)*i/n,y0+(y1-y0)*i/n,c);}

// Keep the existing two-row kick scoreboard; marks remain separate sprites.
for(let ty=0;ty<2;ty++)for(let tx=0;tx<20;tx++){
 attrs[ty*20+tx]=4;
 const t=previous.tiles[previous.map[ty*20+tx]];
 t.forEach((r,y)=>[...r].forEach((v,x)=>plot(tx*8+x,ty*8+y,+v)));
}

// Roof tiles and repeated mirrored trusses. The quiet score strip replaces the
// concept's tall ceiling so a full penalty area still fits below it.
area('roof-panel',0,2,20,2);
for(const tx of [0,6]){tile('roof-beam',tx,2);tile('roof-beam',tx+1,3);}
for(const tx of [13,19]){tile('roof-beam',tx,2,true);tile('roof-beam',tx-1,3,true);}
hline(0,159,22,3);
for(const tx of [3,9,16])tile('light',tx,3);

// Padded wall panels, two small seat rows and stepped aisle ends.
for(let ty=4;ty<10;ty++)for(let tx=0;tx<20;tx++)tile(ty%2===0?(tx%2?'wall-panel':'wall-top'):(tx%2?'wall-edge':'wall-fill'),tx,ty);
for(const y of [5,7])for(const x of [0,1,2,17,18,19])tile('seat',x,y);
for(const y of [48,64])for(const x of [0,136]){hline(x,x+23,y,1);hline(x,x+23,y+1,2);}
for(const x of [3,16]){
 for(let y=4;y<10;y++){
  const y0=y*8;hline(x*8,x*8+7,y0+6,3);hline(x*8,x*8+7,y0+7,1);
 }
}
for(const x of [4,15]){vline(x*8,32,79,3);vline(x*8+1,32,79,1);}

// Timber apron: two plank tiles used repeatedly under both seating blocks.
for(let tx=0;tx<20;tx++)tile(tx&1?'wood-b':'wood-a',tx,10);
// Large low-contrast court panels reuse two flat tiles; court lines are unique.
for(let ty=11;ty<18;ty++)for(let tx=0;tx<20;tx++)tile(((tx>>1)+(ty>>1))&1?'court-b':'court-a',tx,ty);
hline(0,159,88,3);
line(12,88,0,104,3);line(147,88,159,104,3);
let prev=null;
for(let x=0;x<160;x++){
 const d=Math.max(0,Math.abs(x-80)-28),y=Math.round(118-d*d/100);
 for(let yy=prev===null?y:Math.min(prev,y);yy<=Math.max(prev??y,y);yy++)plot(x,yy,3);
 prev=y;
}
rect(78,117,5,3,3);

// Goal palette occupies whole tiles, with the same navy as the back wall.
// Sparse square mesh is a repeated tile; side/roof edges provide depth.
for(let ty=3;ty<11;ty++)for(let tx=5;tx<=14;tx++){
 attrs[ty*20+tx]=3;rect(tx*8,ty*8,8,8,0);
}
for(let ty=5;ty<10;ty++)for(let tx=6;tx<14;tx++)tile('net',tx,ty);
// A shallow receding roof rather than a second rectangle inside the opening.
hline(48,111,28,3);
line(48,28,41,34,3);line(111,28,118,34,3);
for(let i=1;i<8;i++)line(48+i*8,29,41+i*10,34,1);
hline(46,113,30,1);hline(44,115,32,1);
vline(48,44,80,3);vline(111,44,80,3);hline(48,111,80,3);
for(let y=48;y<=80;y+=8){line(43,y+5,48,y,1);line(111,y,116,y+5,1);}
line(43,86,48,80,3);line(111,80,116,86,3);
// Tinted net floor and foot fittings ground the goal on the existing goal line.
hline(46,113,84,1);hline(44,115,86,1);
rect(40,34,80,4,2);rect(40,34,4,54,2);rect(116,34,4,54,2);
hline(40,119,34,0);hline(43,116,37,3);
vline(40,34,87,0);vline(43,37,86,3);vline(119,34,87,0);vline(116,37,86,3);
rect(40,85,4,3,3);rect(116,85,4,3,3);hline(41,42,85,2);hline(117,118,85,2);
for(const x of [38,114]){rect(x,88,8,2,2);hline(x+1,x+6,90,1);}

// GBC map attributes support flips: a mirrored texture still needs one tile.
const tiles=[],map=[],packedAttrs=[],keys=new Map();
for(let ty=0;ty<18;ty++)for(let tx=0;tx<20;tx++){
 const rows=Array.from({length:8},(_,y)=>cv.px[ty*8+y].slice(tx*8,tx*8+8).join(''));
 const variants=[rows,rows.map(r=>[...r].reverse().join('')),[...rows].reverse(),[...rows].reverse().map(r=>[...r].reverse().join(''))];
 let found=false;
 for(let f=0;f<4;f++){
  const key=variants[f].join('');
  if(keys.has(key)){map.push(keys.get(key));packedAttrs.push(attrs[ty*20+tx]|(f&1?32:0)|(f&2?64:0));found=true;break;}
 }
 if(!found){keys.set(rows.join(''),tiles.length);map.push(tiles.length);tiles.push(rows);packedAttrs.push(attrs[ty*20+tx]);}
}
assert.ok(tiles.length<=176,`Arena needs ${tiles.length} tiles; 176 are available after fonts/frame`);
export const BG={tiles,map,attrs:packedAttrs,w:20,h:18};
export const PIXELS=cv.px;
