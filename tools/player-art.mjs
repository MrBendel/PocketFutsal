// Native pixel-row adaptation of the approved generated player studies.
// Keeps the original engine's ten poses per view plus its 24x16 slide.
import fs from 'node:fs';
export const CHARACTERS=['spiky','cap','crop','bandana'];
const read=id=>JSON.parse(fs.readFileSync(new URL(`../artwork/players/roster/${id}/player.json`,import.meta.url)));
const blank=()=>Array.from({length:24},()=>Array(16).fill('.'));
const clone=p=>p.map(r=>[...r]);
const shifted=(p,dy)=>{const out=blank();for(let y=0;y<24;y++) if(y+dy>=0&&y+dy<24)out[y+dy]=[...p[y]];return out;};
function arms(p,raised=false) {
 const out=clone(p),top=raised?7:12;
 for(const x of [0,13]) for(let y=top;y<top+3;y++) for(let dx=0;dx<3;dx++) out[y][x+dx]=(y===top||dx===0||dx===2)?'3':'1';
 return out;
}
export function character(id) {
 const data=read(id),src=data.frames.map(f=>f.rows.map(r=>[...r]));
 const views=[0,4].map(base=>{
  const [idle,runA,runB,kick]=src.slice(base,base+4);
  // Keep the original connected arms. The old synthetic hand boxes sat next
  // to the head after shifting the body, which read as floating fists.
  // Lower only the upper body one pixel into the planted legs for a ready crouch.
  const ready=clone(idle);
  for(let y=0;y<20;y++) ready[y]=y?[...idle[y-1]]:Array(16).fill('.');
  const volley=clone(kick);
  // Lift the extended foot while keeping the planted leg and head readable.
  for(let y=16;y<23;y++) for(let x=11;x<16;x++) {if(kick[y][x]!=='.'){volley[y][x]='.';volley[y-3][x]=kick[y][x];}}
  const header=arms(shifted(idle,-3),true);
  const bicycle=kick.map(r=>[...r].reverse()).reverse();
  const win=arms(idle,true),lose=shifted(idle,2);
  return [idle,runA,runB,ready,kick,volley,header,bicycle,win,lose].map(p=>p.map(r=>r.join('')));
 });
 // A tackle is a seated, feet-first lunge, not a rotated standing body.
 const down=Array.from({length:16},(_,y)=>Array.from({length:24},(_,x)=>src[0][23-x][y]).join(''));
 const tackle=Array.from({length:16},()=>Array(24).fill('.'));
 const dot=(x,y,c)=>{if(x>=0&&x<24&&y>=0&&y<16)tackle[y][x]=String(c);};
 const rect=(x,y,w,h,c)=>{for(let dy=0;dy<h;dy++)for(let dx=0;dx<w;dx++)dot(x+dx,y+dy,c);};
 const poly=(points,c)=>{for(let y=0;y<16;y++)for(let x=0;x<24;x++){
  let inside=false;for(let i=0,j=points.length-1;i<points.length;j=i++){
   const [xi,yi]=points[i],[xj,yj]=points[j];
   if((yi>y+.5)!=(yj>y+.5)&&(x+.5)<(xj-xi)*(y+.5-yi)/(yj-yi)+xi)inside=!inside;
  }if(inside)dot(x,y,c);
 }};
 // Bent rear knee, with an angular silhouette above the extended leg.
 poly([[12,11],[9,7],[6,7],[5,9],[8,13],[13,14]],3);
 poly([[8,8],[9,9],[11,12],[9,12],[7,9]],1);
 rect(5,8,2,2,2);
 // Long leading leg, team sock and a dark boot at the far end.
 poly([[12,10],[15,12],[11,14],[3,15],[0,15],[0,12],[8,11]],3);
 poly([[8,11],[12,11],[11,13],[6,14],[5,12]],1);
 rect(3,12,4,2,2);rect(0,13,3,2,3);
 // Torso leans back toward the planted hand, while the head stays upright.
 poly([[14,6],[19,8],[18,11],[14,13],[10,11],[11,8]],3);
 poly([[14,7],[17,8],[17,10],[14,11],[12,10],[12,8]],2);
 poly([[18,8],[20,8],[22,12],[24,13],[24,15],[20,14]],3);
 poly([[19,9],[20,9],[21,12],[23,13],[23,14],[21,13]],1);
 rect(15,6,3,2,1);
 // Reuse the actual three-quarter profile from the kicking frame at 1:1.
 // Mirror it toward the leading foot; preserve every hair/hat/bandana pixel.
 const profile=src[3];
 let hx0=16,hx1=0,hy0=13;
 for(let y=0;y<=12;y++)for(let x=0;x<16;x++)if(profile[y][x]!=='.'){
  hx0=Math.min(hx0,x);hx1=Math.max(hx1,x);hy0=Math.min(hy0,y);
 }
 const headWidth=hx1-hx0+1,headHeight=13-hy0,headLeft=24-headWidth,headTop=11-headHeight;
 // Join the jaw to a broad shoulder before overlaying the profile, rather than
 // hanging a front-facing portrait above a separate diagonal torso.
 for(let y=0;y<9;y++)for(let x=12;x<24;x++)tackle[y][x]='.';
 rect(12,11,8,3,3);rect(13,11,6,2,2);
 const jaw=[];for(let x=hx0;x<=hx1;x++)if(profile[12][x]==='1')jaw.push(headLeft+hx1-x);
 jaw.sort((a,b)=>a-b);
 const neck=jaw.length?jaw[Math.floor(jaw.length/2)]:16;
 rect(neck-1,10,2,2,1);
 for(let y=0;y<headHeight;y++)for(let x=0;x<headWidth;x++){
  const color=profile[hy0+y][hx0+x];
  if(color!=='.')dot(headLeft+headWidth-1-x,headTop+y,color);
 }
 const slide=tackle.map(r=>r.join(''));

 // A slight three-quarter turn, using the approved front-running gait.
 // Keep the broad shoulders, two-leg idle stance and compact bent strides.
 // The original kicking head already shows both eyes with a slight turn.
 const gait=read('spiky').frames.slice(0,3).map(f=>f.rows);
 const turnedTorso=[
  '....322111233...',
  '...33222222233..',
  '..3132222223113.',
 ];
 const side=gait.map(p=>[
  ...src[3].slice(0,13).map(r=>r.join('')),...turnedTorso,...p.slice(16),
 ]);

 const transitions=views.map(view=>{
  const ready=view[3].map(r=>[...r]),kick=view[4];
  // A compact sideways tuck bridges upright and inverted silhouettes.
  const tuck=blank();
  for(let y=0;y<11;y++) for(let x=0;x<16;x++) tuck[y+6][x]=kick[23-Math.min(23,Math.floor(x*24/16))][Math.min(15,Math.floor(y*16/11))];
  const land=clone(ready),rise=clone(ready);
  for(let y=0;y<17;y++) {
   land[y]=y?[...ready[y-1]]:Array(16).fill('.');
   rise[y]=Array(16).fill('.');
   for(let x=0;x<14;x++) rise[y][x+2]=ready[y][x];
  }
  return [tuck,land,rise].map(p=>p.map(r=>r.join('')));
 });
 return {id,palette:data.palette,views,slide,down,side,transitions,basic:data.frames.map(f=>f.rows)};
}
const empty=Array(8).fill('........');
const cut=(p,x,y)=>p.slice(y,y+8).map(r=>r.slice(x,x+8));
export function upright(p) {return [cut(p,0,0),cut(p,0,8),cut(p,0,16),empty,cut(p,8,0),cut(p,8,8),cut(p,8,16),empty];}
export function fullTiles(id) {const c=character(id);return [...c.views[1].flatMap(upright),...c.views[0].flatMap(upright),...Array.from({length:3},(_,col)=>[cut(c.slide,col*8,0),cut(c.slide,col*8,8)]).flat(),empty,empty,...[0,1,2].flatMap(pose=>[...upright(c.transitions[1][pose]),...upright(c.transitions[0][pose])]),...Array.from({length:3},(_,col)=>[cut(c.down,col*8,0),cut(c.down,col*8,8)]).flat(),empty,empty,...c.side.flatMap(upright)];}
export function basicTiles(id) {return character(id).basic.flatMap(upright);}
export function sideTiles(id) {return character(id).side.flatMap(upright);}
// The top 16px are unchanged across the cycle. The bottom 8px are shared by
// every character (their palette supplies skin/team colors). 28 tiles total.
export function sideAtlas() {
 const roster=CHARACTERS.map(character);
 return [...roster.flatMap(c=>[
  cut(c.side[0],0,0),cut(c.side[0],0,8),cut(c.side[0],8,0),cut(c.side[0],8,8),
 ]),...roster[0].side.flatMap(p=>[cut(p,0,16),empty,cut(p,8,16),empty])];
}

export function actionTiles(id) {const c=character(id);return [...Array.from({length:3},(_,col)=>[cut(c.slide,col*8,0),cut(c.slide,col*8,8)]).flat(),empty,empty,...upright(c.views[0][3]),...upright(c.views[1][3])];}
