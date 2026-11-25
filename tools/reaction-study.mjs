import fs from 'node:fs';
import {reactionFrames,WIN_CYCLE,WIN_LIFT,LOSE_CYCLE} from './reaction-art.mjs';
import {CHARACTERS} from './player-art.mjs';
import {encodePng} from '../harness/node.mjs';
const dir=new URL('../artwork/players/roster/reactions/',import.meta.url);fs.mkdirSync(dir,{recursive:true});
const W=152,H=120,rgb=Buffer.alloc(W*H*3);
for(let p=0;p<rgb.length;p+=3)rgb.set([40,56,72],p);
const data={frameWidth:16,frameHeight:24,framesPerOutcome:3,characters:{}};
CHARACTERS.forEach((id,row)=>{const poses=reactionFrames(id);data.characters[id]=poses;
 ['win','lose'].forEach((outcome,side)=>poses[outcome].forEach((pose,frame)=>{
  const ox=4+side*80+frame*22,oy=3+row*30;
  const pal=[[0,0,0],[248,200,136],side?[232,64,72]:[40,120,232],[24,32,48]];
  if(id==='crop')pal[1]=[184,120,72];
  pose.forEach((r,y)=>[...r].forEach((c,x)=>{if(c!=='.')rgb.set(pal[+c],((oy+y)*W+ox+x)*3);}));
 }));
});
fs.writeFileSync(new URL('reactions.json',dir),JSON.stringify(data,null,2)+'\n');
fs.writeFileSync(new URL('win-lose-sheet-4x.png',dir),encodePng(rgb,{width:W,height:H,scale:4}));
console.log('Four character rows; three win frames left, three lose frames right.');

// Small indexed GIF preview, kept lossless so one-pixel changes remain visible.
const colors=[[40,56,72],[248,200,136],[184,120,72],[40,120,232],[232,64,72],[24,32,48],[248,232,64],[248,248,232]];
const word=n=>[n&255,n>>8],gw=384,gh=576;
const blocks=[Buffer.from('GIF89a'),Buffer.from([...word(gw),...word(gh),0xf2,0,0,...colors.flat()]),Buffer.from([0x21,0xff,11,...Buffer.from('NETSCAPE2.0'),3,1,0,0,0])];
for(let phase=0;phase<8;phase++){
 const delay=13,native=new Uint8Array(96*144);
 CHARACTERS.forEach((id,row)=>['win','lose'].forEach((outcome,side)=>{
  const step=side?LOSE_CYCLE[phase]:WIN_CYCLE[phase];
  const pose=data.characters[id][outcome][step],ox=side?60:20,ground=8+row*34,oy=ground-(side?0:WIN_LIFT[phase]);
  for(let x=4;x<12;x++)native[(ground+23)*96+ox+x]=5;
  pose.forEach((r,y)=>[...r].forEach((c,x)=>{if(c!=='.')native[(oy+y)*96+ox+x]=c==='1'?(id==='crop'?2:1):c==='2'?(side?4:3):5;}));
 }));
 const pixels=Array.from({length:gw*gh},(_,p)=>native[Math.floor(Math.floor(p/gw)/4)*96+Math.floor((p%gw)/4)]);
 const codes=[];for(let p=0;p<pixels.length;p+=6)codes.push(8,...pixels.slice(p,p+6));codes.push(9);
 const packed=Buffer.alloc(Math.ceil(codes.length/2));codes.forEach((c,i)=>packed[i>>1]|=c<<((i&1)*4));
 blocks.push(Buffer.from([0x21,0xf9,4,4,...word(delay),0,0,0x2c,0,0,0,0,...word(gw),...word(gh),0,3]));
 for(let p=0;p<packed.length;p+=255){const chunk=packed.subarray(p,p+255);blocks.push(Buffer.from([chunk.length]),chunk);}blocks.push(Buffer.from([0]));
}
blocks.push(Buffer.from([0x3b]));fs.writeFileSync(new URL('subtle-win-lose.gif',dir),Buffer.concat(blocks));
