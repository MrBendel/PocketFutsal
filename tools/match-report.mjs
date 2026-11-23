#!/usr/bin/env node
// Replays recordings from the playtest page and breaks down every point: who won
// it and how, your touches (the HUD's label and the move), how soon you pressed
// after the CPU struck, and what came of each slam chance.
//
//   node tools/match-report.mjs REC.json [REC2.json ...]          on build/pocketfutsal.gb
//   node tools/match-report.mjs --pages REC.json [...]            on the ROM live on GitHub Pages
//   node tools/match-report.mjs --rom FILE.gb REC.json [...]      on another ROM (its .noi beside it)
//
// A recording only replays on the ROM it was made on (it's checked by CRC; the
// header warns when it isn't). --pages downloads the ROM the Pages site serves now,
// and its RAM symbols, into build/pages-rom/; keep a copy (and use --rom) to replay a
// recording after the site moves on.
import fs from 'node:fs';
import path from 'node:path';
import { loadRom, prepareReplay, namesFromMask, ROOT } from '../harness/node.mjs';
import { crc32 } from '../harness/emu.mjs';

const PAGES = 'https://mrbendel.github.io/PocketFutsal/';
const args = process.argv.slice(2);
const pages = args.includes('--pages');
const romAt = args.indexOf('--rom');
const files = args.filter((a, i) => !a.startsWith('--') && i !== romAt + 1);
if (!files.length) { console.error('usage: node tools/match-report.mjs [--pages] REC.json ...'); process.exit(1); }

let romPath = romAt >= 0 ? path.resolve(args[romAt + 1]) : path.join(ROOT, 'build', 'pocketfutsal.gb');
if (pages) {
  const dir = path.join(ROOT, 'build', 'pages-rom');
  fs.mkdirSync(dir, { recursive: true });
  romPath = path.join(dir, 'pages.gb');
  fs.writeFileSync(romPath, new Uint8Array(await (await fetch(PAGES + 'pocketfutsal.gb')).arrayBuffer()));
  const html = await (await fetch(PAGES)).text();
  const m = html.match(/const __SYM = (".*?");\n/);
  if (!m) throw new Error('no symbols in the Pages page');
  fs.writeFileSync(path.join(dir, 'pages.noi'), JSON.parse(m[1]));
}

const MOVES = ['-', 'kick', 'volley', 'header', 'bicycle', 'slide', 'down', 'control'];
const REASONS = ['?', 'net', 'out', 'bounce', 'touches', 'fault'];
const LABELS = ['YOU', 'PWR', 'CRV', 'LOB', 'DRP', 'GLD', 'POP', 'CTL', 'SLM', 'HDR', 'BIK', 'MIS', 'GO'];
const q = (a, f) => (a.length ? a[Math.min(a.length - 1, Math.floor(a.length * f))] : '-');

