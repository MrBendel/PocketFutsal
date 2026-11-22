import {test} from 'node:test';
import fs from 'node:fs';
import {assert,bootToTitle,at,set16,GAME_OVER,settle} from './helpers.mjs';
const PLAY=3;
function startGame(unused,seed=4242) {
 const e=bootToTitle(); e.tap('DOWN').idle(40); settle(e,0,'menu mode');
 e.writeBytes(e.symbols.addr('test_seed'),[seed&255,seed>>8]);
 return settle(e.tap('START'),PLAY,'futsal mode');
}
const put=(e,n,v)=>e.write8(e.symbols.addr(n),v);
test('four AI players pass, shoot, and finish a match',()=>{
 const e=startGame();
 assert.equal(e.v('fs_spectator'),1);
 let frames=0;
 while(e.v('game_state')===PLAY && frames<24000){e.idle(60); frames+=60;}
 console.log({frames,passes:e.v('fs_pass_count','u16'),shots:e.v('fs_shot_count','u16'),completed:e.v('fs_completed_passes','u16'),blue:at(e,'score',0),red:at(e,'score',1)});
 assert.ok(e.v('fs_pass_count','u16')>0); assert.ok(e.v('fs_shot_count','u16')>0);
 settle(e,GAME_OVER,'full time');
 assert.ok(at(e,'score',0)===5 || at(e,'score',1)===5);
 e.shot('futsal-full-time');
});
test('pause freezes simulation and SELECT takes over fixed slot zero',()=>{
 const e=startGame(); e.tap('START'); const tick=e.v('fs_sim_ticks','u16');
 e.idle(60); assert.equal(e.v('fs_sim_ticks','u16'),tick);
 e.tap('START'); e.tap('SELECT'); assert.equal(e.v('fs_spectator'),0);
 const x=at(e,'fs_pl_x',0,'i16'); e.hold('RIGHT',15); assert.ok(at(e,'fs_pl_x',0,'i16')>x);
 e.shot('futsal-play');
});
test('human possession passes and shoots; end walls only score inside goal',()=>{
 const e=startGame(); put(e,'fs_spectator',0); put(e,'fs_owner',0);
 e.tap('B'); assert.equal(e.v('fs_pass_count','u16'),1);
 put(e,'fs_owner',0); e.tap('A'); assert.equal(e.v('fs_shot_count','u16'),1);
 put(e,'fs_owner',255); set16(e,'fs_ball_x',20*16); set16(e,'fs_ball_y',2); set16(e,'fs_ball_vy',-76);
 e.idle(); assert.equal(at(e,'score',0),0); assert.ok(e.v('fs_ball_vy','i16')>0);
 set16(e,'fs_ball_x',80*16); set16(e,'fs_ball_y',2); set16(e,'fs_ball_vy',-76);
 e.idle(); assert.equal(at(e,'score',0),1); assert.ok(e.v('fs_goal_timer')>0);
});
test('slide steals possession and burst has a cooldown; several seeds finish',()=>{
 const e=startGame(); put(e,'fs_spectator',0); put(e,'fs_owner',2);
 const x=at(e,'fs_pl_x',0,'i16'), y=at(e,'fs_pl_y',0,'i16');
 e.writeBytes(e.symbols.addr('fs_pl_x')+4,[x&255,x>>8]);
 e.writeBytes(e.symbols.addr('fs_pl_y')+4,[(y-16*16)&255,(y-16*16)>>8]);
 set16(e,'fs_ball_x',x); set16(e,'fs_ball_y',y-9*16);
 e.write8(e.symbols.addr('fs_think')+2,40);
 e.writeBytes(e.symbols.addr('fs_ai_tx')+4,[x&255,x>>8]);
 e.writeBytes(e.symbols.addr('fs_ai_ty')+4,[(y-16*16)&255,(y-16*16)>>8]);
 e.tap('A'); assert.equal(e.v('fs_tackle_count','u16'),0,'crouch is not instant possession');
 e.until(e=>e.v('fs_tackle_count','u16')>0,20,'sliding foot contacting the ball');
 put(e,'fs_owner',255); put(e,'fs_cooldown',0); e.tap('B'); assert.ok(e.v('fs_burst')>0); assert.ok(e.v('fs_cooldown')>0);
 for(const seed of [7,123]) {
  const sim=startGame(undefined,seed);
  sim.until(e=>e.v('game_state')===GAME_OVER,18000,'AI match completion');
 }
});
test('menu exposes both modes and futsal returns to the menu',()=>{
 const e=bootToTitle();
 assert.equal(e.v('game_mode'),0);
 e.tap('DOWN').idle(40); assert.equal(e.v('game_mode'),1);
 e.shot('menu-futsal-watch');
 e.tap('SELECT').idle(15); assert.equal(e.v('fs_spectator'),0);
 e.shot('menu-futsal-play');
 settle(e.tap('START'),PLAY,'human futsal');
 assert.equal(e.v('fs_spectator'),0);
 put(e,'score',5); put(e,'fs_goal_timer',1);
 settle(e.idle(2),GAME_OVER,'futsal full time');
 e.idle(35); settle(e.tap('START'),0,'return to menu');
 assert.equal(e.v('game_mode'),1);
 e.tap('UP').idle(40); assert.equal(e.v('game_mode'),0);
 settle(e.tap('START'),1,'futnet after futsal');
 assert.equal(e.v('phase'),0);
});

