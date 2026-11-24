// Reproducible Watch-mode balance audit. No player inputs after kickoff.
// node tools/futsal-balance.mjs [number of seeds, default 12]
import fs from 'node:fs';
import {bootToTitle,settle,at} from '../tests/helpers.mjs';
const seeds=[1,7,19,42,77,123,256,511,1024,2026,4242,7777,17,99,2048,6553];
const count=Math.max(1,Math.min(seeds.length,Number(process.argv[2])||12));
const games=[];
for(const seed of seeds.slice(0,count)){
 const e=bootToTitle();e.tap('DOWN').idle(40);
 e.write8(e.symbols.addr('fs_pace'),2); // Audit Expert simulation ticks.
 e.writeBytes(e.symbols.addr('test_seed'),[seed&255,seed>>8]);settle(e.tap('START'),3,'futsal');
 const phase=e.v('fs_kickoff_phase'),possession=[0,0],shots=[0,0];
 let frames=0,worst=0,previousShots=0;
 while(e.v('game_state')===3&&frames<18000){
  e.idle();frames++;
  if(e.v('game_state')!==3||e.v('in_transition'))continue;
  worst=Math.max(worst,e.lastFrameCycles);
  const owner=e.v('fs_owner');if(owner<4&&!e.v('fs_goal_timer'))possession[owner>>1]++;
  const total=e.v('fs_shot_count','u16');if(total!==previousShots){shots[e.v('fs_last_kicker')>>1]+=total-previousShots;previousShots=total;}
 }
 const result={seed,phase,score:[at(e,'score',0),at(e,'score',1)],frames,finished:e.v('game_state')!==3,worst,possession,shots,passes:e.v('fs_pass_count','u16'),completed:e.v('fs_completed_passes','u16')};
 games.push(result);console.log(JSON.stringify(result));
}
const totals=games.reduce((a,g)=>{for(let t=0;t<2;t++){a.goals[t]+=g.score[t];a.shots[t]+=g.shots[t];a.possession[t]+=g.possession[t];if(g.score[t]===5)a.wins[t]++;}a.worst=Math.max(a.worst,g.worst);return a;},{goals:[0,0],shots:[0,0],possession:[0,0],wins:[0,0],worst:0});
console.log(JSON.stringify({totals}));
fs.mkdirSync('build',{recursive:true});fs.writeFileSync('build/futsal-balance.json',JSON.stringify({games,totals},null,2)+'\n');
