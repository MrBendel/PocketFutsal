// Export the native portable-goal artwork used by both ends of the court.
import fs from 'node:fs';
import {FUTSAL_GOAL,asciiTile} from './tile-art.mjs';
import {encodePng} from '../harness/node.mjs';
const dir=new URL('../artwork/goals/v1/',import.meta.url);
fs.mkdirSync(dir,{recursive:true});
const colors=[[48,144,184],[248,248,232],[136,152,168],[24,20,32]];
const rgb=new Uint8Array(40*32*3);
for(let y=0;y<32;y++)for(let x=0;x<40;x++){
 const col=x>>3,xx=col===4?7-(x&7):x&7,yy=y&15;
 const tile=(col===0||col===4?0:2)+(yy>>3);
 const c=Number(FUTSAL_GOAL[tile][yy&7][xx])||0;
 rgb.set(colors[c],(y*40+x)*3);
}
for(const scale of [1,4])fs.writeFileSync(new URL(`goals-${scale}x.png`,dir),encodePng(rgb,{width:40,height:32,scale}));
fs.writeFileSync(new URL('goal.2bpp',dir),Buffer.from(FUTSAL_GOAL.flatMap(t=>asciiTile(t))));
console.log('Exported matching top/bottom 40x16 goals and native tiles.');
