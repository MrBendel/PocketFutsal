// Dedicated head-only source art; v2 reserves index zero for transparency.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {decodePng} from './assets.mjs';
import {silhouettes,writePng} from './study-raster.mjs';
const version=Number(process.argv[2]??3);
assert.ok([1,2,3].includes(version),'Supported mugshot versions: 1, 2, 3');
const dir=new URL('../artwork/players/mugshots/v'+version+'/',import.meta.url);
fs.mkdirSync(dir,{recursive:true});
const im=decodePng(fs.readFileSync(new URL('../artwork/players/mugshots/v1/generated-source.png',import.meta.url)));
const {labels,frames}=silhouettes(im,2,2,200);
const ids=['spiky','cap','crop','bandana'];
// Preserve the source's isolated mouth stroke when area voting erases it.
function mouthBounds(box){
 const w=box.right-box.left+1,h=box.bottom-box.top+1,seen=new Uint8Array(w*h),queue=new Int32Array(w*h),found=[];
 const dark=(x,y)=>{const p=((box.top+y)*im.width+box.left+x)*4;return labels[(box.top+y)*im.width+box.left+x]===box.id&&im.rgba[p]<70&&im.rgba[p+1]<80&&im.rgba[p+2]<100;};
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const start=y*w+x;if(seen[start]||!dark(x,y))continue;
  let a=0,b=0,l=w,r=0,t=h,d=0;queue[b++]=start;seen[start]=1;
  while(a<b){const k=queue[a++],xx=k%w,yy=Math.floor(k/w);l=Math.min(l,xx);r=Math.max(r,xx);t=Math.min(t,yy);d=Math.max(d,yy);
   for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){
    const nx=xx+dx,ny=yy+dy,n=ny*w+nx;if(nx<0||ny<0||nx>=w||ny>=h||seen[n]||!dark(nx,ny))continue;seen[n]=1;queue[b++]=n;
   }
  }
  if((t+d)/2>h*.55&&(t+d)/2<h*.9&&r-l>w*.05&&r-l<w*.35&&d-t<h*.15&&r-l>(d-t)*1.5)found.push({left:box.left+l,right:box.left+r,top:box.top+t,bottom:box.top+d,pixels:b});
 }
 assert.equal(found.length,1,'Expected one isolated source mouth');return found[0];
}
const characters=frames.map((source,i)=>{
 const extent=version>=2?16:14;
 const scale=Math.min(extent/(source.right-source.left+1),extent/(source.bottom-source.top+1));
 const palette=[i===2?[112,64,40,255]:[192,136,88,255],i===2?[184,120,72,255]:[248,200,136,255],i===1?[248,232,192,255]:i===3?[232,64,72,255]:[48,56,80,255],[24,32,48,255]];
 if(version>=2)palette[0]=[0,0,0,0];
 const cx=(source.left+source.right)/2,cy=(source.top+source.bottom)/2;
 const rows=Array.from({length:16},()=>Array(16).fill('0'));
 for(let y=0;y<16;y++)for(let x=0;x<16;x++){
  const votes=[0,0,0,0];let inside=0,total=0;
  for(let sy=Math.floor(cy+(y-8)/scale);sy<Math.ceil(cy+(y+1-8)/scale);sy++)for(let sx=Math.floor(cx+(x-8)/scale);sx<Math.ceil(cx+(x+1-8)/scale);sx++){
   total++;if(sx<0||sy<0||sx>=im.width||sy>=im.height||labels[sy*im.width+sx]!==source.id)continue;
   inside++;const p=(sy*im.width+sx)*4,[r,g,b]=im.rgba.subarray(p,p+3);let v=3;
   if(i===3&&r>g*2&&r>b*1.6&&r>80)v=2;
   else if(i===1&&r+g+b>600)v=2;
   else if(r>g*1.06&&g>b*1.12&&r>65)v=version>=2?1:(g<(i===2?95:140)&&r<(i===2?165:220)?0:1);
   else if((i===0||i===2)&&r+g+b>110)v=2;
   votes[v]++;
  }
  if(inside<total*.5)continue;
  let best=0;for(let k=1;k<4;k++)if(votes[k]>votes[best])best=k;
  rows[y][x]=String(best);
 }
 const landmark=mouthBounds(source),width=2;
 const x=Math.round(8+((landmark.left+landmark.right)/2-cx)*scale-width/2),y=Math.floor(8+((landmark.top+landmark.bottom)/2-cy)*scale);
 for(let xx=x;xx<x+width;xx++){assert.ok(rows[y][xx]==='1'||rows[y][xx]==='3','Mouth stays within face');rows[y][xx]='3';}
 const c={id:ids[i],width:16,height:16,palette,rows:rows.map(r=>r.join('')),source,sourceScale:scale,mouth:{x,y,width,source:landmark}};
 return c;
});
if(version===3){
 // Editable native-pixel cleanup: face features and jaw contours are authored
 // together so source outlines cannot turn into a third eye or dangling neck.
 const cleanup=JSON.parse(fs.readFileSync(new URL('face-cleanup.json',dir)));
 for(const c of characters){
  const edit=cleanup[c.id];
  for(const [y,row]of Object.entries(edit.rows))c.rows[+y]=row;
  c.sourceMouth=c.mouth;
  c.mouth=edit.mouth;c.eyes=edit.eyes;c.nativeCleanup='face-cleanup.json';
 }
}
for(const c of characters){
 for(const r of c.rows)assert.match(r,/^[0123]{16}$/);
 const bytes=[];for(let y=0;y<16;y+=8)for(let x=0;x<16;x+=8)for(let yy=0;yy<8;yy++){
  let lo=0,hi=0;for(let xx=0;xx<8;xx++){const v=+c.rows[y+yy][x+xx];lo|=(v&1)<<(7-xx);hi|=((v>>1)&1)<<(7-xx);}bytes.push(lo,hi);
 }
 assert.equal(bytes.length,64);
 fs.writeFileSync(new URL(c.id+'-16.2bpp',dir),Buffer.from(bytes));
 writePng(new URL(c.id+'-16.png',dir),16,16,(x,y)=>c.palette[+c.rows[y][x]]);
}
fs.writeFileSync(new URL('mugshots-16.json',dir),JSON.stringify({version,width:32,height:32,opaque:version===1,tileOrder:'row-major 8x8 tiles; low/high bitplanes per row',characters},null,2)+'\n');
writePng(new URL('mugshots-16.png',dir),32,32,(x,y)=>{const c=characters[Math.floor(y/16)*2+Math.floor(x/16)];return c.palette[+c.rows[y%16][x%16]];});
writePng(new URL('mugshots-preview.png',dir),360,360,(x,y)=>{
 const xx=Math.floor(x/10),yy=Math.floor(y/10),c=characters[Math.floor(yy/18)*2+Math.floor(xx/18)],lx=xx%18-1,ly=yy%18-1;
 const v=lx>=0&&ly>=0&&lx<16&&ly<16?+c.rows[ly][lx]:0;
 if(version>=2)return v?c.palette[v]:((Math.floor(xx/3)+Math.floor(yy/3))%2?[56,72,88,255]:[64,80,96,255]);
 return lx>=0&&ly>=0&&lx<16&&ly<16?c.palette[v]:[24,32,48,255];
});
const font=decodePng(fs.readFileSync(new URL('../artwork/Fonts for GB Studio/light/default/ascii.png',import.meta.url)));
const pixels=Array.from({length:32},()=>Array.from({length:160},()=>[24,32,48,255]));
function rect(x,y,w,h,col){for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)pixels[yy][xx]=col;}
function text(s,x,y,col){for(const ch of s){const n=ch.charCodeAt(0)-32;for(let yy=0;yy<8;yy++)for(let xx=0;xx<8;xx++){const p=((Math.floor(n/16)*8+yy)*font.width+n%16*8+xx)*4;if(font.rgba[p]+font.rgba[p+1]+font.rgba[p+2]<300)pixels[y+yy][x+xx]=col;}x+=8;}}
rect(0,0,160,2,[248,232,192,255]);rect(0,30,160,2,[248,232,192,255]);
rect(4,5,38,22,[40,120,232,255]);rect(118,5,38,22,[232,64,72,255]);
characters.forEach((c,i)=>{const x=[6,24,120,138][i];c.rows.forEach((r,y)=>[...r].forEach((v,xx)=>{if(version===1||v!=='0')pixels[y+8][x+xx]=c.palette[+v];}));});
text('2 - 1',60,12,[248,232,192,255]);
writePng(new URL('scoreboard-native.png',dir),160,32,(x,y)=>pixels[y][x]);
writePng(new URL('scoreboard-preview.png',dir),640,128,(x,y)=>pixels[Math.floor(y/4)][Math.floor(x/4)]);
console.log(JSON.stringify(characters.map(c=>({id:c.id,scale:c.sourceScale,source:c.source,rows:c.rows})),null,2));
