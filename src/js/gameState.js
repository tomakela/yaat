import { VALUE_KIND } from './bytecodeLoader.js';
export function cloneValue(v){ return { kind:v?.kind ?? VALUE_KIND.BOOL, bool:!!v?.bool, int:v?.int|0, string:v?.string ?? '' }; }
export class GameState {
  constructor(pkg, options={}){ this.package=pkg; this.currentRoom=options.currentRoom || pkg.rooms?.[0]?.id || ''; this.inventory=[...(options.inventory||[])]; this.vars=new Map(); this.objects=new Map(); this.player={ x:options.playerX ?? 0, y:options.playerY ?? 0, visible:true }; for(const v of pkg.vars||[]) this.vars.set(v.name, cloneValue(v.value)); for(const room of pkg.rooms||[]) for(const ent of room.entities||[]) this.objects.set(ent.id,{ visible:!!ent.visible, x:ent.x, y:ent.y, w:ent.w, h:ent.h, sprite:'' }); }
  getVar(name){ return this.vars.get(name) || { kind:VALUE_KIND.BOOL, bool:false, int:0, string:'' }; }
  setVar(name,value){ this.vars.set(name, cloneValue(value)); }
  hasItem(id){ return this.inventory.includes(id); }
  addItem(id){ if(id && !this.hasItem(id)) this.inventory.push(id); }
  removeItem(id){ this.inventory=this.inventory.filter(x=>x!==id); }
  object(id){ if(!this.objects.has(id)) this.objects.set(id,{visible:true,x:0,y:0,w:0,h:0,sprite:''}); return this.objects.get(id); }
  snapshot(){ return { currentRoom:this.currentRoom, inventory:[...this.inventory], vars:Object.fromEntries([...this.vars].map(([k,v])=>[k,cloneValue(v)])), objects:Object.fromEntries(this.objects), player:{...this.player} }; }
}
export function valueTruthy(v){ if(!v) return false; if(v.kind===VALUE_KIND.STRING) return v.string.length>0; if(v.kind===VALUE_KIND.INT) return v.int!==0; return !!v.bool; }
export function valueEquals(a,b){ if(a.kind===VALUE_KIND.STRING || b.kind===VALUE_KIND.STRING) return (a.string||'') === (b.string||''); if(a.kind===VALUE_KIND.INT || b.kind===VALUE_KIND.INT) return (a.int|0) === (b.int|0); return !!a.bool === !!b.bool; }
