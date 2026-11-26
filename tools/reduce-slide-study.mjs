// Reproducible technical reduction; these files are review art, not runtime assets.
import fs from 'node:fs';
import {decodePng} from './assets.mjs';
import {BALL_FRAMES} from './ball-art.mjs';
import {silhouettes,reducePose,spriteColumns,writePng,writeGif} from './study-raster.mjs';
const dir=new URL('../artwork/players/slide/study-v1/',import.meta.url);
const im=decodePng(fs.readFileSync(new URL('generated-source.png',dir)));
const {labels,frames}=silhouettes(im,4,8);
const ids=['spiky','cap','crop','bandana'];
const phases=['approach','brake','plant','extend','contact','decelerate','kneel','ready'];
const palette=row=>[[0,0,0,0],row===2?[184,120,72,255]:[248,200,136,255],row<2?[40,120,232,255]:[232,64,72,255],[24,32,48,255]];
function trial(w,h,selected){
 const boxes=frames.filter(f=>selected.includes(f.col));
 const scale=Math.min(w/Math.max(...boxes.map(f=>f.right-f.left+1)),(h-1)/Math.max(...boxes.map(f=>f.bottom-f.top+1)));
 const characters=ids.map((id,row)=>({id,palette:palette(row),frames:boxes.filter(f=>f.row===row).map(source=>{
  const rows=reducePose(im,labels,source,w,h,scale,row>=2);
  let leadingBoot={x:0,y:h-1};
  for(let y=Math.floor(h/2);y<h;y++)for(let x=0;x<w;x++)if(rows[y][x]!=='.'&&x>=leadingBoot.x)leadingBoot={x,y};
  return {phase:phases[source.col],rows,leadingBoot,spriteColumns8x16:spriteColumns(rows),source};
 })}));
 return {width:w,height:h,sourceScale:scale,direction:'right',framesPerCharacter:selected.length,characters};
}
const full=trial(32,24,[0,1,2,3,4,5,6,7]),contact=trial(24,16,[3,4]);
for(const [name,data]of [['native-32x24',full],['contact-24x16',contact]]){
 fs.writeFileSync(new URL(`${name}.json`,dir),JSON.stringify(data,null,2)+'\n');
 const count=data.framesPerCharacter;
 writePng(new URL(`${name}.png`,dir),data.width*count,data.height*4,(x,y)=>{
  const c=data.characters[Math.floor(y/data.height)],f=c.frames[Math.floor(x/data.width)];
  return c.palette[Number(f.rows[y%data.height][x%data.width])||0];
 });
 const cols=count===8?4:2,rows=4*count/cols,cw=data.width+4,ch=data.height+4,s=5;
 writePng(new URL(`${name}-preview.png`,dir),cols*cw*s,rows*ch*s,(xx,yy)=>{
  const x=Math.floor(xx/s),y=Math.floor(yy/s),cell=Math.floor(y/ch)*cols+Math.floor(x/cw);
  const c=data.characters[Math.floor(cell/count)],f=c.frames[cell%count],px=x%cw-2,py=y%ch-2;
  const ink=px>=0&&py>=0&&px<data.width&&py<data.height?Number(f.rows[py][px])||0:0;
  return ink?c.palette[ink]:((Math.floor(x/4)+Math.floor(y/4))%2?[56,72,88,255]:[64,80,96,255]);
 });
}
const colors=[[40,56,72],[248,200,136],[184,120,72],[40,120,232],[232,64,72],[24,32,48],[248,248,232],[88,112,128]];
for(const mirror of [false,true]){
 const width=112,height=144,dx=[0,1,5,14,22,26,26,26],delays=[12,10,10,12,20,12,14,28];
 const animation=phases.map((_,phase)=>{
  const pixels=new Uint8Array(width*height);
  const put=(x,y,c)=>{if(mirror)x=width-1-x;if(x>=0&&y>=0&&x<width&&y<height)pixels[y*width+x]=c;};
  full.characters.forEach((c,row)=>{
   const ground=30+row*36,left=24+dx[phase]-16,top=ground-23;
   for(let x=left+10;x<left+22;x++)put(x,ground+1,5);
   c.frames[phase].rows.forEach((line,y)=>[...line].forEach((v,x)=>{if(v!=='.')put(left+x,top+y,v==='1'?(row===2?2:1):v==='2'?(row<2?3:4):5);}));
   const boot=c.frames[4].leadingBoot;
   const ballX=24+dx[4]-16+boot.x+3+(phase>4?(phase-4)*8:0);
   BALL_FRAMES[phase%BALL_FRAMES.length].forEach((line,y)=>[...line].forEach((v,x)=>{if(v!=='.')put(ballX+x,ground-7+y,v==='1'?6:v==='2'?5:7);}));
  });
  return {pixels,delay:delays[phase]};
 });
 writeGif(new URL(`slide-${mirror?'left':'right'}-loop.gif`,dir),width,height,colors,animation,4);
}
console.log(JSON.stringify({source:[im.width,im.height],poses:frames.length,scale:full.sourceScale,spriteSlots:full.characters.map(c=>({id:c.id,slots:c.frames.map(f=>f.spriteColumns8x16.length)}))},null,2));
