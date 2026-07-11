import { COMMAND_KIND as K, CONDITION_OP as C, VALUE_KIND } from './bytecodeLoader.js';
import { valueTruthy, valueEquals } from './gameState.js';
export class ScriptVm {
  constructor(pkg, state){ this.package=pkg; this.state=state; }
  runEvent(event){ return this.runCommands(event.firstCommand, event.commandCount); }
  runCommands(first, count){ const effects=[]; for(let i=0;i<count;i++) this.exec(this.package.commands[first+i], effects); return effects; }
  cmp(left, op, right){ if(op===C.TRUTHY) return valueTruthy(left); if(op===C.EQ) return valueEquals(left,right); if(op===C.NE) return !valueEquals(left,right); const l=left.kind===VALUE_KIND.INT?left.int:Number(left.string||left.bool||0); const r=right.kind===VALUE_KIND.INT?right.int:Number(right.string||right.bool||0); if(op===C.LT) return l<r; if(op===C.LTE) return l<=r; if(op===C.GT) return l>r; if(op===C.GTE) return l>=r; return false; }
  exec(c,effects){ const s=this.state; switch(c.kind){
    case K.SAY: effects.push({type:'say', speaker:c.a, text:c.b, stringId:c.stringId}); break;
    case K.SET: s.setVar(c.a,c.value); effects.push({type:'set', name:c.a, value:c.value}); break;
    case K.GOTO: s.currentRoom=c.a; effects.push({type:'goto', room:c.a}); break;
    case K.PLAY_SOUND: effects.push({type:'playSound', path:c.a}); break;
    case K.TAKE: s.addItem(c.a); effects.push({type:'take', item:c.a}); break;
    case K.HIDE: s.object(c.a).visible=false; effects.push({type:'hide', object:c.a}); break;
    case K.IF: { const ok=this.cmp(s.getVar(c.a), c.conditionOp, c.value); effects.push(...this.runCommands(ok?c.firstChild:c.firstElseChild, ok?c.childCount:c.elseChildCount)); break; }
    case K.SHAKE: effects.push({type:'shake', duration:c.boolValue, magnitude:c.intValue}); break;
    case K.PICKUP: s.object(c.a).visible=false; s.addItem(c.b); effects.push({type:'pickup', object:c.a, item:c.b}); break;
    case K.DROP: s.removeItem(c.a); s.object(c.b).visible=true; effects.push({type:'drop', item:c.a, object:c.b}); break;
    case K.REMOVE_INVENTORY: s.removeItem(c.a); effects.push({type:'removeInventory', item:c.a}); break;
    case K.CONSUME: s.removeItem(c.a); effects.push({type:'consume', item:c.a}); break;
    case K.CALL: effects.push({type:'call', event:c.a}); break;
    case K.SHOW: s.object(c.a).visible=true; effects.push({type:'show', object:c.a}); break;
    case K.MOVE_OBJECT: Object.assign(s.object(c.a), {x:c.boolValue, y:c.intValue}); effects.push({type:'moveObject', object:c.a, x:c.boolValue, y:c.intValue}); break;
    case K.SET_OBJECT_SPRITE: s.object(c.a).sprite=c.b; effects.push({type:'setObjectSprite', object:c.a, sprite:c.b}); break;
    case K.TITLE_CARD: effects.push({type:'titleCard', title:c.a, subtitle:c.b}); break;
    case K.WAIT: effects.push({type:'wait', duration:c.intValue || c.value.int}); break;
    case K.MOVE_PLAYER: s.player.x=c.boolValue; s.player.y=c.intValue; effects.push({type:'movePlayer', x:s.player.x, y:s.player.y}); break;
    case K.SET_PLAYER_VISIBLE: s.player.visible=!!c.boolValue; effects.push({type:'setPlayerVisible', visible:s.player.visible}); break;
    case K.DIALOG: effects.push({type:'dialog', id:c.a, text:c.b, stringId:c.stringId}); break;
    case K.CHOICE: effects.push({type:'choice', prompt:c.a, option:c.b, firstChild:c.firstChild, childCount:c.childCount}); break;
    case K.ANIMATE_OBJECT: effects.push({type:'animateObject', object:c.a, animation:c.b, duration:c.intValue}); break;
    default: throw new Error(`Unsupported command ${c.kind}`);
  }}
}
