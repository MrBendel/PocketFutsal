// Export only the small contact burst needed by futsal. The purchased source
// pack stays ignored; public builds use the committed Game Boy tile data.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
const root=fileURLToPath(new URL('../',import.meta.url));
const pack=process.argv[2]||path.join(root,'artwork','BURST_VFX');
const im=decodePng(fs.readFileSync(path.join(pack,'Hit','Hit_Small_A_GBS.png')));
assert.equal(im.height,16);assert.equal(im.width,64);
const columns=[],keys=new Map(),frames=[];
function bytes(rows){
 return rows.flatMap(row=>{let a=0,b=0;for(let x=0;x<8;x++){a|=(row[x]&1)<<(7-x);b|=(row[x]>>1)<<(7-x);}return[a,b];});
}
for(let frame=0;frame<4;frame++){
 const cells=[];
 for(let col=0;col<2;col++){
  const pixels=Array.from({length:16},(_,y)=>Array.from({length:8},(_,x)=>{
   const p=(y*im.width+frame*16+col*8+x)*4,[r,g,b]=im.rgba.subarray(p,p+3);
   return r===101&&g===255&&b===0?0:r>200?1:r>100?2:3;
  }));
  const variants=[pixels,pixels.map(r=>r.slice().reverse()),pixels.slice().reverse(),pixels.slice().reverse().map(r=>r.slice().reverse())];
  const flags=[0,32,64,96];let found;
  for(let k=0;k<4;k++)if(keys.has(variants[k].flat().join(''))){found=[keys.get(variants[k].flat().join('')),flags[k]];break;}
  if(!found){const index=columns.length;keys.set(pixels.flat().join(''),index);columns.push(bytes(pixels));found=[index,0];}
  cells.push(242+found[0]*2,found[1]);
 }
 frames.push(cells);
}
assert.ok(columns.length*2<=14,'must fit between the side atlas and end of sprite VRAM');
const note='// Generated from BURST_VFX/Hit/Hit_Small_A_GBS.png by tools/export-futsal-fx.mjs.\n';
fs.writeFileSync(path.join(root,'src','futsal_fx_tiles.h'),note+`#ifndef FUTSAL_FX_TILES_H
#define FUTSAL_FX_TILES_H
#include <gb/gb.h>
BANKREF_EXTERN(futsal_fx_tiles)
extern const uint8_t futsal_fx_tiles[];
#define FS_FX_FIRST_TILE 242
#define FS_FX_TILE_COUNT ${columns.length*2}
#define FS_FX_FRAMES 4
#ifdef FUTSAL_FX_TABLES
static const uint8_t fs_fx_cells[4][4]={
${frames.map(f=>'    {'+f.join(',')+'},').join('\n')}
};
#endif
#endif
`);
fs.writeFileSync(path.join(root,'src','futsal_fx_tiles.c'),'#pragma bank 255\n'+note+`#include "futsal_fx_tiles.h"
BANKREF(futsal_fx_tiles)
const uint8_t futsal_fx_tiles[]={
${columns.map(c=>'    '+c.map(v=>'0x'+v.toString(16).padStart(2,'0')).join(',')+',').join('\n')}
};
`);
console.log(`Futsal contact burst: four frames, ${columns.length*2} tiles at 242; hardware flips preserve the original pixels.`);
