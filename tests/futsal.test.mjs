import {test} from 'node:test';
import fs from 'node:fs';
import {assert,bootToTitle,at,set16,GAME_OVER,settle} from './helpers.mjs';
import {sideAtlas} from '../tools/player-art.mjs';
import {asciiTile,FUTSAL_PITCH} from '../tools/tile-art.mjs';
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
 let frames=0,worst=0;
 while(e.v('game_state')===PLAY && frames<24000){
  e.idle();frames++;
  if(e.v('game_state')===PLAY&&!e.v('in_transition'))worst=Math.max(worst,e.lastFrameCycles);
 }
 console.log({frames,passes:e.v('fs_pass_count','u16'),shots:e.v('fs_shot_count','u16'),completed:e.v('fs_completed_passes','u16'),blue:at(e,'score',0),red:at(e,'score',1)});
 assert.ok(e.v('fs_pass_count','u16')>0); assert.ok(e.v('fs_shot_count','u16')>0);
 assert.ok(worst<65664,`full-match frame budget: ${worst} cycles`);
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
  let worst=0;
  for(let f=0;f<18000&&sim.v('game_state')===PLAY;f++){
   sim.idle();
   if(sim.v('game_state')===PLAY&&!sim.v('in_transition'))worst=Math.max(worst,sim.lastFrameCycles);
  }
  settle(sim,GAME_OVER,'AI match completion');
  assert.ok(worst<65664,`seed ${seed} full-match budget: ${worst} cycles`);
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
  const phase=e.v('gfx_ball_phase'),tile=phase<2?phase*2:116+(phase-2)*2;
  assert.equal(e.read8(0xfe02+expected*4)&0xfe,tile,'ball occupies sorted OAM slot');
 }
});
function setPlayer(e,i,x,y){
 for(const [name,value]of[['fs_pl_x',x*16],['fs_pl_y',y*16]])e.writeBytes(e.symbols.addr(name)+i*2,[value&255,value>>8]);
 e.write8(e.symbols.addr('fs_vx')+i,0);e.write8(e.symbols.addr('fs_vy')+i,0);
}
function humanSpace(){
 const e=startGame();put(e,'fs_spectator',0);put(e,'fs_owner',0);
 setPlayer(e,0,80,180);setPlayer(e,1,112,96);setPlayer(e,2,20,20);setPlayer(e,3,30,30);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',173*16);
 set16(e,'fs_ball_z',0);set16(e,'fs_ball_vz',0);set16(e,'fs_ball_vx',0);set16(e,'fs_ball_vy',0);
 // Keep defenders away from the lane when testing delivery mechanics.
 for(const i of [2,3]){
  e.write8(e.symbols.addr('fs_think')+i,200);
  for(const n of ['fs_ai_tx','fs_ai_ty']){
   const v=(i===2?20:30)*16;e.writeBytes(e.symbols.addr(n)+i*2,[v&255,v>>8]);
  }
 }
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
 const e=humanSpace();setPlayer(e,1,128,48);e.tap('B').idle(12);
 assert.equal(e.v('fs_owner'),255,'airborne pass remains loose');
 assert.ok(e.v('fs_ball_z','i16')>2*16,'pass rises above the court');
 e.tap('START').idle(3);
 const ball=e.read8(0xfe00+e.v('fs_ball_oam')*4),shadow=e.read8(0xfe00+30*4);
 assert.ok(shadow-ball>3,'visible separation from court shadow');
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
 const e=humanSpace();put(e,'fs_owner',2);setPlayer(e,2,100,180);
 set16(e,'fs_ball_x',93*16);set16(e,'fs_ball_y',180*16);e.idle(2);
 e.tap('A').hold('LEFT',6);
 assert.ok(at(e,'fs_vx',0,'i8')>0,'rightward slide stays committed despite left input');
});
test('2v2 momentum, aerial ball and AI remain within the frame budget',()=>{
 const e=startGame();let worst=0;
 for(let f=0;f<1000;f++){e.idle();if(e.v('game_state')===PLAY&&!e.v('in_transition'))worst=Math.max(worst,e.lastFrameCycles);}
 assert.ok(worst<65664,`worst 2v2 frame ${worst} cycles`);
});

