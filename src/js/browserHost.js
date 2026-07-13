// Browser host integration demo for the JavaScript parity harness.
// This file is intentionally the DOM edge: it composes GameState and ScriptVm
// with canvas input/drawing for a tiny demo, while the core src/js modules stay
// free of window/document dependencies. It is not a full browser engine yet.
import { GameState, ScriptVm } from './index.js';
const W=320,H=240,PLAY_H=200,SPLASH_MS=2000,DIALOG_MS=2400;
const canvas=document.getElementById('game'), ctx=canvas.getContext('2d'); ctx.imageSmoothingEnabled=false;
const command=(kind,fields={})=>({kind,a:'',b:'',stringId:'',boolValue:0,intValue:0,value:{kind:0,bool:false,int:0,string:''},conditionOp:0,firstChild:0,childCount:0,firstElseChild:0,elseChildCount:0,...fields});
const pkg={vars:[{name:'started',value:{kind:0,bool:false,int:0,string:''}}],rooms:[{id:'demo',label:'YAAT Browser Host',color:0x202040,events:[{name:'enter',item:'',firstCommand:0,commandCount:5}],entities:[]}],commands:[
 command(0,{a:'narrator',b:'Browser host composed with the JavaScript VM.'}),
 command(17,{intValue:900,value:{kind:1,bool:true,int:900,string:''}}),
 command(0,{a:'narrator',b:'Dialogue advances from a small saying queue.'}),
 command(1,{a:'started',value:{kind:0,bool:true,int:1,string:''}}),
 command(7,{boolValue:250,intValue:3,value:{kind:1,bool:true,int:250,string:''}})
],globalEvents:[]};
const state=new GameState(pkg,{currentRoom:'demo'}); const vm=new ScriptVm(pkg,state); let effects=[]; let sayingQueue=[]; let dialog=null; let dialogUntil=0; let actionSentence=''; let actionUntil=0; let splashUntil=performance.now()+SPLASH_MS; let animationFrame=0;
function rect(x,y,w,h,c){ ctx.fillStyle=c; ctx.fillRect(x|0,y|0,w|0,h|0); }
function text(s,x,y,c='#f0e8d0'){ ctx.fillStyle=c; ctx.font='8px monospace'; ctx.textBaseline='top'; ctx.fillText(s,x,y); }
function drawSplash(){ rect(0,0,W,H,'#000'); ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.font='48px monospace'; ctx.fillStyle='#f0f0f0'; ctx.fillText('YAAT',W/2,H/2-18); ctx.font='16px monospace'; ctx.fillStyle='#a0c8ff'; ctx.fillText('Yet Another Adventure Tool',W/2,H/2+30); ctx.textAlign='start'; ctx.textBaseline='top'; }
function scheduleDraw(){ if(!animationFrame) animationFrame=requestAnimationFrame(()=>{ animationFrame=0; draw(); }); }
function startDialog(entry){ dialog=entry; dialogUntil=performance.now()+DIALOG_MS; setTimeout(()=>{ if(dialog===entry && performance.now()>=dialogUntil){ dialog=null; scheduleDraw(); } },DIALOG_MS); }
function showNextSaying(){ const next=sayingQueue.shift(); if(next) startDialog(next); else dialog=null; }
function processEffects(nextEffects){ effects=nextEffects; sayingQueue=effects.filter(e=>e.type==='say'); const wait=effects.find(e=>e.type==='wait'); if(wait){ actionSentence='Waiting...'; actionUntil=performance.now()+wait.duration; setTimeout(()=>{ if(performance.now()>=actionUntil){ actionSentence=''; scheduleDraw(); } },wait.duration); } showNextSaying(); }
function draw(){ const now=performance.now(); if(now<splashUntil){ drawSplash(); return; } if(dialog && now>=dialogUntil) dialog=null; if(actionSentence && now>=actionUntil) actionSentence=''; rect(0,0,W,H,'#101018'); rect(0,0,W,PLAY_H,'#202040'); text('YAAT HTML5 host/demo',88,72,'#ffe070'); text('Core behavior lives in src/js modules.',52,92); rect(0,PLAY_H,W,40,'#181828'); text(actionSentence || 'Click to continue the saying queue.',32,PLAY_H+15,'#d8d0b8'); if(dialog){ rect(16,10,288,24,'rgba(0,0,0,.75)'); text(dialog.text,24,18,'#fff'); } }
canvas.addEventListener('click',()=>{ if(performance.now()<splashUntil){ splashUntil=0; draw(); return; } if(dialog || sayingQueue.length){ showNextSaying(); draw(); return; } processEffects(vm.runEvent(pkg.rooms[0].events[0])); draw(); });
processEffects(vm.runEvent(pkg.rooms[0].events[0])); draw(); setTimeout(draw,SPLASH_MS);
