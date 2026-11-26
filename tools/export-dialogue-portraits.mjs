// Technical reduction of generated dialogue heads; never rescale gameplay heads.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
import {silhouettes,reducePose,writePng} from './study-raster.mjs';
const dir=new URL('../artwork/players/portraits/v1/',import.meta.url);
const im=decodePng(fs.readFileSync(new URL('generated-source.png',dir)));
const {labels,frames}=silhouettes(im,2,2),ids=['spiky','cap','crop','bandana'];
const palette=i=>[[0,0,0,0],i===2?[184,120,72,255]:[248,200,136,255],i<2?[40,120,232,255]:[232,64,72,255],[24,32,48,255]];
const data=[];
for(const size of [32,48]){
 const scale=Math.min((size-2)/Math.max(...frames.map(f=>f.right-f.left+1)),(size-2)/Math.max(...frames.map(f=>f.bottom-f.top+1)));
 const characters=frames.map((source,i)=>{
  // Center inside a transparent one-pixel safe margin without changing aspect.
  const reduced=reducePose(im,labels,source,size-2,size-2,scale,i>=2);
  const rows=['.'.repeat(size),...reduced.map(r=>'.'+r+'.'),'.'.repeat(size)];
  assert.equal(rows.length,size);for(const r of rows)assert.match(r,new RegExp(`^[.123]{${size}}$`));
  const c={id:ids[i],palette:palette(i),rows,source,rect:{x:(i%2)*size,y:Math.floor(i/2)*size,width:size,height:size}};
  writePng(new URL(`${c.id}-${size}.png`,dir),size,size,(x,y)=>c.palette[Number(rows[y][x])||0]);
  // Row-major background tile order: suitable for a tiled dialogue panel.
  const bytes=[];for(let y=0;y<size;y+=8)for(let x=0;x<size;x+=8)for(let yy=0;yy<8;yy++){
   let lo=0,hi=0;for(let xx=0;xx<8;xx++){const v=Number(rows[y+yy][x+xx])||0;lo|=(v&1)<<(7-xx);hi|=((v>>1)&1)<<(7-xx);}bytes.push(lo,hi);
  }
  fs.writeFileSync(new URL(`${c.id}-${size}.2bpp`,dir),Buffer.from(bytes));return c;
 });
 const out={version:1,width:size*2,height:size*2,portraitWidth:size,portraitHeight:size,sourceScale:scale,tileOrder:'row-major 8x8 tiles; low/high bitplane per scanline',characters};data.push(out);
 fs.writeFileSync(new URL(`portraits-${size}.json`,dir),JSON.stringify(out,null,2)+'\n');
 writePng(new URL(`portraits-${size}.png`,dir),size*2,size*2,(x,y)=>{const c=characters[Math.floor(y/size)*2+Math.floor(x/size)];return c.palette[Number(c.rows[y%size][x%size])||0];});
 const cw=size+4,s=size===32?6:4;
 writePng(new URL(`portraits-${size}-preview.png`,dir),cw*2*s,cw*2*s,(xx,yy)=>{
  const x=Math.floor(xx/s),y=Math.floor(yy/s),c=characters[Math.floor(y/cw)*2+Math.floor(x/cw)],lx=x%cw-2,ly=y%cw-2;
  const v=lx>=0&&ly>=0&&lx<size&&ly<size?Number(c.rows[ly][lx])||0:0;
  return v?c.palette[v]:((Math.floor(x/4)+Math.floor(y/4))%2?[56,72,88,255]:[64,80,96,255]);
 });
}
// Lossless individual high-fidelity crops; retain original RGBA samples.
frames.forEach((b,i)=>writePng(new URL(`${ids[i]}-source.png`,dir),b.right-b.left+1,b.bottom-b.top+1,(x,y)=>im.rgba.subarray(((b.top+y)*im.width+b.left+x)*4,((b.top+y)*im.width+b.left+x)*4+4)));
const font=decodePng(fs.readFileSync(new URL('../artwork/Fonts for GB Studio/light/default/ascii.png',import.meta.url)));
const native=data[0],panels=[],cream=[248,240,208,255],ink=[24,32,48,255];
const lines=[['NICE PASS!',"LET'S GO!"],['YOUR BALL.','I AM OPEN!'],['STAY SHARP.','WE GOT THIS.'],['ONE MORE!','KEEP MOVING.']];
for(const [i,c]of native.characters.entries()){
 const pixels=Array.from({length:160*56},()=>[32,48,64,255]);
 const put=(x,y,color)=>{if(x>=0&&y>=0&&x<160&&y<56)pixels[y*160+x]=color;};
 const rect=(x,y,w,h,color)=>{for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)put(xx,yy,color);};
 rect(1,1,158,54,cream);rect(3,3,154,50,ink);rect(6,7,36,42,[48,72,88,255]);
 c.rows.forEach((r,y)=>[...r].forEach((v,x)=>{if(v!=='.')put(8+x,12+y,c.palette[+v]);}));
 const text=(s,x,y,color)=>{for(const ch of s){const k=ch.charCodeAt(0)-32;assert.ok(k>=0&&k<64);for(let yy=0;yy<8;yy++)for(let xx=0;xx<8;xx++){
  const p=((Math.floor(k/16)*8+yy)*font.width+(k%16)*8+xx)*4;
  if(font.rgba[p]+font.rgba[p+1]+font.rgba[p+2]<300)put(x+xx,y+yy,color);
 }x+=8;}};
 text(c.id.toUpperCase(),48,8,c.palette[2]);text(lines[i][0],48,24,cream);text(lines[i][1],48,36,cream);
 panels.push(pixels);writePng(new URL(`${c.id}-dialogue.png`,dir),160,56,(x,y)=>pixels[y*160+x]);
}
writePng(new URL('dialogue-preview.png',dir),640,896,(x,y)=>panels[Math.floor(y/(56*4))][Math.floor(y/4)%56*160+Math.floor(x/4)]);
console.log(`Exported ${frames.length} portraits at 32x32 and 48x48, individual source crops, tile archives and dialogue mockups.`);
