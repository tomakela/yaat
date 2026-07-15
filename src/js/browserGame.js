import { loadGameData } from './browserGameData.js';

'use strict';
const W=320,H=240,PLAY_H=200,PLAYER_W=18,PLAYER_H=34,PLAYER_WALK_SPEED=120,MAX_WALK_DELTA_MS=50,SPLASH_MS=2000,DIALOG_MS=2400,TOUCH_CURSOR_SENSITIVITY=1.75,TOUCH_DRAG_THRESHOLD=3,TOUCH_LONGPRESS_MOVE_THRESHOLD=12,TOUCH_LONGPRESS_MS=550;
const canvas=document.getElementById('game'), main=document.querySelector('main'), fullscreenBtn=document.getElementById('fullscreenBtn'), touchModeInput=document.getElementById('touchMode'), ctx=canvas.getContext('2d'); ctx.imageSmoothingEnabled=false;
const touchState={active:false,id:-1,lastX:0,lastY:0,totalMove:0,moved:false,longTimer:0,longFired:false,suppressClickUntil:0,captureTarget:null};
touchModeInput.checked=true;
function isTouchMode(){ return touchModeInput.checked; }
function updateFullscreenButton(){ fullscreenBtn.textContent=document.fullscreenElement?'Exit full screen':'Full screen'; }
async function lockLandscapeIfPossible(){ try{ if(screen.orientation?.lock) await screen.orientation.lock('landscape'); }catch(_){} }
async function toggleFullscreen(){ if(document.fullscreenElement){ await document.exitFullscreen?.(); return; } await main.requestFullscreen?.(); await lockLandscapeIfPossible(); }
fullscreenBtn.addEventListener('click',()=>toggleFullscreen().catch(()=>{}));
touchModeInput.addEventListener('change',()=>{ touchState.active=false; clearTouchLongPress(); });
document.addEventListener('fullscreenchange',()=>{ updateFullscreenButton(); if(document.fullscreenElement) lockLandscapeIfPossible(); });
updateFullscreenButton();
function clearTouchLongPress(){ if(touchState.longTimer){ clearTimeout(touchState.longTimer); touchState.longTimer=0; } }
function endTouchCursor(e, triggerTap=true){ if(!touchState.active || e.pointerId!==touchState.id) return false; const wasDrag=touchState.moved, wasLong=touchState.longFired, wasTouchMode=isTouchMode(); touchState.active=false; clearTouchLongPress(); touchState.captureTarget?.releasePointerCapture?.(e.pointerId); touchState.captureTarget=null; if(wasTouchMode||wasLong) touchState.suppressClickUntil=performance.now()+500; if(wasTouchMode&&triggerTap&&!wasDrag&&!wasLong) triggerCursorClick(); return true; }
const imageCache=new Map(), transparentImageCache=new Map();
function img(path){ if(!imageCache.has(path)){ const im=new Image(); im.decoding='async'; im.failed=false; im.addEventListener('error',()=>{ im.failed=true; },{once:true}); im.src='game/'+path; imageCache.set(path,im);} return imageCache.get(path); }
function imageLoaded(im){ return im.complete&&im.naturalWidth>0; }
function colorKeyRgb(color){ const m=(color||'').match(/^#?([0-9a-f]{6})$/i); if(!m) return null; const n=parseInt(m[1],16); return [(n>>16)&255,(n>>8)&255,n&255]; }
function transparentImg(path,color){ const im=img(path), rgb=colorKeyRgb(color); if(!rgb||!imageLoaded(im)) return im; const key=path+'|'+color.toLowerCase(); if(!transparentImageCache.has(key)){ const c=document.createElement('canvas'); c.width=im.naturalWidth; c.height=im.naturalHeight; const g=c.getContext('2d'); g.drawImage(im,0,0); const data=g.getImageData(0,0,c.width,c.height); for(let i=0;i<data.data.length;i+=4){ if(data.data[i]===rgb[0]&&data.data[i+1]===rgb[1]&&data.data[i+2]===rgb[2]) data.data[i+3]=0; } g.putImageData(data,0,0); transparentImageCache.set(key,c); } return transparentImageCache.get(key); }
function preloadImages(paths){ return Promise.all([...new Set(paths)].map(path=>new Promise(resolve=>{ const im=img(path); if(imageLoaded(im)){ resolve(); return; } im.addEventListener('load',resolve,{once:true}); im.addEventListener('error',resolve,{once:true}); }))); }
function rect(x,y,w,h,c){ ctx.fillStyle=c; ctx.fillRect(x|0,y|0,w|0,h|0); }
function text(s,x,y,c='#f0e8d0'){ ctx.fillStyle=c; ctx.font='8px monospace'; ctx.textBaseline='top'; ctx.fillText(s,x,y); }
function clamp(v,a,b){ return Math.max(a,Math.min(b,v)); }
function canvasMetrics(){ const b=canvas.getBoundingClientRect(); const scale=Math.min(b.width/W,b.height/H)||1; const contentW=W*scale, contentH=H*scale; return {left:b.left+(b.width-contentW)/2, top:b.top+(b.height-contentH)/2, scale}; }
function canvasPoint(e){ const m=canvasMetrics(); return {x:clamp((e.clientX-m.left)/m.scale,0,W-1), y:clamp((e.clientY-m.top)/m.scale,0,H-1)}; }
function setCursorFromClient(e){ const p=canvasPoint(e); state.cx=p.x; state.cy=p.y; }
function moveCursorByClientDelta(dx,dy){ const m=canvasMetrics(); state.cx=clamp(state.cx+dx*TOUCH_CURSOR_SENSITIVITY/m.scale,0,W-1); state.cy=clamp(state.cy+dy*TOUCH_CURSOR_SENSITIVITY/m.scale,0,H-1); }
function hashColor(s, fallback){ let h=fallback; for (const ch of s) h=((h*33)^ch.charCodeAt(0))>>>0; return '#'+(h&0xffffff).toString(16).padStart(6,'0'); }
const {verbs, verbLabels, inventoryDefs, rooms, firstRoom, initialVars, playerTransparentColor}=await loadGameData();

const state={playerVisible:true,splashUntil:performance.now()+SPLASH_MS,room:'room000_start',px:160,py:100,tx:160,ty:100,face:1,cx:160,cy:100,verb:'walk',selectedInv:'',inv:[],vars:{...initialVars},dialog:null,dialogUntil:0,sayQueue:[],actionSentence:'',actionUntil:0,shakeUntil:0,shakeMag:0,pending:null,chained:null,suppressedExit:null,lastUpdate:0};
function showSay(speaker,msg){ state.dialog={speaker,msg,t:performance.now()}; state.dialogUntil=performance.now()+DIALOG_MS; }
function say(speaker,msg){ if(state.dialog) state.sayQueue.push({speaker,msg}); else showSay(speaker,msg); }
function advanceSay(){ const next=state.sayQueue.shift(); if(next) showSay(next.speaker,next.msg); else { state.dialog=null; state.dialogUntil=0; } }
function setActionSentence(text,ms=0){ state.actionSentence=text; state.actionUntil=ms>0?performance.now()+ms:0; }
function clearActionSentence(){ state.actionSentence=''; state.actionUntil=0; }
function tickActionSentence(){ if(state.actionSentence&&state.actionUntil&&performance.now()>=state.actionUntil) clearActionSentence(); }
function tickDialogue(){ if(state.dialog&&performance.now()>=state.dialogUntil) advanceSay(); tickActionSentence(); }
function play(path){ const a=new Audio('game/'+path); a.volume=0.7; a.play().catch(()=>{}); }
function shake(ms,mag){ state.shakeUntil=performance.now()+ms; state.shakeMag=mag; }
function go(id,x=160,y=180){ state.room=id; state.px=state.tx=x; state.py=state.ty=y; state.selectedInv=''; state.chained=null; state.suppressedExit=null; const e=rooms[id].enter; if(typeof e==='function') e(); else if(e) say('narrator',e); }
function deselectVerb(){ state.verb='walk'; state.selectedInv=''; }
function cant(verb){ const lines={look:"I don't see anything special.",use:"I can't use that.",talk:"I can't talk to that.",take:"I can't take that.",open:"I can't open that.",close:"I can't close that."}; say('player',lines[verb]||"I can't do that."); }
function roomItems(){ const r=rooms[state.room]; return [...r.hotspots, ...r.objects.filter(visibleFor)]; }
function at(x,y){ const a=roomItems(); for(let i=a.length-1;i>=0;i--){ const o=a[i]; if(x>=o.x&&y>=o.y&&x<o.x+o.w&&y<o.y+o.h) return o; } return null; }
function walkToObj(o){ state.tx=o.walkX??clamp(o.x+(o.w>>1),PLAYER_W/2,W-PLAYER_W/2); state.ty=o.walkY??clamp(o.y+o.h-1,PLAYER_H,PLAY_H-1); }
function canChangeRoom(o){ return !!(o&&o.targetRoom&&(!o.requires||o.requires())); }
function exitSuppressed(o){ return !!(o&&state.suppressedExit===o.id&&state.px>=o.x&&state.py>=o.y&&state.px<o.x+o.w&&state.py<o.y+o.h); }
function updateSuppressedExit(){ if(!state.suppressedExit) return; const r=rooms[state.room]; const h=r.hotspots.find(h=>h.id===state.suppressedExit); if(!exitSuppressed(h)) state.suppressedExit=null; }
function hasVerbAction(o,verb){ return !!(o&&(verb==='walk'||o[verb]||(verb==='use'&&!state.selectedInv&&o.inventoryItem))); }
function shouldWalkForVerb(o,verb){ if(!hasVerbAction(o,verb)) return false; const setting=o.walkBefore?.[verb]??o.walkBefore; return setting!==false; }
function changeRoomFrom(o){ go(o.targetRoom,o.targetX??160,o.targetY??180); }
function playerExit(){ const r=rooms[state.room]; for(let i=r.hotspots.length-1;i>=0;i--){ const h=r.hotspots[i]; if(canChangeRoom(h)&&!exitSuppressed(h)&&state.px>=h.x&&state.py>=h.y&&state.px<h.x+h.w&&state.py<h.y+h.h) return h; } return null; }
function beginPendingInteraction(o){ walkToObj(o); state.pending=state.verb==='walk'&&!state.selectedInv&&o.inventoryItem?null:o; }
function continueChainedInteraction(){ const o=state.chained; state.chained=null; if(!o||!state.selectedInv) return false; const command=`Use ${inventoryDefs[state.selectedInv].name} with `; setActionSentence(command+o.name); if(!hasVerbAction(o,'use')||!shouldWalkForVerb(o,'use')) interact(o); else beginPendingInteraction(o); return true; }
function interact(o){ if(!o) return; const verb=state.selectedInv?'use':state.verb; if(verb==='walk'){ if(canChangeRoom(o)) changeRoomFrom(o); else if(o.use) o.use.call(o); clearActionSentence(); return; } if(verb==='use'&&!state.selectedInv&&o.inventoryItem){ if(o.click) o.click.call(o); else if(o.take) o.take.call(o); else cant(verb); if(state.inv.includes(o.inventoryItem)){ state.verb='use'; state.selectedInv=o.inventoryItem; if(continueChainedInteraction()) return; } clearActionSentence(); return; } if(o[verb]) o[verb].call(o); else cant(verb); if(state.selectedInv) state.suppressedExit=o.id; deselectVerb(); clearActionSentence(); }
function triggerCursorLongClick(){ if(performance.now()<state.splashUntil){ state.splashUntil=0; return; } const x=state.cx,y=state.cy; if(y>=PLAY_H){ const ii=state.inv.findIndex((_,i)=>x>=164+i*23&&x<184+i*23&&y>=PLAY_H+4&&y<PLAY_H+24); if(ii>=0){ const def=inventoryDefs[state.inv[ii]]; setActionSentence(`Look at ${def?.name||state.inv[ii]}`,1000); say('player',def?.description||`I have ${def?.name||state.inv[ii]}.`); } return; } const o=at(x,y); if(o){ setActionSentence(`Look at ${o.name}`,1000); if(o.longclick) o.longclick.call(o); else if(o.look) o.look.call(o); else cant('look'); } }
function update(now=performance.now()){ const elapsed=state.lastUpdate?Math.min(now-state.lastUpdate,MAX_WALK_DELTA_MS):1000/30; state.lastUpdate=now; const maxStep=PLAYER_WALK_SPEED*elapsed/1000; const dx=state.tx-state.px, dy=state.ty-state.py, d=Math.hypot(dx,dy); if(d>0){ const step=Math.min(maxStep,d); state.px+=dx/d*step; state.py+=dy/d*step; if(dx<0) state.face=0; else if(dx>0) state.face=1; updateSuppressedExit(); const exit=playerExit(); if(exit){ changeRoomFrom(exit); return; } } if(d<=maxStep && state.pending){ const p=state.pending; state.pending=null; interact(p); } else if(d<=maxStep&&!state.pending){ if(state.actionSentence&&!state.actionUntil) clearActionSentence(); updateSuppressedExit(); const exit=playerExit(); if(exit) changeRoomFrom(exit); } }
function drawBmp(path,x,y,w,h,transparentColor){ const im=img(path); if(imageLoaded(im)) ctx.drawImage(transparentImg(path,transparentColor),x,y,w||im.naturalWidth,h||im.naturalHeight); else if(im.failed) rect(x,y,w||24,h||24,hashColor(path,0x2f5f9e)); }
function roomScaleForY(room,y){ const sc=room?.scale||{}; const nearY=sc.nearY??PLAY_H-1, farY=sc.farY??0; if(nearY===farY) return 1; const t=clamp((y-farY)/(nearY-farY),0,1); return (sc.farScale??1)+(((sc.nearScale??1)-(sc.farScale??1))*t); }
function playerSpritePath(moving){ if(moving) return state.face?'graphics/sprites/player_walk_right.bmp':'graphics/sprites/player_walk_left.bmp'; return state.face?'graphics/sprites/player_idle_right.bmp':'graphics/sprites/player_idle_left.bmp'; }
function drawPlayer(sx,sy){ const moving=Math.abs(state.px-state.tx)+Math.abs(state.py-state.ty)>0.1; const path=playerSpritePath(moving); const im=img(path); const scale=Math.max(0.05,roomScaleForY(rooms[state.room],state.py)); if(imageLoaded(im)){ const w=Math.max(1,Math.round(im.naturalWidth*scale)), h=Math.max(1,Math.round(im.naturalHeight*scale)); ctx.drawImage(transparentImg(path,playerTransparentColor),state.px-w/2+sx,state.py-h+sy,w,h); } else { ctx.save(); ctx.translate(state.px+sx,state.py+sy); ctx.scale(scale,scale); rect(-11,7,22,5,'#664f38'); rect(-5,-34,10,10,'#5a3a24'); rect(-6,-25,12,15,'#2f5f9e'); rect(-9,-22,4,12,'#274774'); rect(5,-22,4,12,'#274774'); rect(-5,-10,4,10,'#1f2430'); rect(1,-10,4,10,'#1f2430'); ctx.restore(); } }
function drawSplash(){ rect(0,0,W,H,'#000'); ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.font='48px monospace'; ctx.fillStyle='#f0f0f0'; ctx.fillText('YAAT',W/2,H/2-18); ctx.font='16px monospace'; ctx.fillStyle='#a0c8ff'; ctx.fillText('Yet Another Adventure Tool',W/2,H/2+30); ctx.textAlign='start'; ctx.textBaseline='top'; }
function draw(){ tickDialogue(); const now=performance.now(); if(now<state.splashUntil||!state.assetsReady){ drawSplash(); return; } let sx=0,sy=0; if(now<state.shakeUntil){ const m=Math.max(1,Math.round(state.shakeMag*(state.shakeUntil-now)/350)); sx=(Math.random()*m*2-m)|0; sy=(Math.random()*m*2-m)|0; } const r=rooms[state.room]; rect(0,0,W,H,hashColor(r.bg,0xd8c7a3)); drawBmp(r.bg,sx,sy,W,PLAY_H); for(const h of r.hotspots){ rect(h.x+sx,h.y+sy,h.w,h.h,'rgba(240,208,32,.45)'); rect(h.x+1+sx,h.y+1+sy,Math.max(0,h.w-2),Math.max(0,h.h-2),'rgba(192,128,32,.35)'); } for(const o of r.objects){ if(!visibleFor(o)) continue; drawBmp(spriteFor(o),o.x+sx,o.y+sy,o.w,o.h,o.transparentColor); } if(!r.hidePlayer&&state.playerVisible!==false) drawPlayer(sx,sy); if(!r.hideUI) drawUI(); drawDialogue(); drawCursor(); }
function drawUI(){ rect(0,PLAY_H,W,40,'#101018'); verbs.forEach((v,i)=>{ const x=4+(i%3)*50,y=PLAY_H+3+(i/3|0)*15; rect(x,y,48,13,v===state.verb?'#6a5430':'#303040'); text(v,x+5,y+3); }); state.inv.forEach((id,i)=>{ const x=164+i*23,y=PLAY_H+4; rect(x,y,20,20,id===state.selectedInv?'#806020':'#404048'); rect(x+1,y+1,18,18,'#202028'); drawBmp(inventoryDefs[id].icon,x+2,y+2,16,16); }); const hover=state.cy<PLAY_H&&at(state.cx,state.cy); const command=state.selectedInv?`Use ${inventoryDefs[state.selectedInv].name} with `:(verbLabels[state.verb]||state.verb)+' '; text(state.actionSentence||command+(hover?hover.name:''),164,PLAY_H+28,'#d8d0b8'); }
function drawDialogue(){ if(!state.dialog) return; rect(16,10,288,20,'rgba(0,0,0,.75)'); text(state.dialog.msg,24,17,'#fff'); }
function drawCursor(){ const use=(at(state.cx,state.cy)?.cursor==='use')||state.selectedInv||state.verb==='use'; rect(state.cx,state.cy,2,14,'#000'); rect(state.cx+2,state.cy+2,2,10,'#000'); rect(state.cx+4,state.cy+4,2,8,'#000'); rect(state.cx+6,state.cy+6,2,6,'#000'); rect(state.cx+8,state.cy+8,2,4,'#000'); rect(state.cx+1,state.cy+1,1,11,use?'#ffe070':'#fff'); rect(state.cx+2,state.cy+3,2,7,use?'#ffe070':'#fff'); rect(state.cx+4,state.cy+5,2,5,use?'#ffe070':'#fff'); rect(state.cx+6,state.cy+7,2,3,use?'#ffe070':'#fff'); }
function triggerCursorClick(){ if(performance.now()<state.splashUntil){ state.splashUntil=0; return; } if(state.dialog||state.sayQueue.length){ advanceSay(); return; } const x=state.cx,y=state.cy; if(y>=PLAY_H){ if(rooms[state.room].hideUI) return; const vi=verbs.findIndex((_,i)=> x>=4+(i%3)*50&&x<52+(i%3)*50&&y>=PLAY_H+3+(i/3|0)*15&&y<PLAY_H+16+(i/3|0)*15); if(vi>=0){ state.verb=verbs[vi]; state.selectedInv=''; return; } const ii=state.inv.findIndex((_,i)=>x>=164+i*23&&x<184+i*23&&y>=PLAY_H+4&&y<PLAY_H+24); if(ii>=0){ state.selectedInv=state.inv[ii]; state.verb='use'; return; } return; } const o=at(x,y); if(o){ const verb=state.selectedInv?'use':state.verb; if(state.pending?.inventoryItem&&state.verb==='use'&&!state.selectedInv&&o!==state.pending){ state.chained=o; const itemName=inventoryDefs[state.pending.inventoryItem]?.name||state.pending.name; setActionSentence(`Use ${itemName} with ${o.name}`); return; } const command=state.selectedInv?`Use ${inventoryDefs[state.selectedInv].name} with `:(verbLabels[state.verb]||state.verb)+' '; setActionSentence(command+o.name); if(!hasVerbAction(o,verb)||!shouldWalkForVerb(o,verb)){ interact(o); } else { beginPendingInteraction(o); } } else { clearActionSentence(); state.pending=null; state.chained=null; if(state.selectedInv||state.verb!=='walk'){ deselectVerb(); return; } state.tx=clamp(x,PLAYER_W/2,W-PLAYER_W/2); state.ty=clamp(y,PLAYER_H,PLAY_H-1); } }
function isControlEvent(e){ return e.target.closest?.('.controls'); }
canvas.addEventListener('mousemove',e=>setCursorFromClient(e));
canvas.addEventListener('click',e=>{ if(performance.now()<touchState.suppressClickUntil) return; setCursorFromClient(e); triggerCursorClick(); });
document.addEventListener('contextmenu',e=>{ if(!isControlEvent(e)) e.preventDefault(); },{passive:false});
document.addEventListener('selectstart',e=>{ if(!isControlEvent(e)) e.preventDefault(); },{passive:false});
document.addEventListener('pointerdown',e=>{ if(isControlEvent(e)||e.pointerType==='mouse'||e.button>0) return; touchState.active=true; touchState.id=e.pointerId; touchState.lastX=e.clientX; touchState.lastY=e.clientY; touchState.totalMove=0; touchState.moved=false; touchState.longFired=false; touchState.captureTarget=e.target; if(!isTouchMode()) setCursorFromClient(e); clearTouchLongPress(); touchState.longTimer=setTimeout(()=>{ if(touchState.active&&touchState.id===e.pointerId&&!touchState.moved){ if(!isTouchMode()) setCursorFromClient(e); touchState.longFired=true; touchState.longTimer=0; triggerCursorLongClick(); } },TOUCH_LONGPRESS_MS); touchState.captureTarget?.setPointerCapture?.(e.pointerId); if(isTouchMode()) e.preventDefault(); },{passive:false});
document.addEventListener('pointermove',e=>{ if(!touchState.active || e.pointerId!==touchState.id) return; const dx=e.clientX-touchState.lastX, dy=e.clientY-touchState.lastY; touchState.totalMove+=Math.hypot(dx,dy); if(touchState.totalMove>(isTouchMode()?TOUCH_DRAG_THRESHOLD:TOUCH_LONGPRESS_MOVE_THRESHOLD)){ touchState.moved=true; clearTouchLongPress(); } if(isTouchMode()) moveCursorByClientDelta(dx,dy); else setCursorFromClient(e); touchState.lastX=e.clientX; touchState.lastY=e.clientY; if(isTouchMode()||touchState.longFired) e.preventDefault(); },{passive:false});
document.addEventListener('pointerup',e=>{ if(!endTouchCursor(e)) return; if(isTouchMode()||touchState.longFired) e.preventDefault(); },{passive:false});
document.addEventListener('pointercancel',e=>{ if(endTouchCursor(e,false)) e.preventDefault(); },{passive:false});
if('serviceWorker' in navigator){ window.addEventListener('load',()=>{ navigator.serviceWorker.register('./sw.js').then(reg=>reg.update()).catch(()=>{}); }); }
function loop(now){ update(now); draw(); requestAnimationFrame(loop); }
function collectImagePaths(){ const paths=['graphics/sprites/player_idle.bmp','graphics/sprites/player_idle_left.bmp','graphics/sprites/player_idle_right.bmp','graphics/sprites/player_walk_left.bmp','graphics/sprites/player_walk_right.bmp']; for(const def of Object.values(inventoryDefs)) paths.push(def.icon); for(const room of Object.values(rooms)){ paths.push(room.bg); for(const o of room.objects){ if(typeof o.sprite==='string') paths.push(o.sprite); } } paths.push('rooms/room002_exit/objects/exit_lamp_lit.bmp'); return paths; }

function truthyVar(name){ return !!state.vars[name]; }
function runScriptCommands(commands=[], ctx={}){
  for(const c of commands){
    switch(c.type){
      case 'say': say(c.speaker, c.text); break;
      case 'set': state.vars[c.name]=c.value; break;
      case 'play_sound': play(c.path); break;
      case 'goto': go(c.room, c.targetX ?? 160, c.targetY ?? 180); break;
      case 'hide_player': state.playerVisible=false; break;
      case 'show_player': state.playerVisible=true; break;
      case 'hide': objectState(c.id).visible=false; break;
      case 'show': objectState(c.id).visible=true; break;
      case 'pickup': objectState(c.object).visible=false; if(!state.inv.includes(c.item)) state.inv.push(c.item); break;
      case 'consume': state.inv=state.inv.filter(id=>id!==c.item); if(state.selectedInv===c.item) state.selectedInv=''; break;
      case 'drop': state.inv=state.inv.filter(id=>id!==c.item); objectState(c.object).visible=true; break;
      case 'shake': shake(c.ms, c.mag); break;
      case 'set_sprite': objectState(c.id).sprite=c.sprite; break;
      case 'if': runScriptCommands(truthyVar(c.varName)?c.then:c.else, ctx); break;
    }
  }
}
function objectState(id){
  const r=rooms[state.room];
  const obj=[...r.objects, ...r.hotspots].find(o=>o.id===id) || {};
  obj.runtime ??= {};
  return obj.runtime;
}
function visibleFor(o){ return o.runtime?.visible ?? o.visible !== false; }
function spriteFor(o){ return o.runtime?.sprite ?? o.sprite; }
function runEvent(o, eventName){
  const ev=o?.events?.find(e=>e.name===eventName && (!e.item || e.item===state.selectedInv))
        || o?.events?.find(e=>e.name===eventName && !e.item)
        || o?.events?.find(e=>e.name==='click' && !e.item);
  if(ev) runScriptCommands(ev.commands,{target:o});
  return !!ev;
}
function hydrateRooms(){
  for(const room of Object.values(rooms)){
    room.enter=()=>runScriptCommands(room.events?.find(e=>e.name==='enter')?.commands||[]);
    for(const item of [...room.hotspots, ...room.objects]){
      item.look=()=>runEvent(item,'look');
      item.use=()=>runEvent(item,'use');
      item.take=()=>runEvent(item,'take');
      item.click=()=>runEvent(item,'click');
      if(item.events?.some(e=>e.walkBefore===false)) item.walkBefore=false;
    }
  }
}
hydrateRooms();
preloadImages(collectImagePaths()).then(()=>{ state.assetsReady=true; });
go(firstRoom,160,100); loop();
