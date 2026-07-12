// JavaScript parity harness: bytecode decoding boundary.
// Mirrors src/script_bytecode.c by turning .yaatbc bytes into the same
// package-shaped records used by the C runtime. Keep this module limited to
// explicit little-endian decoding, fixed-width Windows-1252 strings, and
// validation of bytecode structure; execution semantics belong in scriptVm.js.
export const VALUE_KIND = Object.freeze({ BOOL: 0, INT: 1, STRING: 2 });
export const COMMAND_KIND = Object.freeze({
  SAY: 0, SET: 1, GOTO: 2, PLAY_SOUND: 3, TAKE: 4, HIDE: 5, IF: 6, SHAKE: 7,
  PICKUP: 8, DROP: 9, REMOVE_INVENTORY: 10, CONSUME: 11, CALL: 12, SHOW: 13,
  MOVE_OBJECT: 14, SET_OBJECT_SPRITE: 15, TITLE_CARD: 16, WAIT: 17, MOVE_PLAYER: 18,
  SET_PLAYER_VISIBLE: 19, DIALOG: 20, CHOICE: 21, ANIMATE_OBJECT: 22
});
export const CONDITION_OP = Object.freeze({ TRUTHY: 0, EQ: 1, NE: 2, LT: 3, LTE: 4, GT: 5, GTE: 6 });
const MAGIC = [0x59,0x41,0x41,0x54,0x42,0x43,0,0];
const VERSION = 5;
class Reader {
  constructor(bytes){ this.bytes = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes); this.offset = 0; }
  need(n){ if(this.offset + n > this.bytes.length) throw new Error(`Unexpected end of .yaatbc at ${this.offset}`); }
  u16(){ this.need(2); const v=this.bytes[this.offset] | (this.bytes[this.offset+1]<<8); this.offset+=2; return v; }
  u32(){ const lo=this.u16(); const hi=this.u16(); return (lo + hi * 0x10000) >>> 0; }
  str(n){ this.need(n); const start=this.offset; this.offset+=n; let end=start; const max=start+n; while(end<max && this.bytes[end]!==0) end++; return new TextDecoder('windows-1252').decode(this.bytes.subarray(start,end)); }
}
function value(r){ const kind=r.u16(); if(kind > VALUE_KIND.STRING) throw new Error(`Invalid value kind ${kind}`); return { kind, bool: r.u16() ? true : false, int: r.u32() | 0, string: r.str(96) }; }
function event(r){ return { name:r.str(32), item:r.str(32), firstCommand:r.u16(), commandCount:r.u16() }; }
function command(r){ const kind=r.u16(); if(kind > COMMAND_KIND.ANIMATE_OBJECT) throw new Error(`Invalid command kind ${kind}`); return { kind, a:r.str(96), b:r.str(96), stringId:r.str(64), boolValue:r.u16(), intValue:r.u16(), value:value(r), conditionOp:r.u16(), firstChild:r.u16(), childCount:r.u16(), firstElseChild:r.u16(), elseChildCount:r.u16() }; }
function entity(r){ const kind=r.u16(); const e={ kind, id:r.str(32), name:r.str(64), x:r.u16(), y:r.u16(), w:r.u16(), h:r.u16(), visible:!!r.u16(), events:[] }; const n=r.u16(); for(let i=0;i<n;i++) e.events.push(event(r)); return e; }
function room(r){ const out={ id:r.str(32), label:r.str(64), color:r.u32(), events:[], entities:[] }; const ec=r.u16(), nc=r.u16(); for(let i=0;i<ec;i++) out.events.push(event(r)); for(let i=0;i<nc;i++) out.entities.push(entity(r)); return out; }
export function decodeYaatBytecode(buffer){ const r=new Reader(buffer); for(const b of MAGIC) if(r.u16 && r.bytes[r.offset++] !== b) throw new Error('Invalid .yaatbc magic'); const version=r.u16(); const flags=r.u16(); if(version!==VERSION) throw new Error(`Unsupported .yaatbc version ${version}`); if(flags!==0) throw new Error(`Unsupported .yaatbc flags ${flags}`); const varCount=r.u16(), roomCount=r.u16(), commandCount=r.u16(), globalEventCount=r.u16(); const vars=[]; for(let i=0;i<varCount;i++) vars.push({ name:r.str(32), value:value(r) }); const commands=[]; for(let i=0;i<commandCount;i++) commands.push(command(r)); const globalEvents=[]; for(let i=0;i<globalEventCount;i++) globalEvents.push(event(r)); const rooms=[]; for(let i=0;i<roomCount;i++) rooms.push(room(r)); return { version, flags, vars, commands, globalEvents, rooms }; }
