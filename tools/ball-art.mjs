import fs from 'node:fs';
const data=JSON.parse(fs.readFileSync(new URL('../artwork/ball/v1/ball.json',import.meta.url)));
export const BALL_FRAMES=data.frames.map(f=>f.rows);
const blank=Array(8).fill('........');
export const BALL_EXTRA=BALL_FRAMES.slice(2).flatMap(frame=>[frame,blank]);
