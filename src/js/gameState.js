// JavaScript parity harness: script-visible mutable state.
// Owns the state that C script commands mutate: current room, variables,
// inventory, object visibility/position/sprite metadata, and player state.
// Snapshot output is intentionally plain data so parity tests can compare it
// with C runtime save/state behavior as coverage grows.
import { VALUE_KIND } from './bytecodeLoader.js';
export function cloneValue(v){ return { kind:v?.kind ?? VALUE_KIND.BOOL, bool:!!v?.bool, int:v?.int|0, string:v?.string ?? '' }; }
export class GameState {
  constructor(pkg, options={}){ this.package=pkg; this.currentRoom=options.currentRoom || pkg.rooms?.[0]?.id || ''; this.inventory=[...(options.inventory||[])]; this.vars=new Map(); this.objects=new Map(); this.player={ x:options.playerX ?? 0, y:options.playerY ?? 0, visible:options.playerVisible ?? true }; this.motion={ targetX:options.targetX ?? this.player.x, targetY:options.targetY ?? this.player.y }; for(const v of pkg.vars||[]) this.vars.set(v.name, cloneValue(v.value)); for(const room of pkg.rooms||[]) for(const ent of room.entities||[]) this.objects.set(ent.id,{ visible:!!ent.visible, x:ent.x, y:ent.y, w:ent.w, h:ent.h, sprite:'' }); }
  getVar(name){ return this.vars.get(name) || { kind:VALUE_KIND.BOOL, bool:false, int:0, string:'' }; }
  setVar(name,value){ this.vars.set(name, cloneValue(value)); }
  hasItem(id){ return this.inventory.includes(id); }
  addItem(id){ if(id && !this.hasItem(id)) this.inventory.push(id); }
  removeItem(id){ this.inventory=this.inventory.filter(x=>x!==id); }
  object(id){ if(!this.objects.has(id)) this.objects.set(id,{visible:true,x:0,y:0,w:0,h:0,sprite:''}); return this.objects.get(id); }
  currentRoomRecord(){ return (this.package.rooms||[]).find(r=>r.id===this.currentRoom) || null; }
  entityAt(x,y){ const room=this.currentRoomRecord(); if(!room) return null; for(let i=(room.entities||[]).length-1;i>=0;i--){ const ent=room.entities[i]; const obj=this.object(ent.id); const ex=obj.x ?? ent.x, ey=obj.y ?? ent.y, ew=obj.w ?? ent.w, eh=obj.h ?? ent.h; if(obj.visible && x>=ex && y>=ey && x<ex+ew && y<ey+eh) return { entity:ent, state:obj }; } return null; }
  hoverTargetAt(x,y){ const hit=this.entityAt(x,y); if(!hit) return { kind:'empty', id:'', name:'' }; const kind=hit.entity.kind===0 ? 'hotspot' : 'object'; return { kind, id:hit.entity.id, name:hit.entity.name || hit.entity.id }; }
  setPlayerTarget(x,y){ this.motion.targetX=x|0; this.motion.targetY=y|0; }
  playerMotionComplete(){ return this.player.x===this.motion.targetX && this.player.y===this.motion.targetY; }
  playerLayerVisible(){ return !!this.player.visible; }
  verbUiVisible(){ return !!this.player.visible; }
  roomChangeEnabled(hotspot){ if(!hotspot || hotspot.action!=='change_room' || !hotspot.targetRoom) return false; if(!hotspot.requiredFlag) return true; return valueTruthy(this.getVar(hotspot.requiredFlag)) === !!hotspot.requiredFlagValue; }
  tryEnterRoomChangeRegion(hotspot){ if(!this.playerMotionComplete() || !this.roomChangeEnabled(hotspot)) return false; this.currentRoom=hotspot.targetRoom; return true; }
  snapshot(){ return { currentRoom:this.currentRoom, inventory:[...this.inventory], vars:Object.fromEntries([...this.vars].map(([k,v])=>[k,cloneValue(v)])), objects:Object.fromEntries(this.objects), player:{...this.player} }; }
}
export function valueTruthy(v){ if(!v) return false; if(v.kind===VALUE_KIND.STRING) return v.string.length>0; if(v.kind===VALUE_KIND.INT) return v.int!==0; return !!v.bool; }
export function valueEquals(a,b){ if(a.kind===VALUE_KIND.STRING || b.kind===VALUE_KIND.STRING) return (a.string||'') === (b.string||''); if(a.kind===VALUE_KIND.INT || b.kind===VALUE_KIND.INT) return (a.int|0) === (b.int|0); return !!a.bool === !!b.bool; }
