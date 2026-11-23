import {test} from 'node:test';
import fs from 'node:fs';
import {assert,bootToTitle,startGame,set16,settle} from './helpers.mjs';
import {BALL_FRAMES,BALL_EXTRA} from '../tools/ball-art.mjs';
import {asciiTile} from '../tools/tile-art.mjs';
import {decodePng} from '../tools/assets.mjs';
function futsal(){const e=bootToTitle();e.tap('DOWN').idle(40);return settle(e.tap('START'),3,'futsal');}
const put=(e,n,v)=>e.write8(e.symbols.addr(n),v);
test('soccer ball frames keep one silhouette, six distinct panels and binary transparency',()=>{
 assert.equal(BALL_FRAMES.length,6);
 const masks=[];
 for(const rows of BALL_FRAMES){
  assert.equal(rows.length,8);for(const row of rows){assert.equal(row.length,8);assert.match(row,/^[.123]+$/);}
  masks.push(rows.join('').replace(/[123]/g,'#'));
 }
 assert.equal(new Set(masks).size,1,'silhouette and center never wobble');
 assert.equal(new Set(BALL_FRAMES.map(f=>f.join(''))).size,6);
 for(const size of [8,12,16]){
  const im=decodePng(fs.readFileSync(new URL(`../artwork/ball/v1/sheet-${size}.png`,import.meta.url)));
  assert.equal(im.width,size*6);assert.equal(im.height,size);
  const colors=new Set();for(let p=0;p<im.rgba.length;p+=4){assert.ok(im.rgba[p+3]===0||im.rgba[p+3]===255);colors.add(im.rgba.subarray(p,p+4).toString('hex'));}
  assert.ok(colors.size<=4,'three opaque colors plus transparency');
 }
});
test('extra spin tiles fit both modes without replacing action or effect tiles',()=>{
 const expected=Buffer.from(BALL_EXTRA.flatMap(t=>asciiTile(t)));
 for(const [e,base]of[[startGame(),20],[futsal(),116]]){
  assert.equal(e.v('gfx_ball_first'),base);
  assert.deepEqual(Buffer.from(e.snapshot().vram.subarray(base*16,base*16+expected.length)),expected);
 }
});
test('ball spin follows travel speed, covers all six frames and freezes at rest',()=>{
 const e=futsal();put(e,'fs_spectator',0);put(e,'fs_owner',255);
 put(e,'gfx_ball_phase',0);set16(e,'gfx_ball_distance',0);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',112*16);set16(e,'fs_ball_z',80*16);
 set16(e,'fs_ball_vx',8);set16(e,'fs_ball_vy',0);set16(e,'fs_ball_vz',0);
 const initial=e.v('gfx_ball_phase');e.idle(7);assert.equal(e.v('gfx_ball_phase'),initial,'slow roll holds each frame');
 e.idle();assert.equal(e.v('gfx_ball_phase'),(initial+1)%6);
 set16(e,'fs_ball_vx',64);const frames=new Set();
 for(let f=0;f<6;f++){e.idle();frames.add(e.v('gfx_ball_phase'));}
 assert.equal(frames.size,6,'fast travel advances the complete cycle');
 set16(e,'fs_ball_vx',128);const fast=e.v('gfx_ball_phase');e.idle();
 assert.equal(e.v('gfx_ball_phase'),(fast+1)%6,'power shots advance one readable pose per frame');
 set16(e,'fs_ball_vx',0);const stopped=e.v('gfx_ball_phase');e.idle(12);
 assert.equal(e.v('gfx_ball_phase'),stopped,'stationary ball does not animate');
});
