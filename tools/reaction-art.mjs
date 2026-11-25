// Restrained reactions: translate original pixels, preserving faces and outfits.
import fs from 'node:fs';
import {CHARACTERS,upright} from './player-art.mjs';
export const REACTION_FRAMES=3;
export const WIN_CYCLE=[0,1,2,2,1,0,0,0];
export const WIN_LIFT=[0,2,5,6,3,0,0,0];
export const LOSE_CYCLE=[0,0,1,1,2,2,1,2];
export const reactionHeadOffsets=id=>({win:[[0,0],[0,0],[0,0]],lose:[[0,1],[id==='bandana'?0:-1,2],[1,2]]});
export function reactionFrames(id){
 const src=JSON.parse(fs.readFileSync(new URL(`../artwork/players/roster/${id}/player.json`,import.meta.url))).frames[0].rows;
 const copy=()=>src.map(r=>[...r]);
 function shoulders(dy){const p=copy();
  for(let y=0;y<19;y++)p[y]=Array(16).fill('.');
  for(let y=0;y<19;y++)if(y+dy>=0)p[y+dy]=[...src[y]];
  return p;
 }
 // Lift the original right hand to shoulder height. Join it to the shoulder,
 // then restore the original head on top so the fist never edits the face.
 const raised=copy();
 for(let y=15;y<18;y++)for(let x=11;x<16;x++)raised[y][x]='.';
 for(let y=15;y<18;y++)for(let x=11;x<16;x++)raised[y-3][x]=src[y][x];
 for(let y=14;y<17;y++){raised[y][12]='3';raised[y][13]='1';raised[y][14]='3';}
 for(let y=0;y<13;y++)for(let x=0;x<16;x++)if(src[y][x]!=='.')raised[y][x]=src[y][x];
 const airborne=raised.map(r=>[...r]);
 // Extend the existing feet one pixel into a pointed-toe airborne pose.
 airborne[23]=[...src[22]];airborne[22]=[...src[21]];
 const offsets=reactionHeadOffsets(id).lose;
 const loss=offsets.map(([dx,dy])=>{
  const p=shoulders(dy);
  for(let y=0;y<13+dy;y++)p[y]=Array(16).fill('.');
  for(let y=0;y<13;y++)for(let x=0;x<16;x++)if(src[y][x]!=='.')p[y+dy][x+dx]=src[y][x];
  return p;
 });
 const rows=p=>p.map(r=>r.join(''));
 return {win:[copy(),raised,airborne].map(rows),lose:loss.map(rows)};
}
export const REACTION_TILES=CHARACTERS.flatMap(id=>{const r=reactionFrames(id);return [...r.win,...r.lose].flatMap(upright);});
