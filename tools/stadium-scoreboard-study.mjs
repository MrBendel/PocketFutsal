// Stadium-scale scoreboard concept, native pixels with an extended rear stand.
import fs from 'node:fs';
import {encodePng} from '../harness/node.mjs';
import {FUTSAL_PITCH,projY} from './tile-art.mjs';
import {GOAL_NATIVE,GOAL_H} from './goal-art.mjs';
const font=JSON.parse(fs.readFileSync(new URL('../artwork/fonts/sports-v1/sports.json',import.meta.url)));
const dir=new URL('../artwork/scoreboard/v3/',import.meta.url);fs.mkdirSync(dir,{recursive:true});
const BW=80,BH=32,EXTRA=32,W=160,H=168+EXTRA;
const pal=[[0,0,0],[24,32,48],[104,128,152],[248,224,144]];
const mini={H:['101','101','111','101','101'],O:['111','101','101','101','111'],M:['101','111','111','101','101'],E:['111','100','110','100','111'],A:['010','101','111','101','101'],W:['101','101','111','111','101'],Y:['101','101','010','010','010']};
function board(a,b){const px=Array.from({length:BH},()=>Array(BW).fill(0));
 const rect=(x,y,w,h,v)=>{for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)px[j][i]=v;};
 rect(2,0,76,1,2);rect(1,1,78,29,2);rect(0,3,80,25,1);
 rect(3,2,74,27,1);rect(3,1,74,1,3);rect(3,29,74,2,1);rect(4,31,72,1,1);
 // Recessed display windows and a central seam.
 rect(5,12,31,16,2);rect(44,12,31,16,2);rect(6,13,29,14,1);rect(45,13,29,14,1);
 rect(39,5,2,22,2);
 function tiny(s,x){for(const c of s){mini[c].forEach((r,y)=>[...r].forEach((v,i)=>{if(v==='1')px[y+5][x+i]=2;}));x+=4;}}
 tiny('HOME',12);tiny('AWAY',51);
 function score(n,x){for(const c of String(n).padStart(2,'0')){const g=font.glyphs[c],dx=Math.floor((8-g[0].length)/2);
  for(let y=0;y<16;y++)for(let i=0;i<g[y].length;i++){const v=+g[y][i];if(v===1||v===2)px[y+12][x+dx+i]=3;}x+=8;
 }}
 score(a,13);score(b,52);
 for(const x of [2,77])for(const y of [4,26])px[y][x]=3;
 return px;
}
const cp=[[[48,144,184],[40,132,172],[88,160,64],[248,240,208]],[[88,160,64],[40,88,56],[24,48,56],[208,224,184]],[[88,160,64],[208,152,72],[96,64,40],[248,240,208]],[[136,192,72],[40,104,56],[88,160,64],[248,240,208]]];
function scene(a,b){const rgb=Buffer.alloc(W*H*3);
 const put=(x,y,c)=>{if(x>=0&&x<W&&y>=0&&y<H)rgb.set(c,(y*W+x)*3);};
 const rect=(x,y,w,h,c)=>{for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)put(i,j,c);};
 for(let y=0;y<168;y++)for(let x=0;x<W;x++){
  const cell=Math.floor(y/8)*20+Math.floor(x/8),t=FUTSAL_PITCH.tiles[FUTSAL_PITCH.map[cell]];
  put(x,y+EXTRA,cp[FUTSAL_PITCH.attrs[cell]][+t[y%8][x%8]]);
 }
 rect(0,0,160,32,[40,56,72]);
 // Grandstand steps, alternating seat blocks, and sparse crowd pixels.
 for(let row=0;row<4;row++){
  const y=10+row*7;rect(0,y,160,7,[48+row*4,64+row*4,80+row*4]);
  rect(0,y+6,160,1,[24,40,56]);
  for(let x=3;x<160;x+=7){if(x>69&&x<89)continue;
   rect(x,y+2,4,3,(Math.floor(x/7)+row)%2?[104,128,152]:[144,80,80]);
   if((Math.floor(x/7)+row)%3!==0){put(x+1,y+1,[208,184,136]);put(x+1,y+2,[32,48,64]);}
  }
 }
 // Steel supports rise behind the goal; feet sit on the rear apron.
 for(const x of [48,109]){rect(x,28,3,23,[24,32,48]);rect(x,30,1,19,[104,128,152]);rect(x-2,50,7,2,[40,88,56]);}
 board(a,b).forEach((r,y)=>r.forEach((v,x)=>{if(v)put(40+x,2+y,pal[v]);}));
 // Goal is in front of the stadium backdrop.
 for(const end of [0,1])for(let y=0;y<GOAL_H;y++)for(let x=0;x<40;x++){
  const v=+GOAL_NATIVE[y][x];if(v)put(60+x,EXTRA+projY(end?224:0)-GOAL_H+y,[[0,0,0],[248,248,232],[136,152,168],[24,20,32]][v]);
 }
 return rgb;
}
const pixels=board(2,1);
fs.writeFileSync(new URL('stadium-scoreboard.json',dir),JSON.stringify({name:'Centered stadium scoreboard',width:BW,height:BH,palette:pal,transparentIndex:0,pixels:pixels.map(r=>r.join('')),placement:{x:40,y:2,rearBackdropExtraHeight:EXTRA},digitFields:{home:{x:13,y:12,w:16,h:16},away:{x:52,y:12,w:16,h:16}}},null,2)+'\n');
const rgb=scene(2,1);
fs.writeFileSync(new URL('stadium-court-4x.png',dir),encodePng(rgb,{width:W,height:H,scale:4}));
fs.writeFileSync(new URL('far-stands-5x.png',dir),encodePng(rgb.subarray(0,W*96*3),{width:W,height:96,scale:5}));
fs.writeFileSync(new URL('after-goal-4x.png',dir),encodePng(scene(3,1),{width:W,height:H,scale:4}));
const close=Buffer.alloc(BW*BH*3);pixels.forEach((r,y)=>r.forEach((v,x)=>close.set(pal[v]??pal[1],(y*BW+x)*3)));
fs.writeFileSync(new URL('scoreboard-6x.png',dir),encodePng(close,{width:BW,height:BH,scale:6}));
console.log('80x32 centered stadium scoreboard; stand backdrop adds 32 native pixels above the existing court.');
