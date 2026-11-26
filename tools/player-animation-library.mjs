// Complete authoring library. Runtime tile layouts remain separately budgeted.
import fs from 'node:fs';
import {CHARACTERS,character} from './player-art.mjs';
const read=p=>JSON.parse(fs.readFileSync(new URL(`../artwork/players/${p}`,import.meta.url)));
const reactions=read('roster/reactions/study-v2/native-16x24.json');
const slides=read('slide/study-v1/native-32x24.json');
const keeper=read('keeper-dive/study-v1/native-32x24.json');
const moves=['idle','run-a','run-b','ready','kick','volley','header','bicycle','win-legacy','loss-legacy'];
export const ANIMATION_ROWS=['front','back','transition-front','transition-back','side-right','win','loss','slide-right','dive-low-right','dive-high-right'];
export function playerAnimations(id){
 if(!CHARACTERS.includes(id))throw new Error(`Unknown player: ${id}`);
 const c=character(id),r=reactions.characters[id];
 const sequence=(name,poses,names,extra={})=>({name,sheetRow:ANIMATION_ROWS.indexOf(name),...extra,frames:poses.map((p,i)=>({name:names[i],rows:p.rows??p,...(p.leadingBoot?{leadingBoot:p.leadingBoot}:{}),...(name==='win'?{lift:r.jump[i]}:{})}))});
 const animations=[
  ...c.views.map((poses,v)=>sequence(v?'back':'front',poses,moves)),
  ...c.transitions.map((poses,v)=>sequence(v?'transition-back':'transition-front',poses,['bicycle-tuck','bicycle-land','slide-rise'])),
  sequence('side-right',c.side,['idle','run-a','run-b'],{mirrorForLeft:true}),
  sequence('win',r.win,['anticipate','raise-fist','takeoff','apex','descend','land']),
  sequence('loss',r.loss,['stand','crouch','turn','center','turn-back','settle']),
  sequence('slide-right',slides.characters.find(c=>c.id===id).frames,slides.characters.find(c=>c.id===id).frames.map(f=>f.phase),{mirrorForLeft:true}),
 ];
 if(id==='spiky')for(const kind of ['low','high'])animations.push(sequence(`dive-${kind}-right`,keeper[kind],keeper.phases,{mirrorForLeft:true,contactFrame:keeper.savePose}));
 return {id,palette:r.palette,animations};
}
