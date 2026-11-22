import fs from 'node:fs';
import {FUTSAL_PITCH as court} from './tile-art.mjs';
import {encodePng} from '../harness/node.mjs';
const palettes=[
 [[48,144,184],[40,132,172],[88,160,64],[248,240,208]],
 [[88,160,64],[40,88,56],[24,48,56],[208,224,184]],
 [[88,160,64],[208,152,72],[96,64,40],[248,240,208]],
 [[136,192,72],[40,104,56],[88,160,64],[248,240,208]],
];
const rgb=Buffer.alloc(160*168*3);
for(let y=0;y<168;y++)for(let x=0;x<160;x++){
 const cell=Math.floor(y/8)*20+Math.floor(x/8),tile=court.tiles[court.map[cell]],index=Number(tile[y%8][x%8]);
 rgb.set(palettes[court.attrs[cell]][index],(y*160+x)*3);
}
const dir=new URL('../artwork/courts/',import.meta.url);fs.mkdirSync(dir,{recursive:true});
for(const scale of [1,4])fs.writeFileSync(new URL(`club-court-${scale}x.png`,dir),encodePng(rgb,{width:160,height:168,scale}));
console.log('Club Court: 160x168, four background palettes, '+court.tiles.length+' unique 8x8 tiles.');
