// Resident-frame streams for Shootout: approved keeper and reaction reductions.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const read=p=>JSON.parse(fs.readFileSync(new URL(`../artwork/players/${p}`,import.meta.url)));
export const KEEPER=read('keeper-dive/study-v1/native-32x24.json');
export const REACTIONS=read('roster/reactions/study-v2/native-16x24.json').characters.spiky;
const empty=Array(8).fill('........');
const cut=(p,x,y)=>p.slice(y,y+8).map(r=>r.slice(x,x+8));
// Each 8x16 lower column has a blank upper half, and is drawn at y+8.
// It ends at the actual 24px baseline, avoiding invisible OAM over the banner.
export const KEEPER_TILES=[...KEEPER.low,...KEEPER.high].flatMap(f=>{
 assert.equal(f.rows.length,24);for(const r of f.rows)assert.match(r,/^[.123]{32}$/);
 return [0,8,16,24].flatMap(x=>[cut(f.rows,x,0),cut(f.rows,x,8),empty,cut(f.rows,x,16)]);
});
export const KEEPER_MASKS=[...KEEPER.low,...KEEPER.high].map(f=>f.spriteColumns8x16.reduce((m,c)=>m|(1<<((c.x/8)*2+c.y/16)),0));
export const SHOOTOUT_REACTIONS=[...REACTIONS.win,...REACTIONS.loss].flatMap(f=>[0,8].flatMap(x=>[cut(f.rows,x,0),cut(f.rows,x,8),empty,cut(f.rows,x,16)]));

