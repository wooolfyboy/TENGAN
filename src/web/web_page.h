// ============================================================
//  web_page.h — страница, которую палка отдаёт телефону.
//  Вся страница — один файл: разметка, стиль и скрипт. Данные
//  она сама запрашивает у палки (/api/...) каждые пару секунд.
// ============================================================
#pragma once
#include <Arduino.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radio Radar</title>
<style>
:root{--bg:#02090b;--panel:#071419;--line:#12303a;--acc:#3fd9ff;--hot:#eafcff;--steel:#7f97a3;--dim:#46606b;--alert:#ff8a3d}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--hot);font:14px/1.35 ui-monospace,Menlo,Consolas,monospace}
header{position:sticky;top:0;background:var(--bg);border-bottom:1px solid var(--line);padding:10px 16px;display:flex;align-items:center;gap:12px;z-index:2}
h1{font-size:15px;margin:0;color:var(--acc);letter-spacing:.08em}.sp{flex:1}.mut{color:var(--steel)}.dim{color:var(--dim)}
nav{display:flex;gap:6px;padding:10px 16px 0}nav button{flex:1;background:var(--panel);color:var(--steel);border:1px solid var(--line);padding:9px;font:inherit;letter-spacing:.06em;cursor:pointer}
nav button.on{color:var(--bg);background:var(--acc);border-color:var(--acc)}
main{padding:12px 16px 40px;max-width:900px;margin:0 auto}
.card{background:var(--panel);border:1px solid var(--line);padding:12px;margin-bottom:12px;clip-path:polygon(0 0,calc(100% - 12px) 0,100% 12px,100% 100%,12px 100%,0 calc(100% - 12px))}
.lab{font-size:11px;color:var(--steel);letter-spacing:.1em;text-transform:uppercase}
.big{font-size:44px;font-weight:600;line-height:1}.row{display:flex;gap:16px;align-items:flex-end;flex-wrap:wrap}
canvas{width:100%;display:block}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.chip{border:1px solid var(--line);padding:3px 8px;font-size:12px;color:var(--steel)}.chip b{color:var(--hot);font-weight:600}
table{width:100%;border-collapse:collapse}table{table-layout:auto}th{font-size:11px;color:var(--steel);text-align:left;font-weight:400;letter-spacing:.08em;padding:6px 4px;border-bottom:1px solid var(--line);cursor:pointer;white-space:nowrap}
td{padding:7px 4px;border-bottom:1px solid #0b1f26;vertical-align:middle}td.n{max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.bar{height:6px;background:#0b2630;width:70px;position:relative}.bar i{position:absolute;left:0;top:0;bottom:0;background:var(--acc)}
.alert{color:var(--alert)}.hide{display:none}
</style></head><body>
<header><h1>// RADIO RADAR</h1><span class="sp"></span><span id="bat" class="mut"></span><span id="up" class="dim"></span></header>
<nav><button id="tb" class="on" onclick="tab('b')">BLUETOOTH</button><button id="tw" onclick="tab('w')">WI-FI</button></nav>
<main>
<section id="pb">
 <div class="card"><div class="row"><div><div class="lab">devices around</div><div class="big" id="bc">-</div></div>
  <div class="mut" id="bm"></div></div><canvas id="bh" height="70"></canvas><div class="chips" id="bt"></div></div>
 <div class="card"><table><thead><tr><th data-k="r">SIGNAL</th><th data-k="n">DEVICE</th><th data-k="t">TYPE</th><th data-k="age">SEEN</th></tr></thead><tbody id="bl"></tbody></table></div>
</section>
<section id="pw" class="hide">
 <div class="card"><div class="row"><div><div class="lab">networks</div><div class="big" id="wc">-</div></div><div class="mut" id="wb"></div></div>
  <canvas id="wch" height="120"></canvas></div>
 <div class="card"><table><thead><tr><th data-k="r">SIGNAL</th><th data-k="s">NETWORK</th><th data-k="c">CH</th></tr></thead><tbody id="wl"></tbody></table></div>
</section>
</main>
<script>
const $=id=>document.getElementById(id);let cur='b',hist=[],bs={k:'r',d:-1},ws={k:'r',d:-1},bd=[],wd=[];
function tab(t){cur=t;$('tb').className=t=='b'?'on':'';$('tw').className=t=='w'?'on':'';$('pb').className=t=='b'?'':'hide';$('pw').className=t=='w'?'':'hide';if(t=='w')wifi()}
const esc=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
const pct=r=>Math.max(0,Math.min(100,(r+95)*2));
const ago=s=>s<60?s+'s':s<3600?Math.floor(s/60)+'m':Math.floor(s/3600)+'h';
function sorter(st){return(a,b)=>{let x=a[st.k],y=b[st.k];return(typeof x=='string'?x.localeCompare(y):x-y)*st.d}}
function heads(id,st,fn){document.querySelectorAll('#'+id+' th').forEach(th=>th.onclick=()=>{const k=th.dataset.k;st.d=st.k==k?-st.d:-1;st.k=k;fn()})}
function spark(c,a){const g=c.getContext('2d'),W=c.width=c.clientWidth*devicePixelRatio,H=c.height=70*devicePixelRatio;g.clearRect(0,0,W,H);if(a.length<2)return;
 const mx=Math.max(...a,1);g.strokeStyle='#3fd9ff';g.lineWidth=2*devicePixelRatio;g.beginPath();a.forEach((v,i)=>{const x=i/(119)*W,y=H-4-(v/mx)*(H-12);i?g.lineTo(x,y):g.moveTo(x,y)});g.stroke();
 g.fillStyle='#7f97a3';g.font=11*devicePixelRatio+'px monospace';g.fillText('max '+mx,4,12*devicePixelRatio)}
function drawB(){const rows=[...bd].sort(sorter(bs));$('bl').innerHTML=rows.map(d=>{const al=d.t=='tracker';const nm=d.n||'(no name)';
 return`<tr><td><div class="bar"><i style="width:${pct(d.r)}%;background:${al?'#ff8a3d':''}"></i></div><span class="dim">${d.r}</span></td><td class="n${al?' alert':''}">${esc(nm)}<br><span class="dim">${esc(d.v)}${d.m>1?' · '+d.m+' addr':''}</span></td><td class="${al?'alert':'mut'}">${esc(d.t)}</td><td class="dim">${ago(d.age)}</td></tr>`}).join('')}
async function ble(){try{const j=await(await fetch('/api/ble')).json();bd=j.dev;$('bc').textContent=j.on?j.count:'off';$('bm').textContent=j.macs+' addresses';
 hist.push(j.count);if(hist.length>120)hist.shift();spark($('bh'),hist);const t={};bd.forEach(d=>t[d.t]=(t[d.t]||0)+1);
 $('bt').innerHTML=Object.entries(t).sort((a,b)=>b[1]-a[1]).map(([k,v])=>`<span class="chip ${k=='tracker'?'alert':''}">${esc(k)} <b>${v}</b></span>`).join('');drawB()}catch(e){}}
function drawW(){const rows=[...wd].sort(sorter(ws));$('wl').innerHTML=rows.map(n=>`<tr><td><div class="bar"><i style="width:${pct(n.r)}%"></i></div><span class="dim">${n.r}</span></td><td class="n">${esc(n.s||'(hidden)')} <span class="${n.e=='open'?'alert':'dim'}">${esc(n.e)}</span><br><span class="dim">${n.v?esc(n.v)+' · ':''}${esc(n.b)}</span></td><td>${n.c}</td></tr>`).join('');
 const c=$('wch'),g=c.getContext('2d'),dp=devicePixelRatio,W=c.width=c.clientWidth*dp,H=c.height=120*dp;g.clearRect(0,0,W,H);const cw=W/15;
 g.strokeStyle='#12303a';g.fillStyle='#46606b';g.font=10*dp+'px monospace';for(let ch=1;ch<=13;ch++){const x=(ch+.5)*cw;g.fillText(ch,x-3*dp,H-2*dp)}
 wd.forEach(n=>{if(n.c>13)return;const x=(n.c+.5)*cw,h=pct(n.r)/100*(H-24*dp),w=2*cw;g.strokeStyle=n.e=='open'?'#ff8a3d':'rgba(63,217,255,.8)';g.lineWidth=1.5*dp;g.beginPath();
  g.moveTo(x-w,H-14*dp);g.quadraticCurveTo(x,H-14*dp-h*2,x+w,H-14*dp);g.stroke()});
 const cnt=[0,0,0,0,0,0,0,0,0,0,0,0,0,0];wd.forEach(n=>{for(let k=n.c-2;k<=n.c+2;k++)if(k>0&&k<14)cnt[k]++});const best=[1,6,11].sort((a,b)=>cnt[a]-cnt[b])[0];
 $('wb').textContent=wd.length?'best channel: '+best:''}
async function wifi(){try{const j=await(await fetch('/api/wifi')).json();wd=j.nets;$('wc').textContent=j.scanned?wd.length:'scanning…';drawW()}catch(e){}}
async function info(){try{const j=await(await fetch('/api/info')).json();$('bat').textContent=j.bat>=0?'PWR '+j.bat+'%':'';const s=j.up;$('up').textContent='T+'+[s/3600|0,(s/60|0)%60,s%60].map(v=>String(v).padStart(2,'0')).join(':')}catch(e){}}
heads('pb',bs,drawB);heads('pw',ws,drawW);ble();info();setInterval(ble,2000);setInterval(info,5000);setInterval(()=>{if(cur=='w')wifi()},5000);
</script></body></html>)HTML";
