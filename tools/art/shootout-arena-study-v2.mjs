// Restore the approved comp's broad mouth, deep net and rounded portable frame.
import assert from 'node:assert/strict';
import {canvas} from './common.mjs';
import {VENUE_PIXELS,VENUE_ATTRS,PALETTES as originalPalettes,TEXTURES} from './shootout-arena-study.mjs';
export {TEXTURES};
export const PALETTES=originalPalettes.map(p=>p.map(c=>[...c]));
PALETTES[3]=[[24,48,72],[72,104,128],[248,248,232],[152,176,184]];
export const GEOMETRY={goal:{left:28,right:131,top:34,line:80,rearLeft:40,rearRight:119,rearTop:26,rearBottom:70},spot:{x:80,y:118},targets:{x:[52,80,108],y:[74,50]},hudY:136,keeperFeet:86,requiresShotPlaneUpdate:true};
const cv=canvas(20,18),attrs=[...VENUE_ATTRS];
VENUE_PIXELS.forEach((r,y)=>cv.px[y]=[...r]);
const {plot,rect,hline,vline}=cv;
function line(x0,y0,x1,y1,c){const n=Math.max(Math.abs(x1-x0),Math.abs(y1-y0));for(let i=0;i<=n;i++)plot(x0+(x1-x0)*i/n,y0+(y1-y0)*i/n,c);}
function texture(name,tx,ty){const t=TEXTURES[name];attrs[ty*20+tx]=t.pal;t.rows.forEach((r,y)=>[...r].forEach((v,x)=>plot(tx*8+x,ty*8+y,+v)));}
// Move the goal plane back eight pixels, giving the freestanding net room and
// restoring the comp's floor space. The keeper stands just in front of it.
for(let tx=0;tx<20;tx++)texture(tx&1?'wood-b':'wood-a',tx,9);
for(let ty=10;ty<18;ty++)for(let tx=0;tx<20;tx++)texture(((tx>>1)+(ty>>1))&1?'court-b':'court-a',tx,ty);
hline(0,159,80,3);line(12,80,0,91,3);line(147,80,159,92,3);
let prev=null;
for(let x=0;x<160;x++){
 const d=Math.max(0,Math.abs(x-80)-28),y=Math.round(118-d*d/100);
 for(let yy=prev===null?y:Math.min(prev,y);yy<=Math.max(prev??y,y);yy++)plot(x,yy,3);prev=y;
}
rect(78,117,5,3,3);
for(let ty=3;ty<10;ty++)for(let tx=3;tx<=16;tx++){
 if(ty===3&&(tx===3||tx===16))continue; // keep the two arena lights
 attrs[ty*20+tx]=3;rect(tx*8,ty*8,8,8,0);
}
// Rear plane: a regular, fine net. It stops at the rear floor frame instead of
// running to the front posts, which used to flatten the whole mouth into a wall.
for(let y=38;y<70;y++)for(let x=40;x<=119;x++)if(x%8===0||y%8===2)plot(x,y,1);
// Roof trapezoid: receding upper edges and projected strands, not a thin cap.
hline(40,119,26,3);line(40,26,31,34,3);line(119,26,128,34,3);
for(let i=1;i<10;i++)line(40+i*8,27,31+i*9.7,34,1);
for(const y of [28,31]){const f=(y-26)/8;hline(Math.round(40-9*f),Math.round(119+9*f),y,1);}
// Side faces have clearly diagonal horizontal strands. Rear uprights and base
// are subdued grey, so only the front opening reads as a bright frame.
vline(40,38,70,3);vline(119,38,70,3);hline(40,119,70,3);
for(let y=34;y<=66;y+=8){line(40,y,31,y+8,1);line(119,y,128,y+8,1);}
for(const f of [1/3,2/3]){
 line(40-9*f,26+8*f,40-9*f,70+9*f,1);
 line(119+9*f,26+8*f,119+9*f,70+9*f,1);
}
// Open court beneath the net, rather than mesh closing off the floor like a cage.
for(let y=71;y<80;y++){
 const f=(y-70)/9;hline(Math.round(40-9*f),Math.round(119+9*f),y,1);
}
for(let tx=5;tx<15;tx++){attrs[9*20+tx]=0;rect(tx*8,72,8,8,0);}
hline(40,119,72,1);
line(40,70,31,79,3);line(119,70,128,79,3);
// Rounded tubular front frame: chamfered two-pixel corners, cream highlights,
// inner grey shade and separate small elliptical foot plates.
hline(31,128,34,3);hline(31,128,35,2);hline(31,128,36,2);hline(32,127,37,3);
for(const [x,mirror] of [[28,false],[128,true]]){
 const a=mirror?x+2:x+1,b=mirror?x+1:x+2,shade=mirror?x:x+3;
 vline(a,38,77,2);vline(b,38,77,2);vline(shade,39,77,3);
}
plot(30,35,3);plot(29,36,2);plot(30,36,2);plot(28,37,3);plot(29,37,2);plot(30,37,2);
plot(129,35,3);plot(129,36,2);plot(130,36,2);plot(131,37,3);plot(129,37,2);plot(130,37,2);
for(const cx of [30,130]){
 hline(cx-2,cx+2,77,3);hline(cx-3,cx+3,78,3);hline(cx-2,cx+2,79,3);
 hline(cx-1,cx+1,77,2);hline(cx-1,cx+1,78,2);
}
// Small floor shadows use the court's palette beyond the goal's tile region.
for(const cx of [30,130]){hline(cx-4,cx+4,80,2);hline(cx-2,cx+3,81,1);}

const tiles=[],map=[],packedAttrs=[],keys=new Map();
for(let ty=0;ty<18;ty++)for(let tx=0;tx<20;tx++){
 const rows=Array.from({length:8},(_,y)=>cv.px[ty*8+y].slice(tx*8,tx*8+8).join(''));
 const variants=[rows,rows.map(r=>[...r].reverse().join('')),[...rows].reverse(),[...rows].reverse().map(r=>[...r].reverse().join(''))];
 let found=false;
 for(let f=0;f<4;f++)if(keys.has(variants[f].join(''))){map.push(keys.get(variants[f].join('')));packedAttrs.push(attrs[ty*20+tx]|(f&1?32:0)|(f&2?64:0));found=true;break;}
 if(!found){keys.set(rows.join(''),tiles.length);map.push(tiles.length);tiles.push(rows);packedAttrs.push(attrs[ty*20+tx]);}
}
assert.ok(tiles.length<=176,`Arena needs ${tiles.length} unique tiles`);
export const BG={tiles,map,attrs:packedAttrs,w:20,h:18};
export const PIXELS=cv.px;