test('new roster loads all four pixel sets into VRAM and uses each character palette',()=>{
 const e=startGame();
 const ids=['spiky','cap','crop','bandana'];
 const expected=Buffer.concat(ids.map(id=>fs.readFileSync(new URL(`../artwork/players/roster/${id}/player.2bpp`,import.meta.url))));
 assert.deepEqual(Buffer.from(e.snapshot().vram.subarray(0x2000,0x3000)),expected);
 put(e,'fs_goal_timer',80);
 e.writeBytes(e.symbols.addr('fs_action'),[0,0,0,0]);
 e.writeBytes(e.symbols.addr('fs_moving'),[0,0,0,0]);
 for(let i=0;i<4;i++) {
  const x=(i%2?116:44)*16,y=(i<2?174:64)*16;
  e.writeBytes(e.symbols.addr('fs_pl_x')+i*2,[x&255,x>>8]);
  e.writeBytes(e.symbols.addr('fs_pl_y')+i*2,[y&255,y>>8]);
 }
 e.idle(3);
 const palettes=[];for(let slot=0;slot<4;slot++)palettes.push(e.read8(0xfe00+(1+slot*4)*4+3)&7);
 assert.deepEqual([...palettes].sort(),[0,1,2,3]);
 e.shot('futsal-new-roster');
});
test('power shots leave a visible fading trail; player shadows stay on court',()=>{
 const e=startGame(); put(e,'fs_spectator',0); put(e,'fs_owner',0);
 e.tap('A').idle(6);
 e.tap('START').idle(3);
 assert.ok(e.v('fs_power_trail')>0);
 assert.equal(e.read8(0xfe03+e.v('fs_ball_oam')*4)&7,6,'bright ball palette');
 assert.ok(e.read8(0xfe00+18*4)>0,'first trail sprite visible');
 e.shot('futsal-power-trail');
 for(let i=0;i<4;i++) {
  const py=at(e,'fs_pl_y',i,'i16')>>4;
  const ground=e.read8(e.symbols.addr('fs_projection_y')+py)-e.v('fs_cam_y');
  assert.equal(e.read8(0xfe00+(22+i*2)*4),ground+12,'grounded player shadow');
 }
 e.tap('START');
 put(e,'fs_owner',0); e.idle(2);
 assert.equal(e.v('fs_power_trail'),0);
 assert.equal(e.read8(0xfe00+18*4),0,'trail hidden on possession');
});