test('a forward run can receive a called return pass in stride',()=>{
 const e=humanSpace();setPlayer(e,0,48,160);setPlayer(e,1,112,176);
 set16(e,'fs_ball_x',48*16);set16(e,'fs_ball_y',153*16);e.idle(2);
 e.hold('UP',12).hold('UP+B').hold('UP',24);
 e.until(e=>e.v('fs_owner')===1,80,'teammate receiving first pass');
 e.hold('UP+B').hold('UP',1);
 for(let f=0;f<20&&e.v('fs_receiver')!==0;f++)e.hold('UP');
 assert.equal(e.v('fs_receiver'),0,"B calls the teammate's return at kicking reach");
 for(let f=0;f<60&&e.v('fs_owner')!==0;f++)e.hold('UP');
 assert.equal(e.v('fs_owner'),0,'moving human receives the led return');
 assert.ok(e.v('fs_completed_passes','u16')>=2);
});

test('directional B sends the receiver into space instead of at their old position',()=>{
 const e=humanSpace();setPlayer(e,1,96,112);
 const x=at(e,'fs_pl_x',1,'i16'),y=at(e,'fs_pl_y',1,'i16');
 e.hold('LEFT+B');
 const rx=at(e,'fs_pl_x',1,'i16'),ry=at(e,'fs_pl_y',1,'i16');e.idle();
 assert.equal(e.v('fs_target_x','i16'),rx-24*16);
 assert.equal(e.v('fs_target_y','i16'),ry);
 for(let f=0;f<6&&at(e,'fs_vx',1,'i8')>=0;f++)e.idle();
 assert.ok(at(e,'fs_vx',1,'i8')<0,'receiver brakes and turns toward the through pass');
 e.idle();e.until(e=>e.v('fs_owner')===1,90,'teammate meeting the directional pass');
 assert.ok(at(e,'fs_pl_x',1,'i16')<x-10*16,'receiver ran into the requested space');
 assert.equal(e.v('fs_completed_passes','u16'),1);
});

