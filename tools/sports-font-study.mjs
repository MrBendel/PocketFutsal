// Original, hand-pixelled sports lettering. Run: node tools/sports-font-study.mjs
import fs from 'node:fs';
import {decodePng} from './assets.mjs';
import {encodePng} from '../harness/node.mjs';

// Six-column faces: clipped corners, broad stems, open counters. No traced glyphs.
const faces={
A:'011110/111111/110011/110011/111111/111111/110011/110011/110011',
B:'111110/111111/110011/110011/111110/110011/110011/111111/111110',
C:'011111/111111/110000/110000/110000/110000/110000/111111/011111',
D:'111110/111111/110011/110011/110011/110011/110011/111111/111110',
E:'111111/111111/110000/110000/111110/110000/110000/111111/111111',
F:'111111/111111/110000/110000/111110/110000/110000/110000/110000',
G:'011111/111111/110000/110000/110111/110011/110011/111111/011110',
H:'110011/110011/110011/110011/111111/111111/110011/110011/110011',
I:'1111/1111/0110/0110/0110/0110/0110/1111/1111',
J:'000011/000011/000011/000011/000011/000011/110011/111111/011110',
K:'110011/110011/110110/111100/111000/111100/110110/110011/110011',
L:'110000/110000/110000/110000/110000/110000/110000/111111/111111',
M:'110011/111111/111111/111111/110011/110011/110011/110011/110011',
N:'110011/111011/111011/111111/111111/110111/110111/110011/110011',
O:'011110/111111/110011/110011/110011/110011/110011/111111/011110',
P:'111110/111111/110011/110011/111111/111110/110000/110000/110000',
Q:'011110/111111/110011/110011/110011/110011/110111/111110/011111',
R:'111110/111111/110011/110011/111110/111100/110110/110011/110011',
S:'011111/111111/110000/110000/011110/000011/000011/111111/111110',
T:'111111/111111/001100/001100/001100/001100/001100/001100/001100',
U:'110011/110011/110011/110011/110011/110011/110011/111111/011110',
V:'110011/110011/110011/110011/110011/110011/011110/011110/001100',
W:'110011/110011/110011/110011/110011/111111/111111/111111/010010',
X:'110011/110011/011110/011110/001100/011110/011110/110011/110011',
Y:'110011/110011/110011/011110/001100/001100/001100/001100/001100',
Z:'111111/111111/000011/000110/001100/011000/110000/111111/111111',
0:'011110/111111/110011/110111/111111/111011/110011/111111/011110',
1:'0110/1110/1110/0110/0110/0110/0110/1111/1111',
2:'011110/111111/000011/000011/001110/011000/110000/111111/111111',
3:'111110/111111/000011/000011/011110/000011/000011/111111/111110',
4:'110011/110011/110011/110011/111111/111111/000011/000011/000011',
5:'111111/111111/110000/110000/111110/000011/000011/111111/111110',
6:'011110/111111/110000/110000/111110/110011/110011/111111/011110',
7:'111111/111111/000011/000011/000110/001100/001100/011000/011000',
8:'011110/111111/110011/110011/011110/110011/110011/111111/011110',
9:'011110/111111/110011/110011/011111/000011/000011/111111/011110',
'!':'11/11/11/11/11/11/00/11/11',
'?':'011110/111111/000011/000110/001100/001100/000000/001100/001100',
'-':'0000/0000/0000/1111/1111/0000/0000/0000/0000',
':':'00/11/11/00/00/00/11/11/00',
'.':'00/00/00/00/00/00/00/11/11',
};
const glyphs={};
for(const [ch,source] of Object.entries(faces)){
 const rows=source.split('/'),w=rows[0].length+2,h=16;
 const px=Array.from({length:h},()=>Array(w).fill(0));
 const ink=[];
 for(let y=0;y<12;y++)for(let x=0;x<w-2;x++)if(rows[Math.floor(y*9/12)][x]==='1')ink.push([x+1,y+1]);
 for(const [x,y]of ink)for(let dy=-1;dy<=2;dy++)for(let dx=-1;dx<=1;dx++)if(y+dy>=0&&y+dy<h&&x+dx>=0&&x+dx<w)px[y+dy][x+dx]=3;
 for(const [x,y]of ink)px[y][x]=y<6?1:2;
 glyphs[ch]=px.map(r=>r.join(''));
}
const dir=new URL('../artwork/fonts/sports-v1/',import.meta.url);fs.mkdirSync(dir,{recursive:true});
const palette=[[0,0,0,0],[255,232,152,255],[232,112,64,255],[24,20,32,255]];
fs.writeFileSync(new URL('sports.json',dir),JSON.stringify({name:'Pocket Sports',height:16,gap:0,space:4,palette,glyphs},null,2)+'\n');
const canvas=(w,h,bg=[22,31,48,255])=>({width:w,height:h,rgba:Buffer.from(Array.from({length:w*h},()=>bg).flat())});
function put(c,x,y,color){if(x>=0&&y>=0&&x<c.width&&y<c.height)c.rgba.set(color,(y*c.width+x)*4);}
function width(s){return [...s].reduce((n,ch)=>n+(ch===' '?4:glyphs[ch][0].length),0);}
function draw(c,s,x,y,pal=palette){for(const ch of s){if(ch===' '){x+=4;continue;}const g=glyphs[ch];for(let j=0;j<16;j++)for(let i=0;i<g[j].length;i++){const v=+g[j][i];if(v)put(c,x+i,y+j,pal[v]);}x+=g[0].length;}}
function save(name,c,scale=1){const out=canvas(c.width*scale,c.height*scale);for(let y=0;y<out.height;y++)for(let x=0;x<out.width;x++)put(out,x,y,c.rgba.subarray((Math.floor(y/scale)*c.width+Math.floor(x/scale))*4,(Math.floor(y/scale)*c.width+Math.floor(x/scale))*4+4));fs.writeFileSync(new URL(name,dir),encodePng(Buffer.from([...out.rgba].filter((_,i)=>i%4!==3)),{width:out.width,height:out.height}));}
const sheet=canvas(160,168);
['POCKET SPORTS','ABCDEFGHIJKLM','NOPQRSTUVWXYZ','0123456789 !?','GOAL!','POWER SHOT','COURT CHANGE'].forEach((s,i)=>draw(sheet,s,Math.floor((160-width(s))/2),5+i*23));
save('specimen-1x.png',sheet);save('specimen-4x.png',sheet,4);
const alpha='ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?-:.';
const atlas=canvas(13*8,4*16,[0,0,0,0]);[...alpha].forEach((ch,i)=>draw(atlas,ch,(i%13)*8,Math.floor(i/13)*16));save('glyph-atlas.png',atlas);
// Native-resolution court mockups. These are studies, not emulator captures.
const src=decodePng(fs.readFileSync(new URL('../build/test-output/futsal-object-goals.png',import.meta.url)));
const court=canvas(160,144);for(let y=0;y<144;y++)for(let x=0;x<160;x++){const p=((y*2)*src.width+x*2)*4;put(court,x,y,src.rgba.subarray(p,p+4));}
const cards=canvas(328,296);
[['GOAL!'],['COURT','CHANGE'],['POWER SHOT'],['MATCH POINT']].forEach((lines,i)=>{
 const ox=(i%2)*168,oy=Math.floor(i/2)*152;
 for(let y=0;y<144;y++)for(let x=0;x<160;x++){const p=(y*160+x)*4;put(cards,ox+x,oy+y,court.rgba.subarray(p,p+4));}
 const cy=lines.length===2?53:62;lines.forEach((s,j)=>draw(cards,s,ox+Math.floor((160-width(s))/2),oy+cy+j*16));
});save('court-alerts-3x.png',cards,3);
console.log('Pocket Sports: 41 original 8x16-or-smaller glyphs; native atlas and court studies written.');