test('ball is occluded by nearer players and draws over farther players',()=>{
 const e=startGame(); put(e,'fs_goal_timer',100);
 e.writeBytes(e.symbols.addr('fs_action'),[0,0,0,0]);
 e.writeBytes(e.symbols.addr('fs_tackling'),[0,0,0,0]);
 const ys=[190,150,70,30];
 for(let i=0;i<4;i++)e.writeBytes(e.symbols.addr('fs_pl_y')+i*2,[(ys[i]*16)&255,(ys[i]*16)>>8]);
 for(const [y,expected] of [[210,0],[170,4],[110,8],[50,12],[10,16]]){
  set16(e,'fs_ball_y',y*16);e.idle(3);
  assert.equal(e.v('fs_ball_oam'),expected);
  assert.equal(e.read8(0xfe02+expected*4)&0xfe,(e.v('fs_sim_ticks')&8)?2:0,'ball occupies sorted OAM slot');
 }
});
function setPlayer(e,i,x,y){
 for(const [name,value]of[['fs_pl_x',x*16],['fs_pl_y',y*16]])e.writeBytes(e.symbols.addr(name)+i*2,[value&255,value>>8]);
 e.write8(e.symbols.addr('fs_vx')+i,0);e.write8(e.symbols.addr('fs_vy')+i,0);
}
function humanSpace(){
 const e=startGame();put(e,'fs_spectator',0);put(e,'fs_owner',0);
 setPlayer(e,0,80,180);setPlayer(e,1,112,96);setPlayer(e,2,20,20);setPlayer(e,3,30,30);
 e.idle(2);return e;
}
test('movement accelerates, carries momentum through a cut, and brakes to rest',()=>{
 const e=humanSpace();
 e.hold('RIGHT');const initial=at(e,'fs_vx',0,'i8');
 e.hold('RIGHT',8);const fast=at(e,'fs_vx',0,'i8');
 assert.ok(initial>0&&initial<fast,'speed ramps up');
 e.hold('LEFT');assert.ok(at(e,'fs_vx',0,'i8')>0,'reversal must shed existing momentum');
 const x=at(e,'fs_pl_x',0,'i16');e.idle();assert.ok(at(e,'fs_pl_x',0,'i16')>x,'release does not stop instantly');
 e.idle(12);assert.equal(at(e,'fs_vx',0,'i8'),0);
});
test('a lofted pass clears feet, has a ground shadow, and reaches its teammate',()=>{
 const e=humanSpace();e.tap('B').idle(12);
 assert.equal(e.v('fs_owner'),255,'airborne pass remains loose');
 assert.ok(e.v('fs_ball_z','i16')>8*16,'pass rises above tackling height');
 e.tap('START').idle(3);
 const ball=e.read8(0xfe00+e.v('fs_ball_oam')*4),shadow=e.read8(0xfe00+30*4);
 assert.ok(shadow-ball>8,'visible separation from court shadow');
 e.shot('futsal-lofted-pass');e.tap('START');
 e.until(e=>e.v('fs_owner')===1,100,'teammate receiving the pass');
 assert.ok(e.v('fs_completed_passes','u16')>=1);
 assert.ok(e.v('fs_ball_z','i16')<=8*16);
});
test('airborne balls cannot be collected at ground level or score above the crossbar',()=>{
 const e=humanSpace();put(e,'fs_owner',255);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',180*16);set16(e,'fs_ball_z',20*16);
 set16(e,'fs_ball_vx',0);set16(e,'fs_ball_vy',0);set16(e,'fs_ball_vz',0);
 e.idle();assert.equal(e.v('fs_owner'),255);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',224*16-2);set16(e,'fs_ball_z',30*16);set16(e,'fs_ball_vy',76);
 e.idle(2);assert.equal(at(e,'score',1),0);assert.ok(e.v('fs_ball_vy','i16')<0);
});
test('a slide commits to its launch direction instead of homing through a juke',()=>{
 const e=humanSpace();put(e,'fs_owner',2);setPlayer(e,2,100,180);e.idle(2);
 e.tap('A').hold('LEFT',6);
 assert.ok(at(e,'fs_vx',0,'i8')>0,'rightward slide stays committed despite left input');
});
test('2v2 momentum, aerial ball and AI remain within the frame budget',()=>{
 const e=startGame();let worst=0;
 for(let f=0;f<1000;f++){e.idle();if(e.v('game_state')===PLAY&&!e.v('in_transition'))worst=Math.max(worst,e.lastFrameCycles);}
 assert.ok(worst<65664,`worst 2v2 frame ${worst} cycles`);
});

