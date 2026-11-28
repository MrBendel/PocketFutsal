// Complete authoring library. Runtime tile layouts remain separately budgeted.
import fs from 'node:fs';
import {CHARACTERS,character} from './player-art.mjs';
const read=p=>JSON.parse(fs.readFileSync(new URL(`../artwork/players/${p}`,import.meta.url)));
const reactions=read('roster/reactions/study-v2/native-16x24.json');
const slides=read('slide/study-v1/native-32x24.json');
const keeper=read('keeper-dive/study-v1/native-32x24.json');
const bicycles=read('bicycle/v1/native-24.json');
const campus=read('campus/v2/campus.json');
const avatars=read('mugshots/v4/mugshots-16.json');
const dialogueAvatars=read('portraits/v2/portraits-32x33.json');
const moves=['idle','run-a','run-b','ready','kick','volley','header'];
export const ANIMATION_ROWS=['front','back','bicycle-front','bicycle-back','side-right','win','loss','slide-right','dive-low-right','dive-high-right','campus-down','campus-up','campus-right','campus-left','avatar','avatar-32'];
export function playerAnimations(id){
 if(!CHARACTERS.includes(id))throw new Error(`Unknown player: ${id}`);
 const c=character(id),r=reactions.characters[id],b=bicycles.characters.find(c=>c.id===id);
 const sequence=(name,poses,names,extra={})=>({name,sheetRow:ANIMATION_ROWS.indexOf(name),...extra,frames:poses.map((p,i)=>({name:names[i],rows:p.rows??p,...(p.leadingBoot?{leadingBoot:p.leadingBoot}:{}),...(p.lift!==undefined?{lift:p.lift}:{}),...(name==='win'?{lift:r.jump[i]}:{})}))});
 const animations=[
  ...c.views.map((poses,v)=>sequence(v?'back':'front',poses.slice(0,7),moves)),
  ...['front','back'].map(view=>sequence(`bicycle-${view}`,b[view],['tuck','strike','recover'])),
  sequence('side-right',c.side,['idle','run-a','run-b'],{mirrorForLeft:true}),
  sequence('win',r.win,['anticipate','raise-fist','takeoff','apex','descend','land']),
  sequence('loss',r.loss,['stand','crouch','turn','center','turn-back','settle']),
  sequence('slide-right',slides.characters.find(c=>c.id===id).frames,slides.characters.find(c=>c.id===id).frames.map(f=>f.phase),{mirrorForLeft:true}),
 ];
 if(id==='spiky')for(const kind of ['low','high'])animations.push(sequence(`dive-${kind}-right`,keeper[kind],keeper.phases,{mirrorForLeft:true,contactFrame:keeper.savePose}));
 const walker=campus.characters.find(c=>c.id===id),avatar=avatars.characters.find(c=>c.id===id);
 for(const direction of campus.directions)animations.push(sequence(`campus-${direction}`,walker.animations[direction].map(f=>({rows:f.rows.map(r=>r.replaceAll('0','.'))})),campus.phases,{kind:'campus',walkCycle:walker.walkCycle,stepMs:walker.stepMs}));
 animations.push(sequence('avatar',[avatar.rows.map(r=>r.replaceAll('0','.'))],['portrait'],{kind:'portrait',palette:avatar.palette}));
 const dialogue=dialogueAvatars.characters.find(c=>c.id===id);
 if(dialogue.rows.length!==33||dialogue.rows[32]!=='0'.repeat(32))throw new Error('Expected one background-only padding row below 32x32 dialogue art');
 animations.push(sequence('avatar-32',[dialogue.rows.slice(0,32)],['dialogue-portrait'],{kind:'portrait',opaque:true,palette:dialogue.palette,source:'portraits/v2/portraits-32x33.json',omittedBottomPaddingRows:1}));
 return {id,palette:r.palette,animations};
}
