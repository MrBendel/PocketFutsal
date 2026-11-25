// Background-only stadium extension and fixed VRAM score fields.
import fs from 'node:fs';
import {FUTSAL_PITCH} from './tile-art.mjs';
export const STADIUM_OFFSET=32;
export const STADIUM_PALETTE=[[88,160,64],[24,32,48],[104,128,152],[248,224,144]];
export const COURT_PALETTES=[
 [[48,144,184],[40,132,172],[88,160,64],[248,240,208]],
 [[88,160,64],[40,88,56],[24,48,56],[208,224,184]],
 [[88,160,64],[208,152,72],[96,64,40],[248,240,208]],
 [[136,192,72],[40,104,56],[88,160,64],[248,240,208]],
 [],[],[],STADIUM_PALETTE,
];
const font=JSON.parse(fs.readFileSync(new URL('../artwork/fonts/sports-v1/sports.json',import.meta.url)));
const source=JSON.parse(fs.readFileSync(new URL('../artwork/scoreboard/v3/stadium-scoreboard.json',import.meta.url)));
const board=source.pixels.map(r=>[...r].map(Number));
function rect(px,x,y,w,h,v){for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)px[j][i]=v;}
// Keep the housing fixed; center the visible number faces inside each recess.
for(const x of [5,44]){rect(board,x,12,31,18,2);rect(board,x+1,13,29,16,1);}
function withScore(blue,red){const px=board.map(r=>[...r]);
 for(const [n,x0]of [[blue,12],[red,52]]){let pen=x0;
  // Right-align a narrow 1 in its fixed cell so the visible two-digit block
  // keeps the same outer bounds as 00, rather than leaning toward the zero.
  for(const ch of String(n).padStart(2,'0')){const g=font.glyphs[ch],dx=8-g[0].length;
   for(let y=0;y<16;y++)for(let x=0;x<g[y].length;x++)if(g[y][x]==='1'||g[y][x]==='2')px[14+y][pen+dx+x]=3;
   pen+=8;
  }
 }return px;
}
const nearest=(rgb,pal)=>{let best=0,d=Infinity;pal.forEach((p,i)=>{const n=p.reduce((s,c,j)=>s+(c-rgb[j])**2,0);if(n<d){best=i;d=n;}});return best;};
function makePitch(){
 const rgb=Array.from({length:200},()=>Array.from({length:160},()=>STADIUM_PALETTE[1]));
 const attrs=Array(500).fill(7);
 for(let y=0;y<168;y++)for(let x=0;x<160;x++){
  const cell=(y>>3)*20+(x>>3),pal=FUTSAL_PITCH.attrs[cell];
  rgb[y+32][x]=COURT_PALETTES[pal][+FUTSAL_PITCH.tiles[FUTSAL_PITCH.map[cell]][y%8][x%8]];
  attrs[((y+32)>>3)*20+(x>>3)]=pal;
 }
 function paint(x,y,c){rgb[y][x]=c;attrs[(y>>3)*20+(x>>3)]=7;}
 function box(x,y,w,h,c){for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)paint(i,j,c);}
 // Repeatable seating blocks keep the stand's tile cost small.
 for(let row=0;row<4;row++)for(let x=0;x<160;x++){
  const y=8+row*8;for(let j=0;j<8;j++)paint(x,y+j,STADIUM_PALETTE[1]);
  if(x%8>=1&&x%8<=4){paint(x,y+3,STADIUM_PALETTE[2]);paint(x,y+4,STADIUM_PALETTE[2]);}
  if(x%16===2)paint(x,y+2,STADIUM_PALETTE[3]);
  paint(x,y+7,STADIUM_PALETTE[2]);
 }
 for(const x of [48,109]){box(x,28,3,23,STADIUM_PALETTE[1]);box(x,30,1,19,STADIUM_PALETTE[2]);box(x-2,50,7,2,[40,88,56]);}
 withScore(0,0).forEach((r,y)=>r.forEach((v,x)=>{if(v)paint(40+x,y,STADIUM_PALETTE[v]);}));
 const tiles=[],map=[],keys=new Map();
 for(let ty=0;ty<25;ty++)for(let tx=0;tx<20;tx++){
  const pal=attrs[ty*20+tx],rows=Array.from({length:8},(_,y)=>Array.from({length:8},(_,x)=>nearest(rgb[ty*8+y][tx*8+x],COURT_PALETTES[pal])).join(''));
  const key=rows.join('');if(!keys.has(key)){keys.set(key,tiles.length);tiles.push(rows);}map.push(keys.get(key));
 }
 // Units change in this first-to-five mode; tens stay zero. Give each units
 // region six distinct tiles, including the adjacent static art in those cells.
 const scoreBase=tiles.length;
 for(const x0 of [7,12])for(let y=1;y<4;y++)for(let x=x0;x<x0+2;x++){tiles.push(tiles[map[y*20+x]]);map[y*20+x]=tiles.length-1;}
 if(tiles.length>166)throw new Error(`Stadium needs ${tiles.length} tiles; only 166 safe background tiles`);
 return {tiles:[...tiles.slice(scoreBase),...tiles.slice(0,scoreBase)],map:map.map(n=>n>=scoreBase?n-scoreBase:n+12),attrs,w:20,h:25,scoreBase:0};
}
export const STADIUM_PITCH=makePitch();
export const pitchTile=n=>n<134?80+n:96+n-134;
export const STADIUM_MAP={...STADIUM_PITCH,map:STADIUM_PITCH.map.map(pitchTile),attrs:STADIUM_PITCH.attrs.map((p,i)=>p|(STADIUM_PITCH.map[i]>=134?8:0))};
export const SCORE_TILES=Array.from({length:6},(_,n)=>{
 const px=withScore(n,n),tiles=[];
 for(const x0 of [16,56])for(let y=8;y<32;y+=8)for(let x=x0;x<x0+16;x+=8)tiles.push(px.slice(y,y+8).map(r=>r.slice(x,x+8).join('')));
 return tiles;
}).flat();
