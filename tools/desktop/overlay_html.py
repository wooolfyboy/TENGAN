# -*- coding: utf-8 -*-
"""Страницы всплывашек и виджета (свои маленькие окна поверх всех). Стили NERV / DOMAIN.

Окна получают данные из Python (evaluate_js) и зовут обратно через window.pywebview.api:
  всплывашки:  P.add(извещение), P.style('nerv'|'domain') → api.popup_size(h), api.popup_click()
  виджет:      W.update(снимок), W.style(...)               → api.widget_open()
"""

COMMON_CSS = """
*{box-sizing:border-box;margin:0;padding:0}
html,body{background:transparent;overflow:hidden;user-select:none;-webkit-user-select:none;cursor:default}
body{font:13px/1.25 'Segoe UI',system-ui,sans-serif;color:#ffd9a8}
.jp{font-family:'Yu Gothic UI','Yu Gothic','Meiryo','Noto Sans CJK JP','IPAGothic',sans-serif;font-weight:900}
.mono{font-family:'Cascadia Mono','Consolas','DejaVu Sans Mono',monospace}
"""

POPUP_HTML = """<!doctype html><html><head><meta charset="utf-8"><style>%CSS%
#stack{display:flex;flex-direction:column;gap:8px;padding:6px}
.card{position:relative;height:96px;overflow:hidden;cursor:pointer;animation:in .35s cubic-bezier(.2,.9,.3,1.2)}
.card.out{animation:out .3s ease-in forwards}
@keyframes in{from{transform:translateX(110%);opacity:0}to{transform:none;opacity:1}}
@keyframes out{to{transform:translateX(110%);opacity:0}}
.kj{position:absolute;left:66px;top:6px;font-size:34px;line-height:1;letter-spacing:2px}
.tt{position:absolute;left:68px;top:46px;font:700 13px 'Segoe UI',sans-serif;letter-spacing:2px;text-transform:uppercase}
.tx{position:absolute;left:68px;top:66px;right:10px;font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tm{position:absolute;left:0;bottom:0;height:3px;width:100%;transform-origin:left;animation:tm linear forwards}
@keyframes tm{from{transform:scaleX(1)}to{transform:scaleX(0)}}
/* --- NERV: чёрный, оранжевая рамка, полосы «осторожно» --- */
body.nerv .card{background:#060302;border:2px solid #ff8c1a;color:#ffcf99}
body.nerv .card .side{position:absolute;left:0;top:0;bottom:0;width:54px;background:repeating-linear-gradient(-45deg,#ff8c1a 0 9px,#000 9px 18px)}
body.nerv .kj{color:#ff8c1a}body.nerv .tt{color:#ffb347}body.nerv .tm{background:#ff8c1a}
body.nerv .card.alert{border-color:#ff2a1f;background:#1c0200}
body.nerv .card.alert .side{background:repeating-linear-gradient(-45deg,#ff2a1f 0 9px,#000 9px 18px)}
body.nerv .card.alert .kj,body.nerv .card.alert .tt{color:#ff2a1f}body.nerv .card.alert .tm{background:#ff2a1f}
body.nerv .card.alert .kj{animation:bl 1s steps(1) 3}
@keyframes bl{50%{opacity:.2}}
/* --- DOMAIN: фиолетовая тьма, бумажная «офуда» с печатью --- */
body.domain .card{background:radial-gradient(circle at 20% 50%,#24185a,#0b0820 70%);border:2px solid #d9d4ef;outline:1px solid #000;color:#d9d4ef;
 box-shadow:inset 0 0 22px rgba(157,107,255,.35)}
body.domain .card .side{position:absolute;left:6px;top:6px;bottom:6px;width:42px;background:#efe6d2;border:1px solid #111;
 display:flex;align-items:center;justify-content:center}
body.domain .card .side b{display:block;color:#e3212b;border:2px solid #e3212b;font:900 14px/1.05 'Yu Gothic','Meiryo','IPAGothic',sans-serif;padding:3px 2px;transform:rotate(-6deg);text-align:center}
body.nerv .card .side b{display:none}
body.domain .kj{color:#efe6d2;text-shadow:0 0 10px rgba(127,178,255,.8)}body.domain .tt{color:#7fb2ff}body.domain .tm{background:#9d6bff}
body.domain .card.alert{box-shadow:inset 0 0 26px rgba(227,33,43,.55)}body.domain .card.alert .kj{color:#ff3b45;text-shadow:0 0 12px rgba(227,33,43,.9)}
body.domain .card.alert .tt{color:#ff6b72}body.domain .card.alert .tm{background:#e3212b}
</style></head><body class="nerv"><div id="stack"></div><script>
var MAX=3,DUR=4500,S=document.getElementById('stack');
function api(){return window.pywebview&&window.pywebview.api}
function report(){var a=api();var h=S.children.length?S.getBoundingClientRect().height+4:0;if(a&&a.popup_size)a.popup_size(Math.ceil(h))}
var SEAL={new:'接近',tracker:'追跡',ir:'赤外',nfc:'近接',sensor:'感知',temp:'外気',battery:'電池',link:'通信'};
window.P={
 style:function(s){document.body.className=s||'nerv'},
 dur:function(s){DUR=Math.max(1500,s*1000)},
 add:function(n){
  while(S.children.length>=MAX){S.removeChild(S.firstChild)}
  var c=document.createElement('div');c.className='card '+(n.sev||'info');
  c.innerHTML='<div class="side"><b>'+(SEAL[n.kind]||'天眼').split('').join('<br>')+'</b></div><div class="kj jp"></div><div class="tt"></div><div class="tx mono"></div><div class="tm"></div>';
  c.querySelector('.kj').textContent=n.kj||'天眼';c.querySelector('.tt').textContent=n.title||'';c.querySelector('.tx').textContent=n.text||'';
  c.querySelector('.tm').style.animationDuration=DUR+'ms';
  c.onclick=function(){var a=api();if(a&&a.popup_click)a.popup_click()};
  S.appendChild(c);report();
  setTimeout(function(){c.classList.add('out');setTimeout(function(){if(c.parentNode)S.removeChild(c);report()},320)},DUR);
 }};
</script></body></html>""".replace("%CSS%", COMMON_CSS)


