import {test} from 'node:test';
import fs from 'node:fs';
import {assert,startGame,set16,at} from './helpers.mjs';
import {encodePng} from '../harness/node.mjs';
function sequence(move,timer,name){
 const e=startGame().idle(60);
 const put=(n,v)=>e.write8(e.symbols.addr(n),v);
 put('phase',2);put('cpu_idle',1);put('pl_move',move);put('pl_move_timer',timer);
 set16(e,'ball_x',20*16);set16(e,'ball_y',20*16);set16(e,'ball_z',40*16);
 set16(e,'ball_vx',0);set16(e,'ball_vy',0);set16(e,'ball_vz',0);
 const tiles=new Set(),shots=[];
 for(let i=0;i<timer;i++){
  e.idle();const tile=e.read8(0xfe06)&248;tiles.add(tile);
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