test('a forward run can receive a called return pass in stride',()=>{
 const e=humanSpace();setPlayer(e,0,48,160);setPlayer(e,1,112,176);e.idle(2);
 e.hold('UP',12).hold('UP+B').hold('UP',24);
 e.until(e=>e.v('fs_owner')===1,80,'teammate receiving first pass');
 e.hold('UP+B').hold('UP',1);
 assert.equal(e.v('fs_receiver'),0,"B calls the teammate's return pass");
 for(let f=0;f<60&&e.v('fs_owner')!==0;f++)e.hold('UP');
 assert.equal(e.v('fs_owner'),0,'moving human receives the led return');
 assert.ok(e.v('fs_completed_passes','u16')>=2);
});

test('directional B sends the receiver into space instead of at their old position',()=>{
 const e=humanSpace();setPlayer(e,1,96,112);
 const x=at(e,'fs_pl_x',1,'i16'),y=at(e,'fs_pl_y',1,'i16');
 e.hold('LEFT+B');
 assert.equal(e.v('fs_target_x','i16'),x-24*16);
 assert.equal(e.v('fs_target_y','i16'),y);
 assert.ok(at(e,'fs_vx',1,'i8')<0,'receiver reacts immediately to the through pass');
 e.idle();e.until(e=>e.v('fs_owner')===1,90,'teammate meeting the directional pass');
 assert.ok(at(e,'fs_pl_x',1,'i16')<x-10*16,'receiver ran into the requested space');
 assert.equal(e.v('fs_completed_passes','u16'),1);
});

test('a return called in flight is played on receipt and leads the human runner',()=>{
 const e=humanSpace();setPlayer(e,0,48,160);setPlayer(e,1,112,176);e.idle(2);
 e.tap('B').hold('UP',8).hold('UP+B');
 assert.ok(e.v('fs_return_call')>0,'return request is buffered during flight');
 assert.equal(e.v('fs_burst'),0,'calling a return does not spend the defensive burst');
 for(let f=0;f<90&&e.v('fs_pass_count','u16')<2;f++)e.hold('UP');
 assert.equal(e.v('fs_pass_count','u16'),2,'teammate immediately plays the requested return');
 assert.equal(e.v('fs_receiver'),0);
 assert.equal(e.v('fs_return_call'),0,'request is consumed once');
 for(let f=0;f<80&&e.v('fs_owner')!==0;f++)e.hold('UP');
 assert.equal(e.v('fs_owner'),0,'runner collects the return');
 assert.equal(e.v('fs_completed_passes','u16'),2);
});

test('a buffered return is cancelled when an opponent takes possession',()=>{
 const e=humanSpace();e.tap('B').idle(3).tap('B');
 assert.ok(e.v('fs_return_call')>0);
 put(e,'fs_owner',2);e.idle();
 assert.equal(e.v('fs_return_call'),0);
});

test('an unfulfilled return request expires rather than triggering a later pass',()=>{
 const e=humanSpace();e.tap('B').idle(3).tap('B');
 set16(e,'fs_ball_z',200*16);set16(e,'fs_ball_vz',0);
 set16(e,'fs_ball_vx',0);set16(e,'fs_ball_vy',0);
 e.idle(49);
 assert.equal(e.v('fs_return_call'),0);
 assert.equal(e.v('fs_pass_count','u16'),1);
});
