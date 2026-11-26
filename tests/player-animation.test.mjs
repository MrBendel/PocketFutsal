import {test} from 'node:test';
import fs from 'node:fs';
import {assert,startGame,set16,at} from './helpers.mjs';
import {encodePng} from '../harness/node.mjs';
import {CHARACTERS,character,fullTiles,sideTiles,sideAtlas} from '../tools/player-art.mjs';
import {reactionFrames,reactionHeads,REACTION_TILES} from '../tools/reaction-art.mjs';

test('each roster character has distinct native win and lose cycles with their original head',()=>{
 assert.equal(REACTION_TILES.length,192,'four characters, two outcomes, three eight-tile frames');
 for(const id of CHARACTERS){
  const r=reactionFrames(id),head=character(id).basic[0];
  for(const outcome of ['win','lose']){
   assert.equal(r[outcome].length,3);assert.equal(new Set(r[outcome].map(p=>p.join(''))).size,3);
   for(const pose of r[outcome]){assert.equal(pose.length,24);for(const row of pose){assert.equal(row.length,16);assert.match(row,/^[.123]+$/);}}
  }
  for(const pose of r.win)for(let y=0;y<13;y++)for(let x=0;x<16;x++)if(head[y][x]!=='.')assert.equal(pose[y][x],head[y][x],'winning face stays intact');
  const heads=reactionHeads(id),count=rows=>[1,2,3].map(c=>[...rows.join('')].filter(v=>v===String(c)).length);
  assert.equal(new Set(heads.map(p=>p.join(''))).size,3,'front and two tilted three-quarter views');
  for(let f=0;f<3;f++){
   const original=character(id).basic[f?3:0].slice(0,13);
   assert.deepEqual(count(heads[f]),count(original),'tilt retains every approved head pixel');
   for(let y=0;y<16;y++)for(let x=0;x<16;x++)if(heads[f][y][x]!=='.')assert.equal(r.lose[f][y][x],heads[f][y][x]);
   assert.deepEqual(r.lose[f].slice(16),r.lose[0].slice(16),'crouched body stays still while the head turns');
  }
 }
});

test('side cycles preserve each approved profile head and fit the sprite budget',()=>{
 assert.equal(sideAtlas().length,28,'packed roster fits outside the court tile range');
 for(const id of CHARACTERS){
  const c=character(id);
  assert.equal(fullTiles(id).length,248);
  assert.equal(sideTiles(id).length,24);
  assert.equal(new Set(c.side.map(p=>p.join(''))).size,3,'three distinct poses');
  for(const pose of c.side){
   assert.equal(pose.length,24);
   for(const row of pose){assert.equal(row.length,16);assert.match(row,/^[.123]+$/);}
   assert.deepEqual(pose.slice(0,13),c.basic[3].slice(0,13),'head pixels remain intact');
   assert.deepEqual(pose.slice(0,16),c.side[0].slice(0,16),'upper tiles are shared across the cycle');
   assert.deepEqual(pose.slice(16),character('spiky').side[c.side.indexOf(pose)].slice(16),'lower tiles are shared across characters');
   assert.deepEqual(pose.slice(16),character('spiky').basic[c.side.indexOf(pose)].slice(16),'turned frames keep the approved running gait');
   assert.ok(pose[12].includes('1')&&pose[13].includes('1'),'jaw and neck connect');
  }
 }
});
function sequence(move,timer,name){
 const e=startGame().idle(60);
 const put=(n,v)=>e.write8(e.symbols.addr(n),v);
 put('phase',2);put('cpu_idle',1);put('pl_move',move);put('pl_move_timer',timer);
 set16(e,'ball_x',20*16);set16(e,'ball_y',20*16);set16(e,'ball_z',40*16);
 set16(e,'ball_vx',0);set16(e,'ball_vy',0);set16(e,'ball_vz',0);
 const tiles=new Set(),shots=[];
 for(let i=0;i<timer;i++){
  e.idle();const tile=e.read8(0xfe06)&248;tiles.add(tile+':'+at(e,'slide_shown',0));  // (the slide's frames stream into the same tiles: slide_shown tells them apart)
  if(i===1||i===Math.floor(timer/3)||i===timer-7||i===timer-3)shots.push(e.frameBuffer().slice());
 }
 assert.ok(tiles.size>=4,`${name} should show launch, action, landing and recovery`);
 assert.equal(at(e,'pl_move_timer',0),0,'visual sequence preserves move duration');
 const w=160*shots.length,rgb=new Uint8Array(w*144*3);
 for(let f=0;f<shots.length;f++)for(let y=0;y<144;y++)rgb.set(shots[f].subarray(y*480,(y+1)*480),(y*w+f*160)*3);
 fs.mkdirSync('build/test-output',{recursive:true});fs.writeFileSync(`build/test-output/${name}-sequence.png`,encodePng(rgb,{width:w,height:144,scale:2}));
}
test('bicycle has readable launch, airborne and recovery frames',()=>sequence(4,24,'bicycle'));
test('slide has crouch, extension and get-up frames',()=>sequence(5,64,'slide'));
test('futnet ball changes depth between the two players',()=>{
 const e=startGame();
 const put=(name,v)=>e.write8(e.symbols.addr(name),v);
 put('goal_timer',100);put('cpu_idle',1);put('phase',2);
 set16(e,'pl_y',190*16);e.writeBytes(e.symbols.addr('pl_y')+2,[(50*16)&255,(50*16)>>8]);
 set16(e,'ball_x',20*16);set16(e,'ball_vx',0);set16(e,'ball_vy',0);
 for(const [y,expected]of[[210,0],[120,4],[20,8]]){
  set16(e,'ball_y',y*16);e.idle(3);
  assert.equal(e.v('ball_oam'),expected);
 }
});

test('futnet horizontal movement selects side art in both directions',()=>{
 const e=startGame().idle(60),put=(n,v)=>e.write8(e.symbols.addr(n),v);
 put('phase',2);put('cpu_idle',1);
 set16(e,'ball_x',20*16);set16(e,'ball_y',20*16);set16(e,'ball_z',40*16);
 set16(e,'ball_vx',0);set16(e,'ball_vy',0);set16(e,'ball_vz',0);
 e.hold('RIGHT',10).tap('START').idle(3);
 let n=e.v('ball_oam')?0:1;
 assert.equal(e.v('pl_side'),1);
 assert.ok(e.read8(0xfe02+n*4)>=224&&e.read8(0xfe02+n*4)<248);
 assert.equal(e.read8(0xfe03+n*4)&32,0);
 e.shot('futnet-side-right');
 e.tap('START').hold('LEFT',25).tap('START').idle(3);
 n=e.v('ball_oam')?0:1;
 assert.equal(e.v('pl_side'),1);
 assert.ok(e.read8(0xfe02+n*4)>=224&&e.read8(0xfe02+n*4)<248);
 assert.equal(e.read8(0xfe03+n*4)&32,32);
});
