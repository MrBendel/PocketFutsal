import fs from 'node:fs';
import assert from 'node:assert/strict';
import {BG,PIXELS,PALETTES,GEOMETRY,TEXTURES} from './art/shootout-arena-study.mjs';
import {writePng} from './study-raster.mjs';
import {decodePng} from './assets.mjs';
import {character} from './player-art.mjs';
import {BALL_FRAMES} from './ball-art.mjs';
const dir=new URL('../artwork/shootout/background/native-v1/',import.meta.url);
fs.mkdirSync(dir,{recursive:true});
const rgba=rgb=>[...rgb,255];
function pixel(x,y){
 const cell=Math.floor(y/8)*20+Math.floor(x/8),a=BG.attrs[cell],tile=BG.tiles[BG.map[cell]];
 const xx=a&32?7-x%8:x%8,yy=a&64?7-y%8:y%8,v=+tile[yy][xx];
 assert.equal(v,PIXELS[y][x]);return rgba(PALETTES[a&7][v]);
}
const pixels=Array.from({length:144},(_,y)=>Array.from({length:160},(_,x)=>pixel(x,y)));
writePng(new URL('background.png',dir),160,144,(x,y)=>pixels[y][x]);
writePng(new URL('background-4x.png',dir),640,576,(x,y)=>pixels[Math.floor(y/4)][Math.floor(x/4)]);
const uses=Array(BG.tiles.length).fill(0),firstPalette=[];
BG.map.forEach((v,i)=>{uses[v]++;firstPalette[v]??=BG.attrs[i]&7;});
const bytes=[];for(const t of BG.tiles)for(const row of t){let lo=0,hi=0;[...row].forEach((v,x)=>{lo|=(+v&1)<<(7-x);hi|=((+v>>1)&1)<<(7-x);});bytes.push(lo,hi);}
fs.writeFileSync(new URL('arena.2bpp',dir),Buffer.from(bytes));
fs.writeFileSync(new URL('arena.map',dir),Buffer.from(BG.map));
fs.writeFileSync(new URL('arena.attr',dir),Buffer.from(BG.attrs));
const stats={uniqueTiles:BG.tiles.length,availableTiles:176,mapCells:360,tilesUsedMoreThanOnce:uses.filter(n=>n>1).length,cellsUsingRepeatedTiles:BG.map.filter(t=>uses[t]>1).length,mirrorReuseCells:BG.attrs.filter(a=>a&96).length,tileBytes:bytes.length,mapAndAttributeBytes:720};
fs.writeFileSync(new URL('arena.json',dir),JSON.stringify({version:1,width:160,height:144,mapBase:80,geometry:GEOMETRY,palettes:PALETTES,...BG,stats},null,2)+'\n');
const atlasH=Math.ceil(BG.tiles.length/16)*10;
writePng(new URL('tile-atlas.png',dir),160*4,atlasH*4,(xx,yy)=>{const x=Math.floor(xx/4),y=Math.floor(yy/4),id=Math.floor(y/10)*16+Math.floor(x/10),lx=x%10-1,ly=y%10-1;return BG.tiles[id]&&lx>=0&&lx<8&&ly>=0&&ly<8?rgba(PALETTES[firstPalette[id]][+BG.tiles[id][ly][lx]]):[16,24,40,255];});
writePng(new URL('reuse-map.png',dir),640,576,(xx,yy)=>{
 const x=Math.floor(xx/4),y=Math.floor(yy/4),count=uses[BG.map[Math.floor(y/8)*20+Math.floor(x/8)]];
 return x%8===0||y%8===0?[24,32,48,255]:count===1?[224,152,64,255]:count<5?[64,136,160,255]:[64,184,112,255];
});
const keeper=JSON.parse(fs.readFileSync(new URL('../artwork/players/keeper-dive/study-v1/native-32x24.json',import.meta.url)));
const blue=character('spiky'),red=blue.palette.map((v,i)=>i===2?[232,64,72,255]:v);
const ballPalette=[[0,0,0,0],[248,248,232,255],[24,32,48,255],[120,144,152,255]];
const scenarios=[
 {name:'READY',pose:keeper.low[0].rows,x:80,y:63},
 {name:'LOW DIVE',pose:keeper.low[4].rows,x:63,y:66,flip:true},
 {name:'HIGH DIVE',pose:keeper.high[4].rows,x:99,y:44},
 {name:'HIGH JUMP',pose:keeper.low[0].rows,x:80,y:40},
];
const font=decodePng(fs.readFileSync(new URL('../artwork/Fonts for GB Studio/light/default/ascii.png',import.meta.url)));
function text(dst,s,x,y,color){for(const ch of s){const c=ch.charCodeAt(0)-32;for(let yy=0;yy<8;yy++)for(let xx=0;xx<8;xx++){const p=((Math.floor(c/16)*8+yy)*font.width+(c%16)*8+xx)*4;if(font.rgba[p]+font.rgba[p+1]+font.rgba[p+2]<300&&dst[y+yy]?.[x+xx])dst[y+yy][x+xx]=color;}x+=8;}}
const proofs=scenarios.map(s=>{
 const p=pixels.map(r=>r.map(v=>[...v]));
 const stamp=(rows,left,top,pal)=>rows.forEach((r,y)=>[...r].forEach((v,x)=>{if(v!=='.'&&p[top+y]?.[left+x])p[top+y][left+x]=pal[+v];}));
 const actor=(rows,left,top,pal,flip=false)=>stamp(flip?rows.map(r=>[...r].reverse().join('')):rows,left,top,pal);
 const shadow=(x,y)=>{for(let xx=x-4;xx<x+4;xx++)p[y][xx]=[24,64,80,255];};
 shadow(62,132);shadow(s.x,86);shadow(80,119);
 actor(blue.views[1][0],54,109,blue.palette);
 actor(s.pose,s.x-16,s.y,red,s.flip);
 actor(BALL_FRAMES[0],76,110,ballPalette);
 // The real window overlays the last eight rows; show it in the review.
 for(let y=136;y<144;y++)for(let x=0;x<160;x++)p[y][x]=[24,32,48,255];
 text(p,'YOU 0-0 CPU',0,136,[248,240,208,255]);
 writePng(new URL(`${s.name.toLowerCase().replaceAll(' ','-')}.png`,dir),160,144,(x,y)=>p[y][x]);
 return p;
});
// Ensure the real highest central jump stays below the crossbar's bottom edge.
const jump=keeper.low[0].rows;const crown=jump.findIndex(r=>/[123]/.test(r))+40;
assert.ok(crown>=GEOMETRY.goal.top+6,'High-jump crown needs two clear pixels below the crossbar');
for(const x of GEOMETRY.targets.x)for(const y of GEOMETRY.targets.y)assert.ok(x>43&&x<116&&y>GEOMETRY.goal.top+3&&y<88,'Target outside opening');
const proofH=156;
const labels=scenarios.map(s=>{const p=Array.from({length:12},()=>Array.from({length:160},()=>[24,32,48,255]));text(p,s.name,4,2,[248,240,208,255]);return p;});
writePng(new URL('keeper-clearance-3x.png',dir),960,proofH*2*3,(xx,yy)=>{
 const x=Math.floor(xx/3),y=Math.floor(yy/3),panel=Math.floor(y/proofH)*2+Math.floor(x/160),ly=y%proofH;
 if(ly>=12)return proofs[panel][ly-12][x%160];
 return labels[panel][ly][x%160];
});
// Compact named texture swatches: the source of the repeated background cells.
const textureEntries=Object.entries(TEXTURES),kitW=192,kitH=textureEntries.length*24;
const kit=Array.from({length:kitH},()=>Array.from({length:kitW},()=>[24,32,48,255]));
textureEntries.forEach(([name,t],i)=>{
 t.rows.forEach((r,y)=>[...r].forEach((v,x)=>{for(let dy=0;dy<2;dy++)for(let dx=0;dx<2;dx++)kit[i*24+4+y*2+dy][4+x*2+dx]=rgba(PALETTES[t.pal][+v]);}));
 text(kit,name.toUpperCase(),28,i*24+8,[248,240,208,255]);
});
writePng(new URL('texture-kit.png',dir),kitW*3,kitH*3,(x,y)=>kit[Math.floor(y/3)][Math.floor(x/3)]);
console.log(JSON.stringify({...stats,highJumpCrownY:crown,crossbarBottomY:GEOMETRY.goal.top+3},null,2));
