import {test} from 'node:test';
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
 console.log({frames,passes:e.v('fs_pass_count','u16'),shots:e.v('fs_shot_count','u16'),blue:at(e,'score',0),red:at(e,'score',1)});
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
 e.writeBytes(e.symbols.addr('fs_pl_y')+4,[y&255,y>>8]);
 set16(e,'fs_ball_x',x); set16(e,'fs_ball_y',y);
 e.tap('A'); assert.ok(e.v('fs_tackle_count','u16')>0);
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
