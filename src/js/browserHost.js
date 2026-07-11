import { GameState, ScriptVm } from './index.js';
const W=320,H=240,PLAY_H=200,SPLASH_MS=2000;
const canvas=document.getElementById('game'), ctx=canvas.getContext('2d'); ctx.imageSmoothingEnabled=false;
const pkg={vars:[{name:'started',value:{kind:0,bool:false,int:0,string:''}}],rooms:[{id:'demo',label:'YAAT Browser Host',color:0x202040,events:[{name:'enter',item:'',firstCommand:0,commandCount:3}],entities:[]}],commands:[
 {kind:0,a:'narrator',b:'Browser host composed with the JavaScript VM.',stringId:'',boolValue:0,intValue:0,value:{kind:2,bool:true,int:0,string:''},conditionOp:0,firstChild:0,childCount:0,firstElseChild:0,elseChildCount:0},
 {kind:1,a:'started',b:'',stringId:'',boolValue:0,intValue:0,value:{kind:0,bool:true,int:1,string:''},conditionOp:0,firstChild:0,childCount:0,firstElseChild:0,elseChildCount:0},
 {kind:7,a:'',b:'',stringId:'',boolValue:250,intValue:3,value:{kind:1,bool:true,int:250,string:''},conditionOp:0,firstChild:0,childCount:0,firstElseChild:0,elseChildCount:0}
],globalEvents:[]};
const state=new GameState(pkg,{currentRoom:'demo'}); const vm=new ScriptVm(pkg,state); let effects=vm.runEvent(pkg.rooms[0].events[0]); let dialog=effects.find(e=>e.type==='say'); let splashUntil=performance.now()+SPLASH_MS;
function rect(x,y,w,h,c){ ctx.fillStyle=c; ctx.fillRect(x|0,y|0,w|0,h|0); }
function text(s,x,y,c='#f0e8d0'){ ctx.fillStyle=c; ctx.font='8px monospace'; ctx.textBaseline='top'; ctx.fillText(s,x,y); }
function drawSplash(){ rect(0,0,W,H,'#000'); ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.font='48px monospace'; ctx.fillStyle='#f0f0f0'; ctx.fillText('YAAT',W/2,H/2-18); ctx.font='16px monospace'; ctx.fillStyle='#a0c8ff'; ctx.fillText('Yet Another Adventure Tool',W/2,H/2+30); ctx.textAlign='start'; ctx.textBaseline='top'; }
function draw(){ if(performance.now()<splashUntil){ drawSplash(); return; } rect(0,0,W,H,'#101018'); rect(0,0,W,PLAY_H,'#202040'); text('YAAT HTML5 host/demo',88,72,'#ffe070'); text('Core behavior lives in src/js modules.',52,92); rect(0,PLAY_H,W,40,'#181828'); text('Click to execute the room enter event again.',28,PLAY_H+15,'#d8d0b8'); if(dialog){ rect(16,10,288,28,'rgba(0,0,0,.75)'); text(dialog.speaker+':',24,16,'#ffe070'); text(dialog.text,24,27,'#fff'); } }
canvas.addEventListener('click',()=>{ if(performance.now()<splashUntil){ splashUntil=0; draw(); return; } effects=vm.runEvent(pkg.rooms[0].events[0]); dialog=effects.find(e=>e.type==='say') || dialog; draw(); });
draw(); setTimeout(draw,SPLASH_MS);
