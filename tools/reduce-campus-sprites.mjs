// Purpose-built 16x16 campus walkers, reduced from the approved master study.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
import {silhouettes,reducePose,writePng,writeGif} from './study-raster.mjs';
const version=Number(process.argv[2]??2);
assert.ok(version===1||version===2,'Supported campus study versions: 1, 2');
const dir=new URL('../artwork/players/campus/v'+version+'/',import.meta.url);
fs.mkdirSync(dir,{recursive:true});
const im=decodePng(fs.readFileSync(new URL('../artwork/players/campus/v1/generated-source.png',import.meta.url)));
const {labels,frames}=silhouettes(im,9,4,200),ids=['spiky','cap','crop','bandana'];
const directions=['down','up','right','left'],phases=['idle','step-a','step-b'];
const scale=Math.min(14/Math.max(...frames.map(f=>f.right-f.left+1)),14/Math.max(...frames.map(f=>f.bottom-f.top+1)));
const palette=i=>[[0,0,0,0],i===2?[184,120,72,255]:[248,200,136,255],i<2?[40,120,232,255]:[232,64,72,255],[24,32,48,255]];
// Eyes are isolated dark strokes inside the source face. Ordinary area voting
// can erase a stroke narrower than a destination pixel, so preserve landmarks.
function eyes(box,expected){
 const w=box.right-box.left+1,h=box.bottom-box.top+1,n=w*h,seen=new Uint8Array(n),queue=new Int32Array(n),found=[];
 const dark=(x,y)=>{const p=((box.top+y)*im.width+box.left+x)*4;return labels[(box.top+y)*im.width+box.left+x]===box.id&&im.rgba[p]<75&&im.rgba[p+1]<75&&im.rgba[p+2]<95;};
 for(let y=0;y<h*.65;y++)for(let x=0;x<w;x++){
  const first=y*w+x;if(seen[first]||!dark(x,y))continue;
  let a=0,b=0,l=w,r=0,t=h,d=0;queue[b++]=first;seen[first]=1;
  while(a<b){const p=queue[a++],xx=p%w,yy=Math.floor(p/w);l=Math.min(l,xx);r=Math.max(r,xx);t=Math.min(t,yy);d=Math.max(d,yy);
   for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){
    const nx=xx+dx,ny=yy+dy,k=ny*w+nx;if(nx<0||nx>=w||ny<0||ny>=h*.65||seen[k]||!dark(nx,ny))continue;seen[k]=1;queue[b++]=k;
   }
  }
  const cy=(t+d)/2;
  if(b>40&&r-l+1<w*.21&&d-t+1>h*.045&&d-t+1<h*.25&&cy>h*.27&&cy<h*.6)found.push({left:box.left+l,right:box.left+r,top:box.top+t,bottom:box.top+d,pixels:b});
 }
 const selected=found.sort((a,b)=>b.pixels-a.pixels).slice(0,expected).sort((a,b)=>a.left-b.left);
 assert.equal(selected.length,expected,'Expected separate source eye landmarks');return selected;
}
const characters=ids.map((id,col)=>{
 const animations={};const bytes=[];
 for(let view=0;view<3;view++){
  animations[directions[view]]=phases.map((phase,index)=>{
   const source=frames.find(f=>f.row===view*3+index&&f.col===col);
   const rows=reducePose(im,labels,source,16,16,scale,col>=2);
   assert.equal(rows.length,16);for(const r of rows)assert.match(r,/^[.123]{16}$/);
   return {phase,rows,flipX:false,source};
  });
  const cycle=animations[directions[view]],idle=cycle[0],p=idle.rows.map(r=>[...r]);
  const landmarks=view===1?[]:eyes(idle.source,view===0?2:1);
  idle.eyes=landmarks.map(e=>{
   const sx=(e.left+e.right)/2,sy=(e.top+e.bottom)/2,cx=(idle.source.left+idle.source.right)/2;
   const x=Math.floor(8+(sx-cx)*scale),cy=15+(sy-idle.source.bottom)*scale;
   const height=Math.max(1,Math.round((e.bottom-e.top+1)*scale)),y=Math.round(cy-(height-1)/2);
   for(let yy=y;yy<y+height;yy++){assert.match(p[yy][x],/^[13]$/,'Eye landmark must remain inside the head');p[yy][x]='3';}
   return {x,y,height,source:e};
  });
  idle.rows=p.map(r=>r.join(''));
  const headRows=version===2?12:(view===2?11:10);
  cycle.slice(1).forEach(f=>{
   if(version===1){f.rows=[...idle.rows.slice(0,headRows),...f.rows.slice(headRows)];f.headFrom='idle';f.eyes=idle.eyes;return;}
   // One-pixel settling of the entire upper body on foot contact. Keep the
   // neck/shoulders with the head; never translate a floating face patch.
   f.rows=['................',...idle.rows.slice(0,headRows),...f.rows.slice(headRows+1)];
   f.upperBodyOffset={x:0,y:1};
   f.eyes=idle.eyes.map(e=>({...e,y:e.y+1}));
  });
 }
 // Pack after preserving features; two 8x16 columns per frame.
 for(const view of directions.slice(0,3))for(const f of animations[view]){
  f.byteOffset=bytes.length;
  for(let x=0;x<16;x+=8)for(let y=0;y<16;y+=8)for(let yy=0;yy<8;yy++){
   let lo=0,hi=0;for(let xx=0;xx<8;xx++){const v=Number(f.rows[y+yy][x+xx])||0;lo|=(v&1)<<(7-xx);hi|=((v>>1)&1)<<(7-xx);}bytes.push(lo,hi);
  }
 }
 animations.left=animations.right.map(f=>({...f,rows:f.rows.map(r=>[...r].reverse().join('')),eyes:f.eyes.map(e=>({...e,x:15-e.x})),flipX:true,mirroredFrom:'right'}));
 const c={id,width:16,height:16,palette:palette(col),origin:{x:8,y:15},walkCycle:[0,1,0,2],stepMs:140,tileOrder:'column-major 8x8 tiles, two 8x16 sprite columns per frame',animations};
 for(const d of directions)assert.equal(new Set(animations[d].map(f=>f.rows.join(''))).size,3,`${id}/${d}: walk poses must remain distinct`);
 fs.writeFileSync(new URL(`${id}.2bpp`,dir),Buffer.from(bytes));
 fs.writeFileSync(new URL(`${id}.json`,dir),JSON.stringify(c,null,2)+'\n');
 writePng(new URL(`${id}-16.png`,dir),48,64,(x,y)=>c.palette[Number(animations[directions[Math.floor(y/16)]][Math.floor(x/16)].rows[y%16][x%16])||0]);
 return c;
});
fs.writeFileSync(new URL('campus.json',dir),JSON.stringify({version,frameWidth:16,frameHeight:16,directions,phases,sourceScale:scale,characters},null,2)+'\n');
writePng(new URL('roster-16.png',dir),192,64,(x,y)=>{
 const c=characters[Math.floor(y/16)],cell=Math.floor(x/16),f=c.animations[directions[Math.floor(cell/3)]][cell%3];return c.palette[Number(f.rows[y%16][x%16])||0];
});
const font=decodePng(fs.readFileSync(new URL('../artwork/Fonts for GB Studio/light/default/ascii.png',import.meta.url)));
function text(dst,s,x,y,ink){for(const ch of s){const k=ch.charCodeAt(0)-32;for(let yy=0;yy<8;yy++)for(let xx=0;xx<8;xx++){
 const p=((Math.floor(k/16)*8+yy)*font.width+(k%16)*8+xx)*4;if(font.rgba[p]+font.rgba[p+1]+font.rgba[p+2]<300)dst[y+yy][x+xx]=ink;
 }x+=8;}}