WIDGET_HTML = """<!doctype html><html><head><meta charset="utf-8"><style>%CSS%
body{width:100vw;height:100vh}
#w{position:absolute;inset:0;padding:8px 10px;display:grid;grid-template-columns:auto 1fr;grid-template-rows:auto 1fr;gap:6px 10px}
#hd{grid-column:1/3;display:flex;align-items:center;gap:8px;font:700 12px 'Segoe UI',sans-serif;letter-spacing:2px}
#hd .dot{width:8px;height:8px;transform:rotate(45deg)}
#hd .sp{flex:1}
#rd{width:118px;height:118px;align-self:center}
#rt{display:flex;flex-direction:column;gap:4px;min-width:0}
.lv{display:flex;align-items:baseline;justify-content:space-between}
.lv b{font-size:16px}.bar{height:9px;display:flex;gap:2px}.bar i{flex:1}
.parts{display:flex;gap:6px;font-size:9px;letter-spacing:1px}.parts span{flex:1}.parts u{display:block;height:3px;margin-top:2px;text-decoration:none}
.tp{font:700 26px 'Segoe UI',sans-serif;line-height:1}.tp small{font-size:12px;font-weight:400;margin-left:4px}
.ln{font-size:11px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
/* NERV */
body.nerv #w{background:#050302;border:2px solid #ff8c1a}
body.nerv #hd{color:#ff8c1a}body.nerv .lv b{color:#ffb347}body.nerv .tp{color:#ffb347}body.nerv .ln{color:#ffcf99}
body.nerv .bar i{background:#2a1405}body.nerv .bar i.on{background:#ff8c1a}body.nerv .bar i.hot{background:#ff2a1f}
body.nerv .parts{color:#ff8c1a}body.nerv .parts u{background:#ff8c1a}body.nerv .parts span.off{opacity:.3}
/* DOMAIN */
body.domain #w{background:radial-gradient(circle at 25% 60%,#1d1450,#07060c 75%);border:2px solid #d9d4ef}
body.domain #hd{color:#efe6d2}body.domain .lv b{color:#7fb2ff}body.domain .tp{color:#efe6d2}body.domain .ln{color:#d9d4ef}
body.domain .bar i{background:#1a1730;transform:skewX(-20deg)}body.domain .bar i.on{background:#4f7bff}body.domain .bar i.hot{background:#e3212b}
body.domain .parts{color:#9d6bff}body.domain .parts u{background:#9d6bff}body.domain .parts span.off{opacity:.3}
</style></head><body class="nerv"><div id="w">
<div id="hd"><span class="jp" id="tt">天眼</span><span>TENGAN</span><span class="sp"></span><span id="bat" class="mono"></span><i class="dot" id="lk"></i></div>
<canvas id="rd" width="236" height="236"></canvas>
<div id="rt">
 <div class="lv"><b class="jp" id="st">平穏</b><span class="mono" id="pc">0%</span></div>
 <div class="bar" id="bar"></div>
 <div class="parts" id="parts"></div>
 <div class="tp" id="tp">—<small id="tps"></small></div>
 <div class="ln mono" id="ir">IR —</div>
 <div class="ln mono" id="nfc">NFC —</div>
</div></div><script>
var STYLE='nerv',D=null,t0=Date.now(),B=document.getElementById('bar');
for(var i=0;i<20;i++)B.appendChild(document.createElement('i'));
var NAMES=['平穏','活発','注意','警報'],EN=['QUIET','ACTIVE','ATTN','ALARM'];
var COL={nerv:{ring:'rgba(255,140,26,.5)',sw:'rgba(255,140,26,',dot:'#ffb347',neu:'#ff2a1f',sav:'#69ff6e',bg:'#050302'},
         domain:{ring:'rgba(217,212,239,.45)',sw:'rgba(127,178,255,',dot:'#7fb2ff',neu:'#e3212b',sav:'#efe6d2',bg:'#07060c'}};
function api(){return window.pywebview&&window.pywebview.api}
document.getElementById('w').ondblclick=function(){var a=api();if(a&&a.widget_open)a.widget_open()};
function ago(t){if(!t)return'';var s=Math.max(0,Math.round(Date.now()/1000-t));return s<60?s+'s':Math.round(s/60)+'m'}
window.W={
 style:function(s){STYLE=s||'nerv';document.body.className=STYLE},
 update:function(d){D=d;var a=d.act||{v:0,state:0,parts:{},en:{}};
  document.getElementById('st').textContent=NAMES[a.state]||'';document.getElementById('pc').textContent=Math.round(a.v*100)+'% '+EN[a.state];
  var n=Math.round(a.v*20),w=Math.round((a.warn||.6)*20);
  for(var i=0;i<20;i++)B.children[i].className=i<n?(i>=w?'hot':'on'):'';
  var P=document.getElementById('parts');P.innerHTML='';
  [['ble','BLE'],['rf','433'],['snd','SND'],['prb','IR·NFC']].forEach(function(k){var s=document.createElement('span');
   s.className=a.en&&a.en[k[0]]===false?'off':'';s.innerHTML=k[1]+'<u style="width:'+Math.round(((a.parts||{})[k[0]]||0)*100)+'%"></u>';P.appendChild(s)});
  var tp=document.getElementById('tp'),tps=document.getElementById('tps');
  if(d.temp&&d.temp.t!=null){tp.firstChild.nodeValue=(d.temp.t>0?'+':'')+d.temp.t.toFixed(1)+'°';tps.textContent=(d.temp.h!=null?d.temp.h+'% ':'')+(d.temp.src||'')}
  else{tp.firstChild.nodeValue='—';tps.textContent='no outdoor sensor'}
  document.getElementById('ir').textContent=d.ir?'IR  '+(d.ir.brand||d.ir.p)+'  '+ago(d.ir.t):'IR  —';
  document.getElementById('nfc').textContent=d.nfc?'NFC '+d.nfc.fam+'  '+ago(d.nfc.t):'NFC —';
  document.getElementById('bat').textContent=d.bat!=null?'BAT '+d.bat+'%':'';
  var lk=document.getElementById('lk');lk.style.background=d.link?(STYLE==='nerv'?'#69ff6e':'#7fb2ff'):'#ff2a1f';
 }};
var cv=document.getElementById('rd'),g=cv.getContext('2d');
function frame(){requestAnimationFrame(frame);var C=COL[STYLE],W=cv.width,H=cv.height,cx=W/2,cy=H/2,R=W/2-6,t=(Date.now()-t0)/1000;
 g.clearRect(0,0,W,H);g.lineWidth=1.5;
 for(var i=1;i<=3;i++){g.strokeStyle=C.ring;g.beginPath();g.arc(cx,cy,R*i/3,0,6.2832);g.stroke()}
 var sw=(t*1.4)%6.2832;for(var k=0;k<16;k++){g.fillStyle=C.sw+(0.22*(1-k/16)).toFixed(3)+')';g.beginPath();g.moveTo(cx,cy);g.arc(cx,cy,R,sw-(k+1)*0.04,sw-k*0.04);g.closePath();g.fill()}
 if(D&&D.devs){var bl=D.blink&&((t*2)%1<0.6);D.devs.forEach(function(e){var p=Math.max(0,Math.min(1,(e.r+95)/50)),rr=R*(0.08+0.92*(1-p)),a=e.a*6.2832;
  var x=cx+rr*Math.cos(a),y=cy+rr*Math.sin(a),alert=e.n||e.k;g.fillStyle=e.s?C.sav:alert?C.neu:C.dot;
  if(alert&&!bl&&D.blink){g.globalAlpha=.35}g.beginPath();g.arc(x,y,alert?5:3,0,6.2832);g.fill();g.globalAlpha=1;
  if(alert){g.strokeStyle=C.neu;g.beginPath();g.arc(x,y,10,0,6.2832);g.stroke()}})}
 g.fillStyle=C.bg;g.beginPath();g.arc(cx,cy,9,0,6.2832);g.fill();g.strokeStyle=C.dot;g.lineWidth=2;g.stroke();}
frame();
</script></body></html>""".replace("%CSS%", COMMON_CSS)