test('a return called in flight is played on receipt and leads the human runner',()=>{
 const e=humanSpace();setPlayer(e,0,48,160);setPlayer(e,1,112,176);
 set16(e,'fs_ball_x',48*16);set16(e,'fs_ball_y',153*16);e.idle(2);
 e.tap('B').hold('UP',8).hold('UP+B').hold('UP');
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

function launchPassTo(x,y){
 const e=humanSpace();setPlayer(e,1,x,y);
 e.write8(e.symbols.addr('fs_think')+1,40);
 for(const [n,v]of[['fs_ai_tx',x*16],['fs_ai_ty',y*16]])e.writeBytes(e.symbols.addr(n)+2,[v&255,v>>8]);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',180*16);
 e.tap('B');return e;
}

test('futsal launches precise non-eight-way vectors at a consistent passing speed',()=>{
 const velocities=[];
 for(const x of [101,118,132]){
  const e=launchPassTo(x,96),vx=e.v('fs_ball_vx','i16'),vy=e.v('fs_ball_vy','i16');
  assert.ok(vx>0&&vy<0&&vx!==-vy,'pass has its own slope rather than a diagonal');
  assert.equal(vx-vy,64,'distance changes flight time rather than passing speed');
  assert.ok(Math.abs(vx-((x-80)*64/(x-80+84)))<1,'angle follows the actual target vector');
  velocities.push(vx);
 }
 assert.equal(new Set(velocities).size,3,'nearby targets produce distinct angles');
});

test('long passes take longer and arc higher than short exchanges',()=>{
 const short=launchPassTo(100,156),long=launchPassTo(120,70);
 assert.ok(long.v('fs_flight_left')>short.v('fs_flight_left')*2);
 let shortPeak=0,longPeak=0;
 for(let f=0;f<80;f++){
  shortPeak=Math.max(shortPeak,short.v('fs_ball_z','i16'));
  longPeak=Math.max(longPeak,long.v('fs_ball_z','i16'));
  short.idle();long.idle();
 }
 assert.ok(longPeak>shortPeak*2,'height is derived from travel time');
});

test('futsal bounces shed horizontal and vertical energy like futnet',()=>{
 const e=humanSpace();put(e,'fs_owner',255);put(e,'fs_is_shot',1);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',80*16);
 set16(e,'fs_ball_z',1);set16(e,'fs_ball_vz',-20);
 set16(e,'fs_ball_vx',40);set16(e,'fs_ball_vy',20);
 e.idle();
 assert.equal(e.v('fs_ball_z','i16'),0);
 assert.ok(e.v('fs_ball_vx','i16')>=29&&e.v('fs_ball_vx','i16')<=30);
 assert.ok(e.v('fs_ball_vy','i16')>=14&&e.v('fs_ball_vy','i16')<=15);
 assert.ok(e.v('fs_ball_vz','i16')>0&&e.v('fs_ball_vz','i16')<20);
});

test('running takes discrete touches with a changing gap between player and ball',()=>{
 const e=humanSpace(),gaps=[];
 setPlayer(e,0,30,180);set16(e,'fs_ball_x',30*16);set16(e,'fs_ball_y',180*16);
 for(let f=0;f<55;f++){
  e.hold('RIGHT');gaps.push(e.v('fs_ball_x','i16')-at(e,'fs_pl_x',0,'i16'));
  assert.equal(e.v('fs_owner'),0,'straight dribble stays under control');
 }
 assert.ok(e.v('fs_dribble_touches','u16')>=2,'player repeatedly touches the rolling ball');
 assert.ok(Math.max(...gaps)-Math.min(...gaps)>4*16,'ball is not held at a fixed offset');
});

test('a dribbled ball keeps rolling when the player stops',()=>{
 const e=humanSpace();put(e,'fs_touch_timer',10);
 set16(e,'fs_ball_x',88*16);set16(e,'fs_ball_y',180*16);set16(e,'fs_ball_vx',24);
 const x=at(e,'fs_pl_x',0,'i16');e.idle();
 assert.equal(at(e,'fs_pl_x',0,'i16'),x);
 assert.ok(e.v('fs_ball_x','i16')>88*16,'ball keeps its own velocity');
});

test('a hard cut can lose control without turning or teleporting the ball',()=>{
 const e=humanSpace();put(e,'fs_touch_timer',10);
 set16(e,'fs_ball_x',100*16);set16(e,'fs_ball_y',180*16);set16(e,'fs_ball_vx',40);
 e.write8(e.symbols.addr('fs_vx'),(-22)&255);
 e.hold('LEFT',3);
 assert.equal(e.v('fs_owner'),255,'ball outside control reach becomes loose');
 assert.ok(e.v('fs_ball_x','i16')>100*16,'old ball momentum survives the cut');
 assert.ok(at(e,'fs_pl_x',0,'i16')<80*16);
});

function humanActor(e){
 for(let n=0;n<17;n++)if((e.read8(0xfe03+n*4)&7)===0)return n;
 assert.fail('blue player sprite missing');
}
test('all four side cycles are resident in VRAM without replacing front/back or slides',()=>{
 const e=startGame();
 const expected=Buffer.from(sideAtlas().flatMap(t=>asciiTile(t))),vram=e.snapshot().vram;
 assert.deepEqual(Buffer.from(vram.subarray(214*16,242*16)),expected);
 for(let n=0;n<FUTSAL_PITCH.tiles.length;n++){
  const tile=80+n,offset=0x1000+(tile<128?tile:tile-256)*16;
  assert.deepEqual(Buffer.from(vram.subarray(offset,offset+16)),Buffer.from(asciiTile(FUTSAL_PITCH.tiles[n])),'court tile survives sprite uploads');
 }
});

test('horizontal running uses mirrored side strides, keeps side idle and yields to actions',()=>{
 const e=humanSpace(),poses=new Set();
 e.hold('RIGHT',8);
 for(let f=0;f<16;f++){
  e.hold('RIGHT');
  const n=humanActor(e),tile=e.read8(0xfe02+n*4),pal=e.read8(0xfe03+n*4);
  assert.equal(pal&8,0,'side art uses its bank-zero allocation');
  assert.equal(pal&32,0,'right-facing art is not mirrored');
  assert.equal(tile,214,'spiky profile head is shared across strides');
  poses.add(e.read8(0xfe02+(n+1)*4));
 }
 assert.equal(poses.size,3,'two strides and the passing-foot pose');
 e.idle(12).tap('START').idle(3);
 assert.equal(e.read8(0xfe02+humanActor(e)*4),214,'side view is retained after braking');
 assert.equal(e.read8(0xfe02+(humanActor(e)+1)*4),230,'idle legs after braking');
 e.tap('START').hold('LEFT',14).tap('START').idle(3);
 assert.equal(e.read8(0xfe03+humanActor(e)*4)&32,32,'leftward motion mirrors the side cycle');
 e.shot('futsal-side-left');
 e.tap('START');put(e,'fs_owner',0);
 set16(e,'fs_ball_x',at(e,'fs_pl_x',0,'i16'));set16(e,'fs_ball_y',at(e,'fs_pl_y',0,'i16'));
 set16(e,'fs_ball_vx',0);set16(e,'fs_ball_vy',0);
 e.tap('A').tap('START').idle(3);
 assert.equal(e.read8(0xfe03+humanActor(e)*4)&8,8,'kick keeps the existing action art');
 e.tap('START').hold('UP',24).tap('START').idle(3);
 assert.equal(e.v('fs_side'),0,'vertical movement returns to front/back');
 assert.equal(e.read8(0xfe03+humanActor(e)*4)&8,8);
});

test('a shot waits for kicking reach while the player closes on the rolling ball',()=>{
 const e=humanSpace();put(e,'fs_touch_timer',10);
 set16(e,'fs_ball_x',98*16);set16(e,'fs_ball_y',180*16);
 e.hold('RIGHT+A');
 assert.equal(e.v('fs_shot_count','u16'),0,'no remote kick from beyond foot reach');
 assert.equal(e.v('fs_pending_kick'),2,'shot input is buffered');
 for(let f=0;f<20&&!e.v('fs_shot_count','u16');f++)e.hold('RIGHT');
 assert.equal(e.v('fs_shot_count','u16'),1,'shot releases when the player reaches the ball');
});

test('an opponent can intercept the exposed ball without overlapping its dribbler',()=>{
 const e=humanSpace();setPlayer(e,2,94,180);
 e.writeBytes(e.symbols.addr('fs_ai_tx')+4,[(94*16)&255,(94*16)>>8]);
 e.writeBytes(e.symbols.addr('fs_ai_ty')+4,[(180*16)&255,(180*16)>>8]);
 set16(e,'fs_ball_x',92*16);set16(e,'fs_ball_y',180*16);
 e.idle(2);
 assert.equal(e.v('fs_owner'),2,'defender plays the ball rather than the carrier body');
 assert.equal(at(e,'fs_pl_x',0,'i16'),80*16);
 assert.ok(e.v('fs_ball_x','i16')>=92*16,'interception preserves ball position');
});

test('tap B delivers quickly; holding B charges a much higher lob on release',()=>{
 const quick=humanSpace();quick.hold('B');assert.equal(quick.v('fs_pass_count','u16'),0);
 quick.idle();assert.equal(quick.v('fs_pass_count','u16'),1);assert.equal(quick.v('fs_lob'),0);
 const e=humanSpace();e.hold('B',12);
 assert.equal(e.v('fs_pass_count','u16'),0,'holding sets up the delivery');
 assert.ok(e.v('fs_pass_charge')>=12);e.idle();
 assert.equal(e.v('fs_lob'),1);assert.equal(e.v('fs_lob_count','u16'),1);
 assert.ok(e.v('fs_flight_left')>=62,'lob gives time to run into the zone');
 let peak=0;for(let f=0;f<33;f++){e.idle();peak=Math.max(peak,e.v('fs_ball_z','i16'));}
 assert.ok(peak>40*16,'lob clears players well above a quick pass');
 e.tap('START').idle(3);assert.ok(e.read8(0xfe00+31*4)>0,'landing ellipse is visible');
 e.shot('futsal-high-lob');
});

test('holding B off the ball calls a lob return into the human finishing lane',()=>{
 const e=humanSpace();put(e,'fs_owner',1);setPlayer(e,1,112,176);
 set16(e,'fs_ball_x',112*16);set16(e,'fs_ball_y',176*16);
 e.hold('UP+B',12);assert.equal(e.v('fs_pass_count','u16'),0,'teammate waits for the call');
 e.hold('UP');
 assert.equal(e.v('fs_receiver'),0);assert.equal(e.v('fs_lob'),1);
 assert.ok(e.v('fs_target_y','i16')<at(e,'fs_pl_y',0,'i16')-16*16,'zone leads the runner toward goal');
});

function incomingLob(){
 const e=humanSpace();put(e,'fs_owner',255);put(e,'fs_receiver',0);put(e,'fs_lob',1);
 put(e,'fs_flight_left',24);set16(e,'fs_target_x',80*16);set16(e,'fs_target_y',180*16);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',196*16);set16(e,'fs_ball_z',36*16);
 set16(e,'fs_ball_vx',0);set16(e,'fs_ball_vy',-12);set16(e,'fs_ball_vz',-16);
 return e;
}
test('a timed jump meets the descending lob with a bicycle kick and grounded shadow',()=>{
 const e=incomingLob();put(e,'fs_cooldown',50); // A recent burst must not block the jump.
 const x=at(e,'fs_pl_x',0,'i16'),y=at(e,'fs_pl_y',0,'i16');
 e.tap('A');assert.ok(e.v('fs_aerial')>0);assert.equal(e.v('fs_aerial_count','u16'),0,'windup before contact');
 e.until(e=>e.v('fs_aerial_count','u16')===1,18,'airborne foot meeting lob');
 assert.equal(e.v('fs_shot_count','u16'),1);assert.ok(e.v('fs_ball_z','i16')>0,'shot starts from actual contact height');
 assert.equal(e.v('fs_lob'),0);assert.ok(e.v('fs_power_trail')>0);
 assert.ok(e.v('fs_fx_timer')>0,'pack impact starts on real airborne contact');
 assert.equal(at(e,'fs_pl_x',0,'i16'),x);assert.equal(at(e,'fs_pl_y',0,'i16'),y);
 e.idle(5).tap('START').idle(3);
 assert.ok(e.read8(0xfe03+humanActor(e)*4)&64,'bicycle uses the actual inverted character tiles');
 const ground=e.read8(e.symbols.addr('fs_projection_y')+(y>>4))-e.v('fs_cam_y');
 assert.equal(e.read8(0xfe00+22*4),ground+12,'shadow stays under the launch point');
 e.shot('futsal-bicycle-finish');
});

test('jumping outside the zone or missing the airborne ball cannot create a shot',()=>{
 const outside=incomingLob();setPlayer(outside,0,30,180);outside.tap('A');
 assert.equal(outside.v('fs_aerial'),0);assert.equal(outside.v('fs_shot_count','u16'),0);
 const miss=incomingLob();set16(miss,'fs_ball_x',140*16);set16(miss,'fs_ball_vy',0);
 miss.tap('A');assert.ok(miss.v('fs_aerial')>0);miss.idle(30);
 assert.equal(miss.v('fs_aerial_count','u16'),0);assert.equal(miss.v('fs_shot_count','u16'),0);
 assert.equal(miss.v('fs_fx_timer'),0,'missed jump does not create an impact');
});

test('one-two, charged lob return, run into zone and bicycle finish form a playable sequence',()=>{
 const e=humanSpace();setPlayer(e,0,48,160);setPlayer(e,1,112,176);
 set16(e,'fs_ball_x',48*16);set16(e,'fs_ball_y',153*16);
 let worst=0;
 const step=buttons=>{e.hold(buttons);worst=Math.max(worst,e.lastFrameCycles);};
 step('B');step(''); // Quick first pass.
 for(let f=0;f<12;f++)step('UP+B');
 step('UP');
 for(let f=0;f<80&&!e.v('fs_lob');f++)step('UP');
 assert.equal(e.v('fs_lob'),1,'teammate sends the requested high return');
 assert.equal(e.v('fs_receiver'),0);
 for(let f=0;f<65&&at(e,'fs_pl_y',0,'i16')>e.v('fs_target_y','i16')+6*16;f++)step('UP');
 for(let f=0;f<12&&!e.v('fs_zone_ready');f++)step('');
 step('A');assert.ok(e.v('fs_aerial')>0,'runner jumps from the finishing zone');
 for(let f=0;f<25&&!e.v('fs_aerial_count','u16');f++)step('');
 assert.equal(e.v('fs_aerial_count','u16'),1,'real returned lob is struck in the air');
 assert.ok(e.v('fs_completed_passes','u16')>=2,'both passes count, including the first-time finish');
 for(let f=0;f<80&&!at(e,'score',0);f++)step('');
 assert.equal(at(e,'score',0),1,'aerial strike can finish in the goal');
 assert.ok(worst<65664,`lob-to-bicycle sequence budget: ${worst}`);
});

test('releasing a charged pass during pause delivers it on resume without sticking in charge',()=>{
 const e=humanSpace();e.hold('B',12).tap('START').idle(10);
 assert.equal(e.v('fs_pass_count','u16'),0);
 e.tap('START').idle();
 assert.equal(e.v('fs_lob'),1);assert.equal(e.v('fs_pass_charge'),0);
});

test('BURST FX contact flash animates at the strike point, pauses and cleans up',()=>{
 const e=humanSpace();e.tap('B');assert.equal(e.v('fs_fx_timer'),0,'quick passing does not spam impact bursts');
 put(e,'fs_owner',0);set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',173*16);
 set16(e,'fs_ball_vx',0);set16(e,'fs_ball_vy',0);set16(e,'fs_ball_z',0);
 e.tap('A');assert.ok(e.v('fs_fx_timer')>0);
 const x=e.v('fs_fx_x','i16'),y=e.v('fs_fx_y','i16'),poses=new Set();
 for(let f=0;f<4;f++){e.idle();poses.add(e.read8(0xfe02+33*4));}
 assert.ok(poses.size>=2,'original pack animation advances');
 assert.equal(e.v('fs_fx_x','i16'),x);assert.equal(e.v('fs_fx_y','i16'),y,'flash stays at contact, not on moving ball');
 e.tap('START').idle(3);const timer=e.v('fs_fx_timer');
 assert.ok(e.read8(0xfe00+33*4)>0);assert.equal(e.read8(0xfe03+33*4)&7,6);
 assert.equal(e.read8(0xfe00+33*4),y-e.v('fs_cam_y'),'flash follows the camera');
 e.idle(5);assert.equal(e.v('fs_fx_timer'),timer,'pause freezes the effect');
 e.shot('futsal-burst-contact');e.tap('START').idle(20);
 assert.equal(e.v('fs_fx_timer'),0);assert.equal(e.read8(0xfe00+33*4),0);assert.equal(e.read8(0xfe00+34*4),0);
});

test('kickoff perception timing varies across seeds and reproduces the same seed',()=>{
 const seeds=[1,42,77,511,1024,2026,4242,6553];
 const phases=seeds.map(seed=>startGame(undefined,seed).v('fs_kickoff_phase'));
 assert.ok(new Set(phases).size>=4,'kickoffs must not repeat one fixed slot schedule');
 assert.equal(startGame(undefined,42).v('fs_kickoff_phase'),phases[1],'seeded timing is reproducible');
});

test('seeded Watch matches give both teams wins and comparable scoring opportunities',()=>{
 const wins=[0,0],goals=[0,0],shots=[0,0];let worst=0;
 for(const seed of [1,7,19,42,77,123,256,511]){
  const e=startGame(undefined,seed);let previousShots=0;
  for(let f=0;f<18000&&e.v('game_state')===PLAY;f++){
   e.idle();if(e.v('game_state')!==PLAY||e.v('in_transition'))continue;
   worst=Math.max(worst,e.lastFrameCycles);
   const count=e.v('fs_shot_count','u16');if(count!==previousShots){shots[e.v('fs_last_kicker')>>1]+=count-previousShots;previousShots=count;}
  }
  settle(e,GAME_OVER,'balanced Watch match');
  for(let t=0;t<2;t++){const score=at(e,'score',t);goals[t]+=score;if(score===5)wins[t]++;}
 }
 console.log({balance:{wins,goals,shots,worst}});
 assert.ok(wins[0]>=2&&wins[1]>=2,'neither team should dominate the seeded sample');
 const blueShare=goals[0]/(goals[0]+goals[1]);
 assert.ok(blueShare>=.35&&blueShare<=.65,'neither side receives a persistent scoring advantage');
 assert.ok(shots[0]>=8&&shots[1]>=8,'both teams must create real shots, not just own goals');
 assert.ok(worst<65664,`balanced match frame budget: ${worst}`);
});


test('support opens a wide diagonal outlet instead of crowding a stationary carrier',()=>{
 const e=humanSpace();setPlayer(e,0,48,160);setPlayer(e,1,52,164);
 set16(e,'fs_ball_x',48*16);set16(e,'fs_ball_y',153*16);
 e.write8(e.symbols.addr('fs_think')+1,0);
 e.idle(48);
 assert.equal(e.v('fs_owner'),0,'support movement leaves human possession alone');
 assert.ok(at(e,'fs_pl_x',1,'i16')-at(e,'fs_pl_x',0,'i16')>48*16,'teammate creates real passing width');
 assert.ok(at(e,'fs_pl_y',1,'i16')<at(e,'fs_pl_y',0,'i16')-16*16,'outlet also advances toward goal');
});

function defensiveShape(mirror=false){
 const e=startGame();
 const id=i=>mirror?i^2:i;
 for(const [i,x,y] of [[0,80,160],[1,124,112],[2,80,110],[3,88,110]]){
  const n=id(i);setPlayer(e,n,mirror?160-x:x,mirror?224-y:y);
  e.write8(e.symbols.addr('fs_think')+n,200);
  for(const axis of ['x','y']){
   const v=at(e,'fs_pl_'+axis,n,'i16');
   e.writeBytes(e.symbols.addr('fs_ai_t'+axis)+n*2,[v&255,v>>8]);
  }
 }
 put(e,'fs_owner',id(0));put(e,'fs_receiver',255);
 set16(e,'fs_ball_x',80*16);set16(e,'fs_ball_y',(mirror?71:153)*16);
 for(const n of ['fs_ball_vx','fs_ball_vy','fs_ball_z','fs_ball_vz'])set16(e,n,0);
 e.write8(e.symbols.addr('fs_presser')+(mirror?0:1),id(2));
 e.write8(e.symbols.addr('fs_think')+id(2),0);e.idle();
 e.write8(e.symbols.addr('fs_think')+id(3),0);e.idle();
 return {e,id};
}

test('one defender presses while the partner covers the other attacker on either end',()=>{
 for(const mirror of [false,true]){
  const {e,id}=defensiveShape(mirror),direction=mirror?-1:1;
  assert.equal(at(e,'fs_role',id(2)),0);
  assert.equal(at(e,'fs_role',id(3)),3);
  const pressY=at(e,'fs_pl_y',id(2),'i16'),coverY=at(e,'fs_pl_y',id(3),'i16');
  e.idle(8);
  assert.ok((at(e,'fs_pl_y',id(2),'i16')-pressY)*direction>0,'presser closes the carrier');
  assert.ok((at(e,'fs_pl_y',id(3),'i16')-coverY)*direction<0,'cover drops goal-side of the off-ball attacker');
  assert.ok(Math.abs(at(e,'fs_pl_x',id(3),'i16')-at(e,'fs_pl_x',id(2),'i16'))>12*16,'defenders separate into different lanes');
 }
});

test('small changes in distance do not make both defenders chase; a clear advantage hands over the press',()=>{
 const {e}=defensiveShape();
 setPlayer(e,2,72,110);setPlayer(e,3,80,110);
 e.write8(e.symbols.addr('fs_think')+3,0);e.idle();
 assert.equal(at(e,'fs_presser',1),2,'eight pixels closer does not abandon cover');
 setPlayer(e,2,72,80);setPlayer(e,3,80,132);
 e.write8(e.symbols.addr('fs_think')+3,0);e.idle(2);
 assert.equal(at(e,'fs_presser',1),3,'clearly closer defender takes over');
 e.write8(e.symbols.addr('fs_think')+2,0);e.idle(2);
 assert.equal(at(e,'fs_role',2),3,'previous presser recovers into cover');
});