const preview=Array.from({length:184},(_,y)=>Array.from({length:144},(_,x)=>(Math.floor(x/4)+Math.floor(y/4))%2?[56,72,88,255]:[64,80,96,255]));
characters.forEach((c,i)=>{
 const bx=i%2*72,by=Math.floor(i/2)*92;
 text(preview,c.id.toUpperCase(),bx+4,by+2,[248,240,208,255]);
 directions.forEach((d,v)=>{
  text(preview,['DN','UP','RT','LT'][v],bx,by+16+v*20,[248,240,208,255]);
  c.animations[d].forEach((f,p)=>f.rows.forEach((r,y)=>[...r].forEach((n,x)=>{if(n!=='.')preview[by+14+v*20+y][bx+20+p*16+x]=c.palette[+n];})));
 });
});
writePng(new URL('roster-preview.png',dir),576,736,(x,y)=>preview[Math.floor(y/4)][Math.floor(x/4)]);
const colors=[[176,184,152],[248,200,136],[184,120,72],[40,120,232],[232,64,72],[24,32,48],[128,144,120],[80,128,72]];
const w=96,h=104;
const gifFrames=[0,1,0,2].map(phase=>{
 const p=Array.from({length:h},()=>Array(w).fill(0));
 ['DN','UP','RT','LT'].forEach((s,i)=>text(p,s,i*24+4,0,5));
 characters.forEach((c,row)=>directions.forEach((d,col)=>{
  const left=col*24+4,top=10+row*24;
  for(let x=left+5;x<left+11;x++)p[top+15][x]=6;
  c.animations[d][phase].rows.forEach((r,y)=>[...r].forEach((v,x)=>{if(v!=='.')p[top+y][left+x]=v==='1'?(row===2?2:1):v==='2'?(row<2?3:4):5;}));
 }));
 return {pixels:Uint8Array.from(p.flat()),delay:14};
});
writeGif(new URL('campus-walk-loop.gif',dir),w,h,colors,gifFrames,5);
console.log(JSON.stringify({source:[im.width,im.height],sourcePoses:frames.length,nativeFrames:48,uniqueTileFrames:36,tilesPerCharacter:36,bytesPerCharacter:576,scale},null,2));
