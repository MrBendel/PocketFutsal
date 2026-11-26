// Lossless authoring helpers for generated sprite studies, never game runtime.
import fs from 'node:fs';
import zlib from 'node:zlib';
export function silhouettes(im,rows,cols,alpha=200){
 const n=im.width*im.height,labels=new Int32Array(n),queue=new Int32Array(n),found=[];let serial=0;
 for(let p=0;p<n;p++){
  if(labels[p]||im.rgba[p*4+3]<alpha)continue;
  const id=++serial;let a=0,b=0,l=im.width,r=0,t=im.height,d=0;queue[b++]=p;labels[p]=id;
  while(a<b){const z=queue[a++],x=z%im.width,y=Math.floor(z/im.width);l=Math.min(l,x);r=Math.max(r,x);t=Math.min(t,y);d=Math.max(d,y);
   for(let yy=Math.max(0,y-1);yy<=Math.min(im.height-1,y+1);yy++)for(let xx=Math.max(0,x-1);xx<=Math.min(im.width-1,x+1);xx++){
    const k=yy*im.width+xx;if(!labels[k]&&im.rgba[k*4+3]>=alpha){labels[k]=id;queue[b++]=k;}
   }
  }
  if(b>1000)found.push({id,left:l,right:r,top:t,bottom:d,pixels:b,row:Math.floor(((t+d)/2)/(im.height/rows))});
 }
 if(found.length!==rows*cols)throw new Error(`Expected ${rows*cols} silhouettes, found ${found.length}`);
 const frames=[];for(let row=0;row<rows;row++){
  const group=found.filter(c=>c.row===row).sort((a,b)=>a.left-b.left);
  if(group.length!==cols)throw new Error(`Row ${row}: expected ${cols}, found ${group.length}`);
  frames.push(...group.map((c,col)=>({...c,col})));
 }return {labels,frames};
}
export function materialColor(im,labels,box,x,y,red=false){
 if(x<0||y<0||x>=im.width||y>=im.height||labels[y*im.width+x]!==box.id)return 0;
 const p=(y*im.width+x)*4,[r,g,b]=im.rgba.subarray(p,p+3);
 if(r>g*1.06&&r<g*2.2&&g>b*1.12&&r>55)return 1;
 if(!red&&b>r*1.25&&b>g*1.4&&b>80)return 2;
 if(red&&r>g*2.2&&r>b*1.6&&r>55)return 2;
 if(r+g+b>580&&Math.abs(r-g)<80)return 1;
 return 3;
}
export function reducePose(im,labels,box,w,h,scale,red=false){
 const p=Array.from({length:h},()=>Array(w).fill('.')),cx=(box.left+box.right)/2;
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const votes=[0,0,0,0],xa=Math.floor(cx+(x-w/2)/scale),xb=Math.ceil(cx+(x+1-w/2)/scale),ya=Math.floor(box.bottom+(y-h+1)/scale),yb=Math.ceil(box.bottom+(y+1-h+1)/scale);
  for(let yy=ya;yy<yb;yy++)for(let xx=xa;xx<xb;xx++)votes[materialColor(im,labels,box,xx,yy,red)]++;
  const total=votes.reduce((a,b)=>a+b,0);if(votes[0]>total*.6)continue;
  let c=1;for(let v=2;v<4;v++)if(votes[v]>votes[c])c=v;p[y][x]=String(c);
 }return p.map(r=>r.join(''));
}
export function spriteColumns(rows){const out=[];for(let y=0;y<rows.length;y+=16)for(let x=0;x<rows[0].length;x+=8)if(rows.slice(y,y+16).some(r=>/[123]/.test(r.slice(x,x+8))))out.push({x,y});return out;}
function crc(buf){let c=0xffffffff;for(const b of buf){c^=b;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;}return(c^0xffffffff)>>>0;}
export function writePng(file,w,h,pixel){
 const raw=Buffer.alloc((w*4+1)*h);for(let y=0;y<h;y++)for(let x=0;x<w;x++)raw.set(pixel(x,y),(w*4+1)*y+1+x*4);
 const chunk=(name,data)=>{const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(name,4);data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,-4)),b.length-4);return b;};
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=6;
 fs.writeFileSync(file,Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
export function writeGif(file,width,height,palette,frames,scale=4){
 if(palette.length!==8)throw new Error('Study GIF uses exactly eight colors');
 const word=n=>[n&255,n>>8],w=width*scale,h=height*scale;
 const blocks=[Buffer.from('GIF89a'),Buffer.from([...word(w),...word(h),0xf2,0,0,...palette.flat()]),Buffer.from([0x21,0xff,11,...Buffer.from('NETSCAPE2.0'),3,1,0,0,0])];
 for(const frame of frames){
  if(frame.pixels.length!==width*height)throw new Error('Incorrect GIF frame dimensions');
  const codes=[];for(let p=0;p<w*h;p++){
   if(p%6===0)codes.push(8);
   const c=frame.pixels[Math.floor(Math.floor(p/w)/scale)*width+Math.floor((p%w)/scale)];
   if(c>7)throw new Error('GIF palette index exceeds seven');codes.push(c);
  }codes.push(9);
  const packed=Buffer.alloc(Math.ceil(codes.length/2));codes.forEach((c,i)=>packed[i>>1]|=c<<((i&1)*4));
  blocks.push(Buffer.from([0x21,0xf9,4,4,...word(frame.delay),0,0,0x2c,0,0,0,0,...word(w),...word(h),0,3]));
  for(let p=0;p<packed.length;p+=255){const b=packed.subarray(p,p+255);blocks.push(Buffer.from([b.length]),b);}blocks.push(Buffer.from([0]));
 }blocks.push(Buffer.from([0x3b]));fs.writeFileSync(file,Buffer.concat(blocks));
}
