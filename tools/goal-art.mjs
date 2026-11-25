import fs from 'node:fs';
export const GOAL_ART=JSON.parse(fs.readFileSync(new URL('../artwork/goals/v3/native.json',import.meta.url),'utf8'));
export const GOAL_NATIVE=GOAL_ART.pixels;
export const GOAL_W=GOAL_ART.width,GOAL_H=GOAL_ART.height;