for (const file of files) {
  const replay = JSON.parse(fs.readFileSync(file, 'utf8'));
  const emu = loadRom(romPath);
  const crc = crc32(new Uint8Array(fs.readFileSync(romPath)));
  console.log(`\n=== ${path.basename(file)} (${replay.length} frames)`);
  if (replay.rom?.crc32 && replay.rom.crc32 !== crc) console.log(`  WARNING: recorded on ROM ${replay.rom.crc32}, replaying on ${crc}: it will drift${pages ? '' : ' (try --pages)'}`);
  const input = prepareReplay(emu, replay);
  const v = (n, t = 'u8', i = 0) => {
    const sz = t.endsWith('16') ? 2 : 1, a = emu.symbols.addr(n) + sz * i;
    let x = sz === 1 ? emu.read8(a) : emu.read8(a) | (emu.read8(a + 1) << 8);
    if (t === 'i16' && x > 32767) x -= 65536;
    return x;
  };
  const you = (n) => v(n, 'i16', 0) >> 4;
  let cur = null, cpuShot = null, prev = 0, cpuF = -1, react = -1, slam = null, lastTouch = -1;
  const flights = [], reacts = [], charges = [], labels = {}, tally = {}, slams = [];
  for (let f = 0; f < replay.length; f++) {
    const mask = input(f), btn = namesFromMask(mask), was = namesFromMask(prev);
    const press = ['A', 'B'].some((b) => btn.includes(b) && !was.includes(b));
    prev = mask;
    if (press && cpuF >= 0 && react < 0) react = f - cpuF;
    if (press && slam) { slam.presses++; }
    const s0 = v('pl_shots', 'u8', 0), s1 = v('pl_shots', 'u8', 1), g0 = v('goal_timer'), c0 = v('pl_charge');
    emu.setButtons(mask);
    emu.step();
    if (v('game_state') !== 1) continue;
    cur ??= { start: f, you: [], cpu: 0 };
    if (v('meet_slam') && !slam) slam = { spot: [v('meet_x', 'i16'), v('meet_y', 'i16')], near: 999, presses: 0 };
    if (slam) slam.near = Math.min(slam.near, Math.abs(you('pl_x') - slam.spot[0]) + Math.abs(you('pl_y') - slam.spot[1]));
    if (v('pl_shots', 'u8', 1) !== s1) {
      cur.cpu++;
      cpuShot = v('ball_pop') ? 'pop-up' : v('ball_lob') ? 'lob' : v('ball_drop') ? 'drop' : v('ball_power') ? 'fire' : v('ball_curve') ? 'curve' : 'drive';
      cpuShot += ` (${MOVES[v('pl_last_move', 'u8', 1)]})`;
      cpuF = f; react = -1;
    }
    let touched = v('pl_shots', 'u8', 0) !== s0;
    if (touched) {
      const lab = LABELS[v('shot_seen')], mv = MOVES[v('pl_last_move', 'u8', 0)];
      labels[lab] = (labels[lab] || 0) + 1;
      charges.push(c0);
      cur.you.push(`${lab}/${mv}`);
      if (cpuF >= 0) { flights.push(f - cpuF); if (react >= 0) reacts.push(react); }
      cpuF = -1; lastTouch = f;
    }
    const point = !g0 && v('goal_timer');
    if (slam && (touched || point || !v('meet_slam'))) {
      slams.push(`${touched ? LABELS[v('shot_seen')] : point ? 'point' : 'missed'}, got within ${slam.near} px, ${slam.presses} presses`);
      slam = null;
    }
    if (point) {
      const cpuWon = v('last_scorer') === 1, reason = REASONS[v('point_reason')], last = v('last_touch');
      const d = Math.abs((v('ball_x', 'i16') >> 4) - you('pl_x')) + Math.abs((v('ball_y', 'i16') >> 4) - you('pl_y'));
      const how = cpuWon
        ? last === 0 ? `your shot: ${reason}` : `CPU ${cpuShot} past you, ${d > 40 ? 'far' : d > 16 ? 'near' : 'close'}, you ${MOVES[v('pl_move')] || '-'}${v('pl_shot') ? ', queued' : ''}`
        : last === 1 ? `CPU's shot: ${reason}` : `your shot: ${reason}`;
      tally[(cpuWon ? 'lost  ' : 'won   ') + how] = (tally[(cpuWon ? 'lost  ' : 'won   ') + how] || 0) + 1;
      console.log(`${v('score', 'u8', 0)}-${v('score', 'u8', 1)}`.padEnd(6) + `${((f - cur.start) / 60).toFixed(0).padStart(3)}s  ${cpuWon ? 'lost' : 'WON '}  ${how}\n        you: ${cur.you.join(' ') || '-'}`);
      cur = null;
    }
  }
  console.log('\nhow points went:\n' + Object.entries(tally).sort((a, b) => b[1] - a[1]).map(([k, n]) => `${String(n).padStart(4)}  ${k}`).join('\n'));
  console.log('your touches:', JSON.stringify(labels));
  reacts.sort((a, b) => a - b); flights.sort((a, b) => a - b); charges.sort((a, b) => a - b);
  console.log(`after the CPU strikes: first press at frame ${q(reacts, 0.5)} (median; ${q(reacts, 0.25)}-${q(reacts, 0.75)}), the ball reaches you at ${q(flights, 0.5)}; your meter as you strike: ${q(charges, 0.5)} (median)`);
  console.log(`slam chances (${slams.length}):\n  ` + slams.join('\n  '));
}
