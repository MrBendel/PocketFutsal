// Technical reduction of the built-in ImageGen mockups; no pose construction.
import fs from 'node:fs';
import zlib from 'node:zlib';
import {decodePng} from './assets.mjs';
import {encodePng} from '../harness/node.mjs';
const dir=new URL('../artwork/players/roster/reactions/study-v2/',import.meta.url);
const ids=['spiky','cap','crop','bandana'],outcomes=['win','loss'];
const sources=Object.fromEntries(outcomes.map(o=>[o,decodePng(fs.readFileSync(new URL(`${o}-source.png`,dir)))]));
const rects={};let maxW=0,maxH=0;
for(const outcome of outcomes){const im=sources[outcome],n=im.width*im.height,labels=new Int32Array(n),queue=new Int32Array(n),components=[];let serial=0;
 for(let p=0;p<n;p++){
  if(labels[p]||im.rgba[p*4+3]<200)continue;
  const id=++serial;let start=0,end=0,l=im.width,r=0,t=im.height,b=0;queue[end++]=p;labels[p]=id;
  while(start<end){const z=queue[start++],x=z%im.width,y=Math.floor(z/im.width);l=Math.min(l,x);r=Math.max(r,x);t=Math.min(t,y);b=Math.max(b,y);
   for(let yy=Math.max(0,y-1);yy<=Math.min(im.height-1,y+1);yy++)for(let xx=Math.max(0,x-1);xx<=Math.min(im.width-1,x+1);xx++){
    const k=yy*im.width+xx;if(!labels[k]&&im.rgba[k*4+3]>=200){labels[k]=id;queue[end++]=k;}
   }
  }
  if(end>1000)components.push({id,left:l,top:t,right:r,bottom:b,pixels:end,row:Math.floor(((t+b)/2)/(im.height/4))});
 }
 if(components.length!==24)throw new Error(`${outcome}: expected 24 silhouettes, found ${components.length}`);
 im.labels=labels;rects[outcome]=[];
 for(let row=0;row<4;row++){
  const group=components.filter(c=>c.row===row).sort((a,b)=>a.left-b.left);
  if(group.length!==6)throw new Error(`${outcome}: row ${row} has ${group.length} silhouettes`);
  group.forEach((box,col)=>{
   let footL=im.width,footR=-1;
   for(let y=box.bottom-Math.round((box.bottom-box.top)*.06);y<=box.bottom;y++)for(let x=box.left;x<=box.right;x++)if(labels[y*im.width+x]===box.id){footL=Math.min(footL,x);footR=Math.max(footR,x);}
   rects[outcome].push({...box,col,footX:(footL+footR)/2});
   maxW=Math.max(maxW,box.right-box.left+1);maxH=Math.max(maxH,box.bottom-box.top+1);
  });
 }
}
function crc(buf){let c=0xffffffff;for(const b of buf){c^=b;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;}return(c^0xffffffff)>>>0;}
function png(name,w,h,pixel){
 const raw=Buffer.alloc((w*4+1)*h);for(let y=0;y<h;y++)for(let x=0;x<w;x++)raw.set(pixel(x,y),(w*4+1)*y+1+x*4);
 const chunk=(name,data)=>{const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(name,4);data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,-4)),b.length-4);return b;};
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=6;
 fs.writeFileSync(new URL(name,dir),Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
const palette=i=>[[0,0,0,0],i===2?[184,120,72,255]:[248,200,136,255],i<2?[40,120,232,255]:[232,64,72,255],[24,32,48,255]];
function reduce(w,h){const scale=Math.min(w/maxW,(h-1)/maxH),characters={};
 for(let row=0;row<4;row++){
  const pal=palette(row),character={palette:pal};
  for(const outcome of outcomes){const im=sources[outcome];character[outcome]=[];
   for(const box of rects[outcome].filter(r=>r.row===row)){
    const pixels=Array.from({length:h},()=>Array(w).fill(0));
    const colorAt=(x,y)=>{
     if(x<0||y<0||x>=im.width||y>=im.height||im.labels[y*im.width+x]!==box.id)return 0;
     const p=(y*im.width+x)*4;
     const [r,g,b]=im.rgba.subarray(p,p+3);
     // Flatten material shading before voting: shaded skin must not become
     // extra black facial marks, and jersey shadows should stay team colored.
     if(r>g*1.06&&r<g*2.2&&g>b*1.12&&r>55)return 1;
     if(row<2&&b>r*1.25&&b>g*1.4&&b>80)return 2;
     if(row>=2&&r>g*2.2&&r>b*1.6&&r>55)return 2;
     if(r+g+b>580&&Math.abs(r-g)<80)return 1;
     return 3;
    };
    for(let y=0;y<h;y++)for(let x=0;x<w;x++){
     const votes=[0,0,0,0];
     const xa=Math.floor(box.footX+(x-w/2)/scale),xb=Math.ceil(box.footX+(x+1-w/2)/scale);
     const ya=Math.floor(box.bottom+(y-(h-1))/scale),yb=Math.ceil(box.bottom+(y+1-(h-1))/scale);
     for(let sy=ya;sy<yb;sy++)for(let sx=xa;sx<xb;sx++)votes[colorAt(sx,sy)]++;
     const total=votes.reduce((a,b)=>a+b,0);if(votes[0]>total*.6)continue;
     let c=1;for(let v=2;v<4;v++)if(votes[v]>votes[c])c=v;pixels[y][x]=c;
    }
    character[outcome].push({rows:pixels.map(r=>r.map(c=>c||'.').join('')),source:box});
   }
  }
  const baseline=Math.max(...rects.win.filter(r=>r.row===row&&[0,1,5].includes(r.col)).map(r=>r.bottom));
  character.jump=rects.win.filter(r=>r.row===row).map(r=>Math.max(0,Math.round((baseline-r.bottom)*scale)));
  characters[ids[row]]=character;
 }
 return {width:w,height:h,framesPerOutcome:6,scale,method:'Whole connected silhouettes; common scale; feet anchored; alpha >=200; material-aware palette flattening and area vote',characters};
}
for(const [w,h]of [[16,24],[32,48]]){
 const data=reduce(w,h);fs.writeFileSync(new URL(`native-${w}x${h}.json`,dir),JSON.stringify(data,null,2)+'\n');
 for(const outcome of outcomes){
  png(`${outcome}-${w}x${h}.png`,w*6,h*4,(x,y)=>{const c=data.characters[ids[Math.floor(y/h)]],v=c[outcome][Math.floor(x/w)].rows[y%h][x%w];return c.palette[v==='.'?0:+v];});
  const gap=4,cw=w+gap,ch=h+gap,scale=w===16?6:3;
  png(`${outcome}-${w}x${h}-preview.png`,cw*6*scale,ch*4*scale,(x,y)=>{
   const px=Math.floor(x/scale),py=Math.floor(y/scale),row=Math.floor(py/ch),col=Math.floor(px/cw),lx=px%cw-2,ly=py%ch-2;
   const bg=((Math.floor(px/2)+Math.floor(py/2))&1)?[56,72,88,255]:[64,80,96,255];
   if(lx<0||ly<0||lx>=w||ly>=h)return bg;const c=data.characters[ids[row]],v=c[outcome][col].rows[ly][lx];return v==='.'?bg:c.palette[+v];
  });
 }
 console.log(`${w}x${h}: scale ${data.scale.toFixed(4)}, jump offsets ${ids.map(id=>data.characters[id].jump.join(',')).join(' / ')}`);
}
// Native-size loop with only the whole-body jump separated from the pose art.
const data=JSON.parse(fs.readFileSync(new URL('native-16x24.json',dir)));
const colors=[[40,56,72],[248,200,136],[184,120,72],[40,120,232],[232,64,72],[24,32,48],[248,232,64],[248,248,232]],word=n=>[n&255,n>>8],gw=384,gh=576;
const blocks=[Buffer.from('GIF89a'),Buffer.from([...word(gw),...word(gh),0xf2,0,0,...colors.flat()]),Buffer.from([0x21,0xff,11,...Buffer.from('NETSCAPE2.0'),3,1,0,0,0])];
for(let f=0;f<6;f++){
 const native=new Uint8Array(96*144);
 ids.forEach((id,row)=>outcomes.forEach((o,side)=>{
  const c=data.characters[id],step=side?[1,2,3,4,5,3][f]:f,pose=c[o][step].rows,ox=side?60:20,ground=8+row*34,oy=ground-(side?0:c.jump[f]);
  for(let x=4;x<12;x++)native[(ground+23)*96+ox+x]=5;
  pose.forEach((r,y)=>[...r].forEach((v,x)=>{if(v!=='.'&&oy+y>=0)native[(oy+y)*96+ox+x]=v==='1'?(row===2?2:1):v==='2'?(row<2?3:4):5;}));
 }));
 const pixels=Array.from({length:gw*gh},(_,p)=>native[Math.floor(Math.floor(p/gw)/4)*96+Math.floor((p%gw)/4)]),codes=[];
 for(let p=0;p<pixels.length;p+=6)codes.push(8,...pixels.slice(p,p+6));codes.push(9);
 const packed=Buffer.alloc(Math.ceil(codes.length/2));codes.forEach((c,i)=>packed[i>>1]|=c<<((i&1)*4));
 blocks.push(Buffer.from([0x21,0xf9,4,4,...word([18,12,12,22,12,20][f]),0,0,0x2c,0,0,0,0,...word(gw),...word(gh),0,3]));
 for(let p=0;p<packed.length;p+=255){const b=packed.subarray(p,p+255);blocks.push(Buffer.from([b.length]),b);}blocks.push(Buffer.from([0]));
}
blocks.push(Buffer.from([0x3b]));fs.writeFileSync(new URL('native-16x24-loop.gif',dir),Buffer.concat(blocks));
