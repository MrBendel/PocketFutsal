// Restrained reactions: translate original pixels, preserving faces and outfits.
import fs from 'node:fs';
import {CHARACTERS,upright} from './player-art.mjs';
export const REACTION_FRAMES=3;
export function reactionFrames(id){
 const src=JSON.parse(fs.readFileSync(new URL(`../artwork/players/roster/${id}/player.json`,import.meta.url))).frames[0].rows;
 const copy=()=>src.map(r=>[...r]);
 function shoulders(dy){const p=copy();
  for(let y=0;y<(dy<0?18:19);y++)p[y]=Array(16).fill('.');
  for(let y=0;y<19;y++)if(y+dy>=0)p[y+dy]=[...src[y]];
  return p;
 }
 function arm(p,side){const old=p.map(r=>[...r]),lo=side?11:0,hi=side?16:4;
  for(let y=15;y<18;y++)for(let x=lo;x<hi;x++)p[y][x]='.';
  for(let y=15;y<18;y++)for(let x=lo;x<hi;x++)p[y-1][x]=old[y][x];
 }
 const subtle=copy();
 if(id==='spiky'||id==='crop')arm(subtle,id==='spiky'?1:0);
 else if(id==='cap'){arm(subtle,0);arm(subtle,1);}
 else{const raised=shoulders(-1);arm(raised,1);for(let y=0;y<24;y++)subtle[y]=raised[y];}
 const slump=shoulders(1),nod=shoulders(1);
 // A one-pixel head dip relative to the slumped shoulders; retain the exact
 // original hair, eyes, hat and bandana instead of drawing a new expression.
 for(let y=0;y<14;y++)nod[y]=Array(16).fill('.');
 for(let y=0;y<13;y++)for(let x=0;x<16;x++)if(src[y][x]!=='.')nod[y+2][x]=src[y][x];
 const rows=p=>p.map(r=>r.join(''));
 return {win:[copy(),shoulders(-1),subtle].map(rows),lose:[copy(),slump,nod].map(rows)};
}
export const REACTION_TILES=CHARACTERS.flatMap(id=>{const r=reactionFrames(id);return [...r.win,...r.lose].flatMap(upright);});
