// Technical reduction of the generated keeper study; no rotated standing poses.
import fs from 'node:fs';
import zlib from 'node:zlib';
import {decodePng} from './assets.mjs';
import {BALL_FRAMES} from './ball-art.mjs';
const dir=new URL('../artwork/players/keeper-dive/study-v1/',import.meta.url);
const im=decodePng(fs.readFileSync(new URL('generated-source.png',dir))),n=im.width*im.height;
const labels=new Int32Array(n),queue=new Int32Array(n),components=[];let serial=0;
for(let p=0;p<n;p++){
 if(labels[p]||im.rgba[p*4+3]<200)continue;
 const id=++serial;let a=0,b=0,l=im.width,r=0,t=im.height,d=0;queue[b++]=p;labels[p]=id;
 while(a<b){const z=queue[a++],x=z%im.width,y=Math.floor(z/im.width);l=Math.min(l,x);r=Math.max(r,x);t=Math.min(t,y);d=Math.max(d,y);
  for(let yy=Math.max(0,y-1);yy<=Math.min(im.height-1,y+1);yy++)for(let xx=Math.max(0,x-1);xx<=Math.min(im.width-1,x+1);xx++){
   const k=yy*im.width+xx;if(!labels[k]&&im.rgba[k*4+3]>=200){labels[k]=id;queue[b++]=k;}
  }
 }
 if(b>1000)components.push({id,left:l,right:r,top:t,bottom:d,pixels:b,row:Math.floor(((t+d)/2)/(im.height/4))});
}
if(components.length!==16)throw new Error(`Expected 16 connected keeper silhouettes; found ${components.length}`);
const frames=[];
for(let row=0;row<4;row++){
 const group=components.filter(c=>c.row===row).sort((a,b)=>a.left-b.left);
 if(group.length!==4)throw new Error(`Row ${row} has ${group.length} silhouettes`);
 frames.push(...group.map((c,col)=>({...c,col})));
}
const maxW=Math.max(...frames.map(f=>f.right-f.left+1)),maxH=Math.max(...frames.map(f=>f.bottom-f.top+1));
const blue=[[0,0,0,0],[248,200,136,255],[40,120,232,255],[24,32,48,255]],red=blue.map(c=>[...c]);red[2]=[232,64,72,255];
const colorAt=(box,x,y)=>{
 if(x<0||y<0||x>=im.width||y>=im.height||labels[y*im.width+x]!==box.id)return 0;
 const p=(y*im.width+x)*4,[r,g,b]=im.rgba.subarray(p,p+3);
 if(r>g*1.06&&r<g*2.2&&g>b*1.12&&r>55)return 1;
 if(b>r*1.25&&b>g*1.4&&b>80)return 2;
 if(r+g+b>580&&Math.abs(r-g)<80)return 1;
 return 3;
};
function crc(buf){let c=0xffffffff;for(const b of buf){c^=b;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;}return(c^0xffffffff)>>>0;}
function png(name,w,h,pixel){
 const raw=Buffer.alloc((w*4+1)*h);for(let y=0;y<h;y++)for(let x=0;x<w;x++)raw.set(pixel(x,y),(w*4+1)*y+1+x*4);
 const chunk=(name,data)=>{const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(name,4);data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,-4)),b.length-4);return b;};
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=6;
 fs.writeFileSync(new URL(name,dir),Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
for(const [w,h]of [[32,24],[32,32]]){
 const scale=Math.min(w/maxW,(h-1)/maxH),poses=frames.map(box=>{
  const p=Array.from({length:h},()=>Array(w).fill(0)),cx=(box.left+box.right)/2;
  for(let y=0;y<h;y++)for(let x=0;x<w;x++){
   const votes=[0,0,0,0],xa=Math.floor(cx+(x-w/2)/scale),xb=Math.ceil(cx+(x+1-w/2)/scale),ya=Math.floor(box.bottom+(y-h+1)/scale),yb=Math.ceil(box.bottom+(y+1-h+1)/scale);
   for(let yy=ya;yy<yb;yy++)for(let xx=xa;xx<xb;xx++)votes[colorAt(box,xx,yy)]++;
   const total=votes.reduce((a,b)=>a+b,0);if(votes[0]>total*.6)continue;
   let c=1;for(let v=2;v<4;v++)if(votes[v]>votes[c])c=v;p[y][x]=c;
  }
  let l=w,r=-1,t=h,b=-1,handX=-1,handY=[];const tiles=[];
  p.forEach((row,y)=>row.forEach((v,x)=>{if(v){l=Math.min(l,x);r=Math.max(r,x);t=Math.min(t,y);b=Math.max(b,y);}if(v===1)handX=Math.max(handX,x);}));
  p.forEach((row,y)=>row.forEach((v,x)=>{if(v===1&&x>=handX-1)handY.push(y);}));
  for(let ty=0;ty<h;ty+=16)for(let tx=0;tx<w;tx+=8)if(p.slice(ty,ty+16).some(row=>row.slice(tx,tx+8).some(Boolean)))tiles.push({x:tx,y:ty});
  return {rows:p.map(row=>row.map(v=>v||'.').join('')),ink:{left:l,right:r,top:t,bottom:b},reach:{x:handX,y:Math.round(handY.reduce((a,b)=>a+b,0)/handY.length)},spriteColumns8x16:tiles,source:box};
 });
 const data={width:w,height:h,scale,framesPerDive:8,palette:blue,paletteRed:red,direction:'right',oppositeDirection:'horizontal mirror',savePose:4,phases:['ready','plant','push-off','airborne','reach','descend','land','recover'],low:poses.slice(0,8),high:poses.slice(8)};
 fs.writeFileSync(new URL(`native-${w}x${h}.json`,dir),JSON.stringify(data,null,2)+'\n');
 for(const [team,pal]of [['blue',blue],['red',red]]){
  png(`${team}-${w}x${h}.png`,w*4,h*4,(x,y)=>{const v=poses[Math.floor(y/h)*4+Math.floor(x/w)].rows[y%h][x%w];return pal[v==='.'?0:+v];});
  const cw=w+4,ch=h+4,s=5;
  png(`${team}-${w}x${h}-preview.png`,cw*4*s,ch*4*s,(x,y)=>{
   const px=Math.floor(x/s),py=Math.floor(y/s),row=Math.floor(py/ch),col=Math.floor(px/cw),lx=px%cw-2,ly=py%ch-2;
   const bg=((Math.floor(px/2)+Math.floor(py/2))&1)?[56,72,88,255]:[64,80,96,255];
   if(lx<0||ly<0||lx>=w||ly>=h)return bg;const v=poses[row*4+col].rows[ly][lx];return v==='.'?bg:pal[+v];
  });
 }
 console.log(`${w}x${h}: ${poses.length} poses, ${Math.min(...poses.map(p=>p.spriteColumns8x16.length))}..${Math.max(...poses.map(p=>p.spriteColumns8x16.length))} nonempty 8x16 sprite columns`);
}
// Pose-only artwork is separate from this illustrative travel/lift trajectory.
const data=JSON.parse(fs.readFileSync(new URL('native-32x24.json',dir)));
const colors=[[40,56,72],[248,200,136],[40,120,232],[24,32,48],[232,64,72],[248,248,232],[88,112,128],[240,224,160]],word=n=>[n&255,n>>8],gw=640,gh=224;
const dx=[0,0,5,12,20,24,24,24],lowLift=[0,0,2,5,5,3,0,0],highLift=[0,0,4,8,12,7,0,0];
function gif(mirror){
 const blocks=[Buffer.from('GIF89a'),Buffer.from([...word(gw),...word(gh),0xf2,0,0,...colors.flat()]),Buffer.from([0x21,0xff,11,...Buffer.from('NETSCAPE2.0'),3,1,0,0,0])];
 for(let f=0;f<8;f++){
  const native=new Uint8Array(160*56);
  const font={L:['100','100','100','100','111'],O:['111','101','101','101','111'],W:['10001','10001','10101','10101','01010'],H:['101','101','111','101','101'],I:['111','010','010','010','111'],G:['111','100','101','101','111']};
  function text(s,x){for(const ch of s){font[ch].forEach((r,y)=>[...r].forEach((v,xx)=>{if(v==='1')native[(2+y)*160+x+xx]=5;}));x+=font[ch][0].length+1;}}
  text('LOW',32);text('HIGH',112);
  ['low','high'].forEach((kind,panel)=>{
   const cx=panel*80+(mirror?56-dx[f]:24+dx[f]),ground=43,top=ground-23-(panel?highLift:lowLift)[f];
   for(let x=-5;x<5;x++)native[(ground+2)*160+cx+x]=6;
   data[kind][f].rows.forEach((row,y)=>[...row].forEach((v,x)=>{if(v!=='.')native[(top+y)*160+cx-16+(mirror?31-x:x)]=+v;}));
   // The existing game ball is a separate preview layer, never part of a pose.
   if(f>=1&&f<=5){
    const reach=data[kind][4].reach,contactX=panel*80+(mirror?56-dx[4]:24+dx[4])-16+(mirror?31-reach.x:reach.x)+(mirror?-3:3),contactY=ground-23-(panel?highLift:lowLift)[4]+reach.y;
    const incoming=panel*80+(mirror?4:75),t=Math.min(1,(f-1)/3);
    const bx=Math.round(f===5?contactX+(mirror?-10:10):incoming+(contactX-incoming)*t),by=Math.round(f===5?contactY-6:contactY-2*(1-t));
    BALL_FRAMES[f%6].forEach((row,y)=>[...row].forEach((v,x)=>{const xx=bx-4+x,yy=by-4+y;if(v!=='.'&&xx>=panel*80&&xx<(panel+1)*80&&yy>=0&&yy<56)native[yy*160+xx]=v==='1'?5:v==='2'?3:6;}));
   }
  });
  const pixels=Array.from({length:gw*gh},(_,p)=>native[Math.floor(Math.floor(p/gw)/4)*160+Math.floor((p%gw)/4)]),codes=[];
  for(let p=0;p<pixels.length;p+=6)codes.push(8,...pixels.slice(p,p+6));codes.push(9);
  const packed=Buffer.alloc(Math.ceil(codes.length/2));codes.forEach((c,i)=>packed[i>>1]|=c<<((i&1)*4));
  blocks.push(Buffer.from([0x21,0xf9,4,4,...word([18,10,10,10,18,10,12,30][f]),0,0,0x2c,0,0,0,0,...word(gw),...word(gh),0,3]));
  for(let p=0;p<packed.length;p+=255){const b=packed.subarray(p,p+255);blocks.push(Buffer.from([b.length]),b);}blocks.push(Buffer.from([0]));
 }
 blocks.push(Buffer.from([0x3b]));fs.writeFileSync(new URL(`dive-${mirror?'left':'right'}-loop.gif`,dir),Buffer.concat(blocks));
}
gif(false);gif(true);
