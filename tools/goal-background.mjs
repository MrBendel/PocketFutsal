// Goals are composited into bank-one BG tiles, so OBJ scanline limits cannot
// remove the frame. Precompute small displacement poses; runtime only uploads.
import {FUTSAL_PITCH,FUTSAL_GOAL,projY} from './tile-art.mjs';
export const PATCH_W=8,PATCH_H=3,PATCH_X=6,POSES=16;
export const PATCH_Y=[1,16];
export const GOAL_PALETTES=[
 [[40,132,172],[24,20,32],[88,160,64],[248,248,232]],
 [[48,144,184],[24,20,32],[88,160,64],[248,248,232]],
 [[88,160,64],[24,20,32],[40,88,56],[248,248,232]],
];
const courtPalettes=[
 [[48,144,184],[40,132,172],[88,160,64],[248,240,208]],
 [[88,160,64],[40,88,56],[24,48,56],[208,224,184]],
 [[88,160,64],[208,152,72],[96,64,40],[248,240,208]],
 [[136,192,72],[40,104,56],[88,160,64],[248,240,208]],
];
export function goalPose(dx,dy){const a=Math.abs(dy);return a?1+(a-1)*3+(dx<0?0:dx>0?2:1):0;}
function nearest(rgb,pal){let best=0,score=Infinity;pal.forEach((c,i)=>{const d=c.reduce((s,v,j)=>s+(v-rgb[j])**2,0);if(d<score){score=d;best=i;}});return best;}
function goalPixel(x,y){if(x<0||x>=40||y<0||y>=16)return 0;const col=x>>3,xx=col===4?7-(x&7):x&7;return Number(FUTSAL_GOAL[(col===0||col===4?0:2)+(y>>3)][y&7][xx])||0;}
export function buildGoalBackground(){
 const tiles=[],map=[],attrs=[];
 for(let end=0;end<2;end++)for(let pose=0;pose<POSES;pose++){
  const amount=pose?Math.floor((pose-1)/3)+1:0,dir=pose?(pose-1)%3-1:0;
  const gx=60+dir*amount,gy=projY(end?224:0)-16+(end?amount:-amount);
  if(gx<PATCH_X*8||gx+40>(PATCH_X+PATCH_W)*8||gy<PATCH_Y[end]*8||gy+16>(PATCH_Y[end]+PATCH_H)*8)throw new Error('Goal knockback leaves its BG patch');
  for(let local=0;local<PATCH_W*PATCH_H;local++){
   const tx=PATCH_X+local%PATCH_W,ty=PATCH_Y[end]+Math.floor(local/PATCH_W),index=ty*20+tx;
   const original=FUTSAL_PITCH.tiles[FUTSAL_PITCH.map[index]],oldPal=FUTSAL_PITCH.attrs[index];
   const ink=Array.from({length:8},(_,y)=>Array.from({length:8},(_,x)=>goalPixel(tx*8+x-gx,ty*8+y-gy)));
   const covered=ink.some(r=>r.some(Boolean));
   const count=[0,0];for(const r of original)for(const c of r)if(c==='0'||c==='1')count[Number(c)]++;
   const pal=oldPal===0?(count[1]>count[0]?4:5):6;
   const rows=original.map((r,y)=>[...r].map((c,x)=>ink[y][x]?(ink[y][x]===1?'3':'1'):String(nearest(courtPalettes[oldPal][Number(c)],GOAL_PALETTES[pal-4]))).join(''));
   tiles.push(rows);
   map.push(covered?end*PATCH_W*PATCH_H+local:80+FUTSAL_PITCH.map[index]);
   attrs.push(covered?8|pal:oldPal);
  }
 }
 return {tiles,map,attrs,w:PATCH_W,h:PATCH_H*POSES*2};
}
export const GOAL_BG=buildGoalBackground();
