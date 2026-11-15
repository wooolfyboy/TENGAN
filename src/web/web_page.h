// ============================================================
//  web_page.h — страница, которую палка отдаёт телефону.
//  Вся страница — один файл: разметка, стиль и скрипт. Меню из шести
//  вкладок; каждая сама спрашивает у палки свои данные (/api/...).
//  Вкладки Sub-GHz и Sound включают CC1101 / микрофон, пока открыты.
// ============================================================
#pragma once
#include <Arduino.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TENGAN</title>
<style>
:root{--bg:#02090b;--panel:#071419;--line:#12303a;--acc:#3fd9ff;--hot:#eafcff;--steel:#7f97a3;--dim:#46606b;--alert:#ff8a3d}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--hot);font:14px/1.35 ui-monospace,Menlo,Consolas,monospace}
header{position:sticky;top:0;background:var(--bg);border-bottom:1px solid var(--line);z-index:3}
.top{padding:10px 16px;display:flex;align-items:center;gap:12px}
h1{font-size:15px;margin:0;color:var(--acc);letter-spacing:.08em}.sp{flex:1}.mut{color:var(--steel)}.dim{color:var(--dim)}
nav{display:flex;gap:6px;padding:0 16px 10px;overflow-x:auto;scrollbar-width:none}nav::-webkit-scrollbar{display:none}
nav button{flex:0 0 auto;background:var(--panel);color:var(--steel);border:1px solid var(--line);padding:8px 12px;font:inherit;font-size:12px;letter-spacing:.08em;cursor:pointer}
nav button.on{color:var(--bg);background:var(--acc);border-color:var(--acc)}
main{padding:12px 16px 40px;max-width:960px;margin:0 auto}
.card{background:var(--panel);border:1px solid var(--line);padding:12px;margin-bottom:12px;clip-path:polygon(0 0,calc(100% - 12px) 0,100% 12px,100% 100%,12px 100%,0 calc(100% - 12px))}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:12px}.grid .card{margin:0}
.lab{font-size:11px;color:var(--steel);letter-spacing:.1em;text-transform:uppercase}
.big{font-size:42px;font-weight:600;line-height:1.05}.mid{font-size:22px;font-weight:600}
.row{display:flex;gap:18px;align-items:flex-end;flex-wrap:wrap}
canvas{width:100%;display:block}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.chip{border:1px solid var(--line);padding:3px 8px;font-size:12px;color:var(--steel)}.chip b{color:var(--hot);font-weight:600}
table{width:100%;border-collapse:collapse}th{font-size:11px;color:var(--steel);text-align:left;font-weight:400;letter-spacing:.08em;padding:6px 4px;border-bottom:1px solid var(--line);cursor:pointer;white-space:nowrap}
td{padding:7px 4px;border-bottom:1px solid #0b1f26;vertical-align:middle}td.n{max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
tr.cl{cursor:pointer}tr.cl:hover td{background:#0a1d24}
.bar{height:6px;background:#0b2630;width:64px;position:relative}.bar i{position:absolute;left:0;top:0;bottom:0;background:var(--acc)}
.alert{color:var(--alert)}.hide{display:none!important}
.seg{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}.seg button{background:transparent;color:var(--steel);border:1px solid var(--line);padding:6px 10px;font:inherit;font-size:12px;cursor:pointer}.seg button.on{color:var(--hot);border-color:var(--acc)}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 14px}.kv span:nth-child(odd){color:var(--steel)}
.banner{border:1px solid var(--alert);color:var(--alert);padding:10px 12px;margin-bottom:12px}
.clk{cursor:pointer}.clk:hover{border-color:var(--acc)}
#modal{position:fixed;inset:0;background:rgba(0,0,0,.7);display:flex;align-items:flex-end;justify-content:center;z-index:5}
#modal .card{width:100%;max-width:640px;max-height:85vh;overflow:auto;margin:0}
.x{float:right;background:none;border:1px solid var(--line);color:var(--steel);font:inherit;padding:4px 10px;cursor:pointer}
.empty{color:var(--steel);padding:24px 0;text-align:center}
</style></head><body>
<header><div class="top"><h1>// TENGAN</h1><span class="sp"></span><a href="/tablet" style="color:var(--acc);text-decoration:none;border:1px solid var(--line);padding:4px 10px">TABLET</a><span id="bat" class="mut"></span><span id="up" class="dim"></span></div>
<nav id="nav"></nav></header>
<main>
<section id="p-ov"><div class="grid">
 <div class="card clk" onclick="go('ble')"><div class="lab">bluetooth</div><div class="big" id="o-b">-</div><div class="mut" id="o-b2"></div></div>
 <div class="card clk" onclick="go('wifi')"><div class="lab">wi-fi networks</div><div class="big" id="o-w">-</div><div class="mut" id="o-w2"></div></div>
 <div class="card clk" onclick="go('sub')"><div class="lab">sub-ghz</div><div class="mid">433 MHz</div><div class="mut">open to listen</div></div>
 <div class="card clk" onclick="go('snd')"><div class="lab">sound</div><div class="mid">mic</div><div class="mut">open to listen</div></div>
 <div class="card clk" onclick="go('sys')"><div class="lab">battery</div><div class="big" id="o-p">-</div><div class="mut" id="o-p2"></div></div>
 <div class="card clk" onclick="go('sys')"><div class="lab">device</div><div class="mid" id="o-d">-</div><div class="mut" id="o-d2"></div></div>
</div></section>

<section id="p-ble">
 <div id="trk" class="banner hide"></div>
 <div class="card"><div class="row"><div><div class="lab">devices around</div><div class="big" id="bc">-</div></div><div class="mut" id="bm"></div></div>
  <canvas id="bh" height="70"></canvas><div class="chips" id="bt"></div></div>
 <div class="card hide" id="sens"><div class="lab">thermometers</div><div id="sl"></div></div>
 <div class="card"><table><thead><tr><th data-k="r">SIGNAL</th><th data-k="n">DEVICE</th><th data-k="t">TYPE</th><th data-k="age">NEAR</th></tr></thead><tbody id="bl"></tbody></table></div>
</section>

<section id="p-wifi">
 <div class="card"><div class="row"><div><div class="lab">networks</div><div class="big" id="wc">-</div></div><div class="mut" id="wb"></div></div><canvas id="wch" height="120"></canvas></div>
 <div class="card"><table><thead><tr><th data-k="r">SIGNAL</th><th data-k="s">NETWORK</th><th data-k="c">CH</th></tr></thead><tbody id="wl"></tbody></table></div>
</section>

<section id="p-sub">
 <div class="card"><div class="seg" id="sm"></div><div class="seg" id="sb"></div>
  <div id="s-none" class="empty hide">CC1101 radio not found</div>
  <div id="s-sp"><div class="mut" id="s-rng"></div><canvas id="spc" height="140"></canvas><canvas id="wf" height="160"></canvas></div>
  <div id="s-mo" class="hide"><div class="row"><div><div class="lab">signals caught</div><div class="big" id="s-ev">0</div></div>
   <div><div class="lab">now</div><div class="mid" id="s-st">quiet</div></div><div class="mut" id="s-fl"></div></div>
   <canvas id="mon" height="120"></canvas><table><thead><tr><th>WHEN</th><th>LENGTH</th><th>PEAK</th></tr></thead><tbody id="s-tb"></tbody></table></div>
 </div>
</section>

<section id="p-snd">
 <div class="card" id="a-off"><div class="empty" id="a-msg">starting microphone…</div></div>
 <div id="a-on" class="hide">
  <div class="card"><div class="row"><div><div class="lab">loudness</div><div class="big" id="a-lv">-</div></div><div><div class="mid" id="a-w"></div><div class="mut" id="a-hz"></div></div></div>
   <canvas id="a-bars" height="150"></canvas></div>
  <div class="card"><div class="row"><div><div class="lab">peak</div><div class="mid" id="a-pk"></div></div><div><div class="lab">max</div><div class="mid" id="a-mx"></div></div><div><div class="lab">average</div><div class="mid" id="a-avg"></div></div></div>
   <canvas id="a-h" height="90"></canvas></div>
 </div>
</section>

<section id="p-sys"><div class="grid">
 <div class="card"><div class="lab">battery</div><div class="big" id="y-soc">-</div><div class="kv" id="y-bat"></div></div>
 <div class="card"><div class="lab">device</div><div class="kv" id="y-dev"></div></div>
</div></section>

<section id="p-upd"><div class="card">
 <div class="lab">firmware update over wi-fi</div>
 <div class="kv" id="u-ver" style="margin:8px 0 14px"></div>
 <div class="mut" style="margin-bottom:10px">Choose the firmware file (the same .bin you flash over USB, or firmware.bin). The device restarts by itself.
 If the new firmware does not start, the old one comes back automatically.</div>
 <input type="file" id="u-f" accept=".bin" style="margin-bottom:10px;color:var(--steel)"><br>
 <button id="u-go" class="x" style="float:none" onclick="upload()">UPLOAD</button>
 <div class="bar" style="width:100%;height:10px;margin-top:14px"><i id="u-bar" style="width:0%"></i></div>
 <div id="u-st" class="mut" style="margin-top:8px"></div>
</div></section>
</main>
<div id="modal" class="hide" onclick="if(event.target==this)closeM()"><div class="card" id="mc"></div></div>
<script>
const $=id=>document.getElementById(id),dp=()=>window.devicePixelRatio||1;
const TABS=[['ov','OVERVIEW'],['ble','BLUETOOTH'],['wifi','WI-FI'],['sub','SUB-GHZ'],['snd','SOUND'],['sys','SYSTEM'],['upd','UPDATE']];
let cur='ov',timer=null,bd=[],wd=[],sens=[],bhist=[],rhist={},bs={k:'r',d:-1},ws={k:'r',d:-1},sub={m:0,b:1},ahist=[],detId=null;
const esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const pct=r=>Math.max(0,Math.min(100,(r+95)*2));
const ago=s=>s<60?s+'s':s<3600?Math.floor(s/60)+'m':Math.floor(s/3600)+'h '+Math.floor(s/60)%60+'m';
const heat=x=>{x=Math.max(0,Math.min(1,x));const a=[[70,96,107],[63,217,255],[234,252,255]];const i=x<.6?0:1,t=x<.6?x/.6:(x-.6)/.4;const c=a[i].map((v,k)=>Math.round(v+(a[i+1][k]-v)*t));return`rgb(${c})`};
async function J(u){const r=await fetch(u+(u.includes('?')?'&':'?')+'tab='+cur,{cache:'no-store'});return r.json()}
function sizeC(c,h){const W=c.width=c.clientWidth*dp(),H=c.height=h*dp();const g=c.getContext('2d');g.clearRect(0,0,W,H);return[g,W,H]}

$('nav').innerHTML=TABS.map(([k,n])=>`<button id="n-${k}" onclick="go('${k}')">${n}</button>`).join('');
// опрос: следующий запрос — только после ответа на предыдущий (не копятся в очередь)
let gen=0;
function go(t){cur=t;TABS.forEach(([k])=>{$('n-'+k).className=k==t?'on':'';$('p-'+k).classList.toggle('hide',k!=t)});
 $('n-'+t).scrollIntoView({inline:'nearest',block:'nearest'});const f={ov:ov,ble:ble,wifi:wifi,sub:subP,snd:snd,sys:sys,upd:ver}[t];
 const ms={ov:3000,ble:2000,wifi:5000,sub:60,snd:60,sys:3000,upd:600000}[t];const my=++gen;
 (async function loop(){while(my==gen){const t0=Date.now();await f();const w=ms-(Date.now()-t0);await new Promise(r=>setTimeout(r,Math.max(w,10)))}})()}

// ---------- шапка ----------
async function info(){try{const j=await J('/api/info');const b=j.bat;$('bat').textContent=b.ok?'PWR '+b.soc+'%':'';
 const s=j.up;$('up').textContent='T+'+[s/3600|0,(s/60|0)%60,s%60].map(v=>String(v).padStart(2,'0')).join(':');return j}catch(e){}}

// ---------- обзор ----------
async function ov(){try{const [b,i]=await Promise.all([J('/api/ble'),info()]);bd=b.dev;
 $('o-b').textContent=b.on?b.count:'off';const tr=bd.filter(d=>d.t=='tracker').length;$('o-b2').innerHTML=tr?`<span class="alert">${tr} tracker(s) nearby</span>`:b.macs+' addresses';
 $('o-w').textContent=i.nets||'…';$('o-w2').textContent=i.nets?'last scan':'scanning';
 $('o-p').textContent=i.bat.ok?i.bat.soc+'%':'n/a';$('o-p2').textContent=i.bat.ok?(i.bat.mv/1000).toFixed(2)+' V · '+(i.bat.usb?'USB':'battery'):'';
 $('o-d').textContent=ago(i.up);$('o-d2').textContent=i.cli+' connected · chip '+i.chip+'°C'}catch(e){}}

// ---------- bluetooth ----------
function sorter(st){return(a,b)=>{let x=a[st.k],y=b[st.k];return(typeof x=='string'?x.localeCompare(y):x-y)*st.d}}
function heads(sec,st,fn){document.querySelectorAll('#'+sec+' th[data-k]').forEach(th=>th.onclick=()=>{const k=th.dataset.k;st.d=st.k==k?-st.d:-1;st.k=k;fn()})}
function line(c,a,h,col,min,max){const[g,W,H]=sizeC(c,h);if(a.length<2)return;min=min??Math.min(...a);max=max??Math.max(...a);if(max==min)max=min+1;
 g.strokeStyle=col;g.lineWidth=2*dp();g.beginPath();a.forEach((v,i)=>{const x=i/(a.length-1)*W,y=H-4*dp()-(v-min)/(max-min)*(H-12*dp());i?g.lineTo(x,y):g.moveTo(x,y)});g.stroke();
 g.fillStyle='#7f97a3';g.font=10*dp()+'px monospace';g.fillText(Math.round(max),3,11*dp());g.fillText(Math.round(min),3,H-3)}
function drawB(){$('bl').innerHTML=[...bd].sort(sorter(bs)).map(d=>{const al=d.t=='tracker';
 return`<tr class="cl" onclick="openDev(${d.id})"><td><div class="bar"><i style="width:${pct(d.r)}%;${al?'background:#ff8a3d':''}"></i></div><span class="dim">${d.r}</span></td><td class="n${al?' alert':''}">${esc(d.n||'(no name)')}<br><span class="dim">${esc(d.v)}${d.m>1?' · '+d.m+' addr':''}</span></td><td class="${al?'alert':'mut'}">${esc(d.t)}</td><td class="dim">${ago(d.age)}</td></tr>`}).join('')}
async function ble(){try{const j=await J('/api/ble');bd=j.dev;sens=j.sens;info();
 $('bc').textContent=j.on?j.count:'off';$('bm').textContent=j.macs+' addresses';bhist.push(j.count);if(bhist.length>150)bhist.shift();line($('bh'),bhist,70,'#3fd9ff',0);
 bd.forEach(d=>{(rhist[d.id]=rhist[d.id]||[]).push(d.r);if(rhist[d.id].length>90)rhist[d.id].shift()});
 const t={};bd.forEach(d=>t[d.t]=(t[d.t]||0)+1);$('bt').innerHTML=Object.entries(t).sort((a,b)=>b[1]-a[1]).map(([k,v])=>`<span class="chip ${k=='tracker'?'alert':''}">${esc(k)} <b>${v}</b></span>`).join('');
 const tr=bd.filter(d=>d.t=='tracker');$('trk').classList.toggle('hide',!tr.length);
 $('trk').innerHTML=tr.length?`⚠ ${tr.length} tracker tag(s) nearby — `+tr.map(d=>`${esc(d.v)} ${d.r} dBm, near ${ago(d.age)}`).join('; '):'';
 $('sens').classList.toggle('hide',!sens.length);
 $('sl').innerHTML=sens.map(z=>`<div style="padding:8px 0;border-bottom:1px solid #0b1f26"><b>${esc(z.n)}</b> <span class="dim">${esc(z.mo)} · ${z.r} dBm · ${ago(z.ago)} ago</span><br><span class="mid">${z.t!=null?z.t+'°C':''}</span> <span class="mut">${z.h!=null?z.h+'% ':''}${z.p!=null?z.p+' hPa ':''}${z.co2!=null?z.co2+' ppm ':''}${z.b!=null?'· battery '+z.b+'%':''}</span></div>`).join('');
 drawB();if(detId!=null)openDev(detId,true)}catch(e){}}
async function openDev(id,refresh){detId=id;try{const d=await J('/api/bledev?id='+id);if(detId!=id)return;
 const kv=[['type',d.t],['vendor',d.v],['signal',d.r+' dBm (min '+d.rmin+', max '+d.rmax+')'],['near',ago(d.age||0)],['packets',d.pk],d.cid?['company id',d.cid]:null,d.app?['appearance',d.app]:null,d.mfr?['mfr data',d.mfr]:null].filter(Boolean);
 $('mc').innerHTML=`<button class="x" onclick="closeM()">close</button><div class="lab">device</div><div class="mid">${esc(d.n||'(no name)')}</div>
 <canvas id="dsp" height="70" style="margin:10px 0"></canvas><div class="kv">${kv.map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('')}</div>
 ${d.uu&&d.uu.length?`<div class="lab" style="margin-top:12px">services</div>`+d.uu.map(u=>`<div>${u.u} <span class="dim">${esc(u.o)}</span></div>`).join(''):''}
 <div class="lab" style="margin-top:12px">addresses (phones change them)</div>${(d.addr||[]).map(a=>`<div>${a.mac} <span class="dim">${a.kind} · ${a.r} dBm · ${ago(a.ago)} ago</span></div>`).join('')}`;
 $('modal').classList.remove('hide');line($('dsp'),rhist[id]||[],70,'#3fd9ff',-100,-30)}catch(e){}}
function closeM(){detId=null;$('modal').classList.add('hide')}

// ---------- wi-fi ----------
function drawW(){$('wl').innerHTML=[...wd].sort(sorter(ws)).map(n=>`<tr><td><div class="bar"><i style="width:${pct(n.r)}%"></i></div><span class="dim">${n.r}</span></td><td class="n">${esc(n.s||'(hidden)')} <span class="${n.e=='open'?'alert':'dim'}">${esc(n.e)}</span><br><span class="dim">${n.v?esc(n.v)+' · ':''}${esc(n.b)}</span></td><td>${n.c}</td></tr>`).join('');
 const[g,W,H]=sizeC($('wch'),120),d=dp(),cw=W/15;g.fillStyle='#46606b';g.font=10*d+'px monospace';for(let ch=1;ch<=13;ch++)g.fillText(ch,(ch+.5)*cw-3*d,H-2*d);
 wd.forEach(n=>{if(n.c>13)return;const x=(n.c+.5)*cw,h=pct(n.r)/100*(H-24*d),w=2*cw;g.strokeStyle=n.e=='open'?'#ff8a3d':'rgba(63,217,255,.8)';g.lineWidth=1.5*d;g.beginPath();g.moveTo(x-w,H-14*d);g.quadraticCurveTo(x,H-14*d-h*2,x+w,H-14*d);g.stroke()});
 const cnt=Array(14).fill(0);wd.forEach(n=>{for(let k=n.c-2;k<=n.c+2;k++)if(k>0&&k<14)cnt[k]++});const best=[1,6,11].sort((a,b)=>cnt[a]-cnt[b])[0];$('wb').textContent=wd.length?'best channel: '+best:''}
async function wifi(){try{const j=await J('/api/wifi');wd=j.nets;$('wc').textContent=j.scanned?wd.length:'scanning…';drawW();info()}catch(e){}}

// ---------- sub-ghz ----------
const BANDS=['315','433','868','915'];let wfImg=null,lastMode=-1,lastBand=-1;
function subUI(){$('sm').innerHTML=['SPECTRUM','MONITOR'].map((n,i)=>`<button class="${sub.m==i?'on':''}" onclick="sub.m=${i};subUI()">${n}</button>`).join('');
 $('sb').innerHTML=BANDS.map((n,i)=>`<button class="${sub.b==i?'on':''}" onclick="sub.b=${i};subUI()">${n} MHz</button>`).join('');
 $('s-sp').classList.toggle('hide',sub.m!=0);$('s-mo').classList.toggle('hide',sub.m!=1)}
async function subP(){try{const j=await J(`/api/sub?m=${sub.m}&b=${sub.b}`);if(cur!='sub')return;$('s-none').classList.toggle('hide',j.ok);if(!j.ok)return;
 if(j.mode!=lastMode||j.band!=lastBand){wfImg=null;lastMode=j.mode;lastBand=j.band}
 if(j.mode==0&&j.cur){$('s-rng').textContent=j.lo+' – '+j.hi+' MHz';const[g,W,H]=sizeC($('spc'),140),d=dp(),n=j.cur.length,y=v=>H-((v+120)/90)*(H-8*d);
  g.strokeStyle='#12303a';for(let v=-110;v<=-40;v+=10){g.beginPath();g.moveTo(0,y(v));g.lineTo(W,y(v));g.stroke()}
  g.fillStyle='rgba(63,217,255,.25)';g.beginPath();g.moveTo(0,H);j.cur.forEach((v,i)=>g.lineTo(i/(n-1)*W,y(v)));g.lineTo(W,H);g.fill();
  g.strokeStyle='#3fd9ff';g.lineWidth=1.5*d;g.beginPath();j.cur.forEach((v,i)=>{const x=i/(n-1)*W;i?g.lineTo(x,y(v)):g.moveTo(x,y(v))});g.stroke();
  g.strokeStyle='#eafcff';g.lineWidth=1*d;g.setLineDash([3*d,3*d]);g.beginPath();j.peak.forEach((v,i)=>{const x=i/(n-1)*W;i?g.lineTo(x,y(v)):g.moveTo(x,y(v))});g.stroke();g.setLineDash([]);
  g.fillStyle='#7f97a3';g.font=10*d+'px monospace';g.fillText('-40',3,y(-40)+10*d);g.fillText('-110',3,y(-110)-3*d);
  const c=$('wf'),wg=c.getContext('2d');if(!wfImg){c.width=n;c.height=80;wfImg=true;wg.fillStyle='#02090b';wg.fillRect(0,0,n,80)}
  wg.drawImage(c,0,0,n,79,0,1,n,79);const row=wg.createImageData(n,1);j.cur.forEach((v,i)=>{const x=Math.max(0,Math.min(1,(v+105)/55));const m=heat(x).match(/\d+/g);const k=x<.08?.15:1;row.data.set([m[0]*k,m[1]*k,m[2]*k,255],i*4)});wg.putImageData(row,0,0);
  c.style.height='160px';c.style.imageRendering='pixelated'}
 if(j.mode==1&&j.rssi){$('s-ev').textContent=j.events;$('s-st').innerHTML=j.burst?'<span class="alert">SIGNAL!</span>':'quiet';$('s-fl').textContent='noise floor '+j.floor+' dBm · '+j.mon+' MHz';
  const[g,W,H]=sizeC($('mon'),120),d=dp(),n=j.rssi.length,y=v=>H-((v+120)/90)*(H-6*d);g.strokeStyle='#46606b';g.setLineDash([4*d,4*d]);g.beginPath();g.moveTo(0,y(j.floor+12));g.lineTo(W,y(j.floor+12));g.stroke();g.setLineDash([]);
  g.strokeStyle='#3fd9ff';g.lineWidth=1.5*d;g.beginPath();j.rssi.forEach((v,i)=>{const x=i/(Math.max(n,120)-1)*W;i?g.lineTo(x,y(v)):g.moveTo(x,y(v))});g.stroke();
  $('s-tb').innerHTML=j.ev.map(e=>`<tr><td>${ago(e.ago)} ago</td><td>${e.ms} ms</td><td>${e.pk} dBm</td></tr>`).join('')||'<tr><td colspan="3" class="dim">press a car key, doorbell or gate remote nearby</td></tr>'}
}catch(e){}}

// ---------- sound ----------
async function snd(){try{const j=await J('/api/snd');if(cur!='snd')return;$('a-on').classList.toggle('hide',!j.ok);$('a-off').classList.toggle('hide',j.ok);
 if(!j.en){$('a-msg').textContent='microphone is off — turn it on on the device: Settings > Sound > Microphone';return}
 if(!j.ok){$('a-msg').textContent='starting microphone…';return}
 $('a-lv').textContent=Math.round(j.lv)+' dBFS';$('a-w').innerHTML=j.lv>=-25?`<span class="alert">${j.w}</span>`:j.w;$('a-hz').textContent=j.e>.15?'loudest '+(j.hz>=1000?(j.hz/1000).toFixed(1)+' kHz':j.hz+' Hz'):'';
 $('a-pk').textContent=Math.round(j.pk);$('a-mx').textContent=Math.round(j.mx);$('a-avg').textContent=Math.round(j.avg);
 const[g,W,H]=sizeC($('a-bars'),150),d=dp(),n=j.b.length,bw=W/n;j.b.forEach((v,i)=>{const h=v/100*(H-4*d);g.fillStyle=heat(.2+v/120);g.fillRect(i*bw+1*d,H-h,bw-2*d,h)});
 ahist.push(j.lv);if(ahist.length>1200)ahist.shift();line($('a-h'),ahist,90,'#3fd9ff',-90,0)}catch(e){}}

// ---------- system ----------
async function sys(){const j=await info();if(!j)return;const b=j.bat;$('y-soc').textContent=b.ok?b.soc+'%':'n/a';
 const st=['not charging','pre-charge','charging','full'][b.chg]||'';const kv=b.ok?[['voltage',(b.mv/1000).toFixed(2)+' V'],['current',(b.ma>0?'+':'')+b.ma+' mA'],['temperature',b.t+' °C'],
 ['capacity',b.rem+' / '+b.full+' mAh'],['power',b.usb?'USB · '+st:'battery'],b.tte>0?['time left','~'+Math.floor(b.tte/60)+'h '+b.tte%60+'m']:null,b.ttf>0?['until full','~'+Math.floor(b.ttf/60)+'h '+b.ttf%60+'m']:null].filter(Boolean):[];
 $('y-bat').innerHTML=kv.map(([k,v])=>`<span>${k}</span><span>${v}</span>`).join('');
 $('y-dev').innerHTML=[['uptime',ago(j.up)],['chip temp',j.chip+' °C'],['free memory',j.heap+' KB'],['free PSRAM',j.psram+' KB'],['connected',j.cli],['network',j.ssid],['firmware',j.build]].map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('')}

// ---------- update ----------
async function ver(){try{const j=await J('/api/ver');$('u-ver').innerHTML=[['running',j.build],['slot',j.slot],['max size',(j.max/1048576).toFixed(1)+' MB']].map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('')}catch(e){}}
function upload(){const f=$('u-f').files[0];if(!f){$('u-st').textContent='choose a .bin file first';return}
 if(!confirm('Upload '+f.name+' ('+(f.size/1048576).toFixed(2)+' MB) and restart the device?'))return;
 gen++;$('u-go').disabled=true;const fd=new FormData();fd.append('fw',f,f.name);const x=new XMLHttpRequest();x.open('POST','/update');
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$('u-bar').style.width=p+'%';$('u-st').textContent='uploading '+p+'%'}};
 x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
  if(j.ok){$('u-bar').style.width='100%';$('u-st').innerHTML='done — device is restarting. Reconnect to its Wi-Fi in ~20 s and reload this page.'}
  else{$('u-st').innerHTML='<span class="alert">failed: '+esc(j.err||x.status)+'</span> — old firmware kept';$('u-go').disabled=false;go('upd')}};
 x.onerror=()=>{$('u-st').innerHTML='<span class="alert">connection lost</span> — if the device restarted, reconnect and check the version';$('u-go').disabled=false};
 $('u-st').textContent='uploading…';x.send(fd)}
heads('p-ble',bs,drawB);heads('p-wifi',ws,drawW);subUI();go('ov');
</script></body></html>)HTML";
