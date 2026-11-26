// Opaque four-color dialogue portraits, reduced from the approved generated art.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
import {writePng} from './study-raster.mjs';
const root=new URL('../artwork/players/portraits/',import.meta.url),dir=new URL('v2/',root);
fs.mkdirSync(dir,{recursive:true});
const prior=JSON.parse(fs.readFileSync(new URL('v1/portraits-32x33.json',root)));
const source=decodePng(fs.readFileSync(new URL('v1/generated-source.png',root)));
const characters=prior.characters.map((c,i)=>{
 const palette=c.palette.map(p=>[...p]);
 // Reuse the warm backdrop for nose, cheek and neck shadows: four colors total.
 palette[0]=i===2?[112,64,40,255]:[176,112,72,255];
 const rows=c.rows.map((r,y)=>[...r].map((v,x)=>{
  if(v==='.')return '0';if(v!=='1')return v;
  const s=prior.sourceScale,b=c.source,cx=(b.left+b.right)/2;
  const xa=Math.floor(cx+(x-1-15)/s),xb=Math.ceil(cx+(x-15)/s);
  const ya=Math.floor(b.bottom+(y-30)/s),yb=Math.ceil(b.bottom+(y-29)/s);
  let skin=0,shadow=0;
  for(let yy=ya;yy<yb;yy++)for(let xx=xa;xx<xb;xx++){
   if(xx<b.left||xx>b.right||yy<b.top||yy>b.bottom)continue;
   const p=(yy*source.width+xx)*4,[r,g,blue,a]=source.rgba.subarray(p,p+4);
   if(a<200||!(r>g*1.06&&r<g*2.2&&g>blue*1.12&&r>55))continue;
   skin++;if(g<(i===2?96:145)&&r<(i===2?170:220))shadow++;
  }
  return skin&&shadow/skin>.45?'0':'1';
 }).join(''));
 const mouth=c.mouth;
 assert.equal(rows[mouth.y].slice(mouth.x,mouth.x+mouth.width),'3'.repeat(mouth.width));
 assert.ok(rows.some((r,y)=>[...r].some((v,x)=>v==='0'&&c.rows[y][x]==='1')),'Source skin shading must survive');
 for(const r of rows)assert.match(r,/^[0123]{32}$/);
 const result={...c,palette,rows};
 writePng(new URL(`${c.id}-32x33.png`,dir),32,33,(x,y)=>palette[+rows[y][x]]);
 const bytes=[];for(let y=0;y<33;y+=8)for(let x=0;x<32;x+=8)for(let yy=0;yy<8;yy++){
  let lo=0,hi=0;for(let xx=0;xx<8;xx++){const v=Number(rows[y+yy]?.[x+xx])||0;lo|=(v&1)<<(7-xx);hi|=((v>>1)&1)<<(7-xx);}bytes.push(lo,hi);
 }fs.writeFileSync(new URL(`${c.id}-32x33.2bpp`,dir),Buffer.from(bytes));
 return result;
});
const out={...prior,version:2,opaque:true,paletteRoles:['background and skin shadow','skin','team','outline and hair'],source:'../v1/generated-source.png',characters};
fs.writeFileSync(new URL('portraits-32x33.json',dir),JSON.stringify(out,null,2)+'\n');
writePng(new URL('portraits-32x33.png',dir),64,66,(x,y)=>{const c=characters[Math.floor(y/33)*2+Math.floor(x/32)];return c.palette[+c.rows[y%33][x%32]];});
writePng(new URL('portraits-preview.png',dir),432,444,(xx,yy)=>{
 const x=Math.floor(xx/6),y=Math.floor(yy/6),c=characters[Math.floor(y/37)*2+Math.floor(x/36)],lx=x%36-2,ly=y%37-2;
 return lx>=0&&ly>=0&&lx<32&&ly<33?c.palette[+c.rows[ly][lx]]:[24,32,48,255];
});
const panels=characters.map(c=>{
 const im=decodePng(fs.readFileSync(new URL(`v1/${c.id}-dialogue.png`,root)));
 const pixel=(x,y)=>x>=6&&x<42&&y>=7&&y<49
  ?(x>=8&&x<40&&y>=12&&y<45?c.palette[+c.rows[y-12][x-8]]:c.palette[0])
  :im.rgba.subarray((y*160+x)*4,(y*160+x)*4+4);
 writePng(new URL(`${c.id}-dialogue.png`,dir),160,56,pixel);return pixel;
});
writePng(new URL('dialogue-preview.png',dir),640,896,(x,y)=>panels[Math.floor(y/224)](Math.floor(x/4),Math.floor(y/4)%56));
console.log('Exported four opaque 32x33 portraits with four colors each and source-derived skin shading.');
