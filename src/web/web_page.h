// ============================================================
//  web_page.h — лёгкая страница TENGAN для телефона (/lite).
//  Вся страница — один файл: разметка, стиль и скрипт. Меню из шести
//  вкладок; каждая сама спрашивает у устройства свои данные (/api/...).
//  Язык — как у прошивки: <html lang> подставляется при сборке, тексты — L("en","ru").
//  Вкладки Sub-GHz и Sound включают CC1101 / микрофон, пока открыты.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/lang.h"

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang=")HTML" LANG_CODE R"HTML("><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TENGAN</title>
<style>
:root{--bg:#02090b;--panel:#071419;--line:#12303a;--acc:#3fd9ff;--hot:#eafcff;--steel:#7f97a3;--dim:#46606b;--alert:#ff8a3d;--row:#0b1f26;--hov:#0a1d24;--track:#0b2630;--accA8:rgba(63,217,255,.8);--accA2:rgba(63,217,255,.25);--onacc:#02090b;--fh:inherit}
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
td{padding:7px 4px;border-bottom:1px solid var(--row);vertical-align:middle}td.n{max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
tr.cl{cursor:pointer}tr.cl:hover td{background:var(--hov)}
.bar{height:6px;background:var(--track);width:64px;position:relative}.bar i{position:absolute;left:0;top:0;bottom:0;background:var(--acc)}
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
<header><div class="top"><h1>// TENGAN</h1><span class="sp"></span><a href="/full" style="color:var(--acc);text-decoration:none;border:1px solid var(--line);padding:4px 10px">FULL</a><span id="bat" class="mut"></span><span id="up" class="dim"></span></div>
<nav id="nav"></nav></header>
<main>
<section id="p-ov"><div class="grid">
 <div class="card clk" onclick="go('ble')"><div class="lab">bluetooth</div><div class="big" id="o-b">-</div><div class="mut" id="o-b2"></div></div>
 <div class="card clk" onclick="go('wifi')"><div class="lab" data-ru="сети wi-fi">wi-fi networks</div><div class="big" id="o-w">-</div><div class="mut" id="o-w2"></div></div>
 <div class="card clk" onclick="go('sub')"><div class="lab">sub-ghz</div><div class="mid" data-ru="433 МГц">433 MHz</div><div class="mut" data-ru="откройте, чтобы слушать">open to listen</div></div>
 <div class="card clk" onclick="go('snd')"><div class="lab" data-ru="звук">sound</div><div class="mid" data-ru="микрофон">mic</div><div class="mut" data-ru="откройте, чтобы слушать">open to listen</div></div>
 <div class="card clk" onclick="go('sys')"><div class="lab" data-ru="батарея">battery</div><div class="big" id="o-p">-</div><div class="mut" id="o-p2"></div></div>
 <div class="card clk" onclick="go('sys')"><div class="lab" data-ru="устройство">device</div><div class="mid" id="o-d">-</div><div class="mut" id="o-d2"></div></div>
</div></section>

<section id="p-ble">
 <div id="trk" class="banner hide"></div>
 <div class="card"><div class="row"><div><div class="lab" data-ru="устройств вокруг">devices around</div><div class="big" id="bc">-</div></div><div class="mut" id="bm"></div></div>
  <canvas id="bh" height="70"></canvas><div class="chips" id="bt"></div></div>
 <div class="card hide" id="sens"><div class="lab" data-ru="термометры">thermometers</div><div id="sl"></div></div>
 <div class="card"><table><thead><tr><th data-k="r" data-ru="СИГНАЛ">SIGNAL</th><th data-k="n" data-ru="УСТРОЙСТВО">DEVICE</th><th data-k="t" data-ru="ТИП">TYPE</th><th data-k="age" data-ru="РЯДОМ">NEAR</th></tr></thead><tbody id="bl"></tbody></table></div>
</section>

<section id="p-wifi">
 <div class="card"><div class="row"><div><div class="lab" data-ru="сети">networks</div><div class="big" id="wc">-</div></div><div class="mut" id="wb"></div></div><canvas id="wch" height="120"></canvas></div>
 <div class="card"><table><thead><tr><th data-k="r" data-ru="СИГНАЛ">SIGNAL</th><th data-k="s" data-ru="СЕТЬ">NETWORK</th><th data-k="c" data-ru="КАН">CH</th></tr></thead><tbody id="wl"></tbody></table></div>
</section>

<section id="p-sub">
 <div class="card"><div class="seg" id="sm"></div><div class="seg" id="sb"></div>
  <div id="s-none" class="empty hide" data-ru="радио CC1101 не найдено">CC1101 radio not found</div>
  <div id="s-sp"><div class="mut" id="s-rng"></div><canvas id="spc" height="140"></canvas><canvas id="wf" height="160"></canvas></div>
  <div id="s-mo" class="hide"><div class="row"><div><div class="lab" data-ru="поймано сигналов">signals caught</div><div class="big" id="s-ev">0</div></div>
   <div><div class="lab" data-ru="сейчас">now</div><div class="mid" id="s-st" data-ru="тихо">quiet</div></div><div class="mut" id="s-fl"></div></div>
   <canvas id="mon" height="120"></canvas><table><thead><tr><th data-ru="КОГДА">WHEN</th><th data-ru="ДЛИНА">LENGTH</th><th data-ru="ПИК">PEAK</th></tr></thead><tbody id="s-tb"></tbody></table></div>
 </div>
</section>

<section id="p-snd">
 <div class="card" id="a-off"><div class="empty" id="a-msg" data-ru="запускаю микрофон…">starting microphone…</div></div>
 <div id="a-on" class="hide">
  <div class="card"><div class="row"><div><div class="lab" data-ru="громкость">loudness</div><div class="big" id="a-lv">-</div></div><div><div class="mid" id="a-w"></div><div class="mut" id="a-hz"></div></div></div>
   <canvas id="a-bars" height="150"></canvas></div>
  <div class="card"><div class="row"><div><div class="lab" data-ru="пик">peak</div><div class="mid" id="a-pk"></div></div><div><div class="lab" data-ru="макс">max</div><div class="mid" id="a-mx"></div></div><div><div class="lab" data-ru="среднее">average</div><div class="mid" id="a-avg"></div></div></div>
   <canvas id="a-h" height="90"></canvas></div>
 </div>
</section>

<section id="p-sys"><div class="grid">
 <div class="card"><div class="lab" data-ru="батарея">battery</div><div class="big" id="y-soc">-</div><div class="kv" id="y-bat"></div></div>
 <div class="card"><div class="lab" data-ru="устройство">device</div><div class="kv" id="y-dev"></div></div>
</div></section>

<section id="p-upd"><div class="card">
 <div class="lab" data-ru="обновление прошивки по wi-fi">firmware update over wi-fi</div>
 <div class="kv" id="u-ver" style="margin:8px 0 14px"></div>
 <div class="mut" style="margin-bottom:10px" data-ru="Выберите файл прошивки (tengan_firmware_ota_ru.bin из релиза). Устройство перезапустится само; если новая прошивка не запустится, вернётся прежняя.">Choose the firmware file (tengan_firmware_ota_en.bin from the release). The device restarts by itself; if the new firmware does not start, the previous one comes back automatically.</div>
 <input type="file" id="u-f" accept=".bin" style="margin-bottom:10px;color:var(--steel)"><br>
 <button id="u-go" class="x" style="float:none" onclick="upload()" data-ru="ЗАГРУЗИТЬ">UPLOAD</button>
 <div class="bar" style="width:100%;height:10px;margin-top:14px"><i id="u-bar" style="width:0%"></i></div>
 <div id="u-st" class="mut" style="margin-top:8px"></div>
</div></section>
</main>
<div id="modal" class="hide" onclick="if(event.target==this)closeM()"><div class="card" id="mc"></div></div>
<script>
const $=id=>document.getElementById(id),dp=()=>window.devicePixelRatio||1;
const RU=document.documentElement.lang=='ru',L=(e,r)=>RU?r:e;   // язык прошивки
if(RU)document.querySelectorAll('[data-ru]').forEach(e=>e.textContent=e.dataset.ru);
const TABS=[['ov',L('OVERVIEW','ОБЗОР')],['ble','BLUETOOTH'],['wifi','WI-FI'],['sub','SUB-GHZ'],['snd',L('SOUND','ЗВУК')],['sys',L('SYSTEM','СИСТЕМА')],['upd',L('UPDATE','ОБНОВЛЕНИЕ')]];
let cur='ov',timer=null,bd=[],wd=[],sens=[],bhist=[],rhist={},bs={k:'r',d:-1},ws={k:'r',d:-1},sub={m:0,b:1},ahist=[],detId=null;
const esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const pct=r=>Math.max(0,Math.min(100,(r+95)*2));
const ago=s=>s<60?s+L('s',' с'):s<3600?Math.floor(s/60)+L('m',' мин'):Math.floor(s/3600)+L('h ',' ч ')+Math.floor(s/60)%60+L('m',' мин');
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
// цвета холстов берутся из CSS-переменных
const CV=n=>getComputedStyle(document.body).getPropertyValue('--'+n).trim();
function theme(t){}   // лёгкая страница — одна тема
async function info(){try{const j=await J('/api/info');theme(j.theme);const b=j.bat;$('bat').textContent=b.ok?L('BAT ','ЗАРЯД ')+b.soc+'%':'';
 const s=j.up;$('up').textContent='T+'+[s/3600|0,(s/60|0)%60,s%60].map(v=>String(v).padStart(2,'0')).join(':');return j}catch(e){}}

// ---------- обзор ----------
async function ov(){try{const [b,i]=await Promise.all([J('/api/ble'),info()]);bd=b.dev;
 $('o-b').textContent=b.on?b.count:L('off','выкл');const tr=bd.filter(d=>d.t=='tracker').length;$('o-b2').innerHTML=tr?`<span class="alert">${tr} ${L('tracker(s) nearby','трекер(ов) рядом')}</span>`:b.macs+L(' addresses',' адресов');
 $('o-w').textContent=i.nets||'…';$('o-w2').textContent=i.nets?L('last scan','последний поиск'):L('scanning','поиск');
 $('o-p').textContent=i.bat.ok?i.bat.soc+'%':L('n/a','нет');$('o-p2').textContent=i.bat.ok?(i.bat.mv/1000).toFixed(2)+L(' V · ',' В · ')+(i.bat.usb?'USB':L('battery','батарея')):'';
 $('o-d').textContent=ago(i.up);$('o-d2').textContent=i.cli+L(' connected · chip ',' подключено · чип ')+i.chip+'°C'}catch(e){}}

// ---------- bluetooth ----------
function sorter(st){return(a,b)=>{let x=a[st.k],y=b[st.k];return(typeof x=='string'?x.localeCompare(y):x-y)*st.d}}
function heads(sec,st,fn){document.querySelectorAll('#'+sec+' th[data-k]').forEach(th=>th.onclick=()=>{const k=th.dataset.k;st.d=st.k==k?-st.d:-1;st.k=k;fn()})}
function line(c,a,h,col,min,max){const[g,W,H]=sizeC(c,h);if(a.length<2)return;min=min??Math.min(...a);max=max??Math.max(...a);if(max==min)max=min+1;
 g.strokeStyle=col;g.lineWidth=2*dp();g.beginPath();a.forEach((v,i)=>{const x=i/(a.length-1)*W,y=H-4*dp()-(v-min)/(max-min)*(H-12*dp());i?g.lineTo(x,y):g.moveTo(x,y)});g.stroke();
 g.fillStyle=CV('steel');g.font=10*dp()+'px monospace';g.fillText(Math.round(max),3,11*dp());g.fillText(Math.round(min),3,H-3)}
function drawB(){$('bl').innerHTML=[...bd].sort(sorter(bs)).map(d=>{const al=d.t=='tracker';
 return`<tr class="cl" onclick="openDev(${d.id})"><td><div class="bar"><i style="width:${pct(d.r)}%;${al?'background:var(--alert)':''}"></i></div><span class="dim">${d.r}</span></td><td class="n${al?' alert':''}">${esc(d.n||L('(no name)','(без имени)'))}<br><span class="dim">${esc(d.v)}${d.m>1?' · '+d.m+' addr':''}</span></td><td class="${al?'alert':'mut'}">${esc(d.t)}</td><td class="dim">${ago(d.age)}</td></tr>`}).join('')}
async function ble(){try{const j=await J('/api/ble');bd=j.dev;sens=j.sens;info();
 $('bc').textContent=j.on?j.count:L('off','выкл');$('bm').textContent=j.macs+L(' addresses',' адресов');bhist.push(j.count);if(bhist.length>150)bhist.shift();line($('bh'),bhist,70,CV('acc'),0);
 bd.forEach(d=>{(rhist[d.id]=rhist[d.id]||[]).push(d.r);if(rhist[d.id].length>90)rhist[d.id].shift()});
 const t={};bd.forEach(d=>t[d.t]=(t[d.t]||0)+1);$('bt').innerHTML=Object.entries(t).sort((a,b)=>b[1]-a[1]).map(([k,v])=>`<span class="chip ${k=='tracker'?'alert':''}">${esc(k)} <b>${v}</b></span>`).join('');
 const tr=bd.filter(d=>d.t=='tracker');$('trk').classList.toggle('hide',!tr.length);
 $('trk').innerHTML=tr.length?`⚠ ${tr.length} ${L('tracker tag(s) nearby','трекер(ов) рядом')} — `+tr.map(d=>`${esc(d.v)} ${d.r} ${L('dBm, near','дБм, рядом')} ${ago(d.age)}`).join('; '):'';
 $('sens').classList.toggle('hide',!sens.length);
 $('sl').innerHTML=sens.map(z=>`<div style="padding:8px 0;border-bottom:1px solid var(--row)"><b>${esc(z.n)}</b> <span class="dim">${esc(z.mo)} · ${z.r} ${L('dBm','дБм')} · ${ago(z.ago)} ${L('ago','назад')}</span><br><span class="mid">${z.t!=null?z.t+'°C':''}</span> <span class="mut">${z.h!=null?z.h+'% ':''}${z.p!=null?z.p+' hPa ':''}${z.co2!=null?z.co2+' ppm ':''}${z.b!=null?'· battery '+z.b+'%':''}</span></div>`).join('');
 drawB();if(detId!=null)openDev(detId,true)}catch(e){}}
async function openDev(id,refresh){detId=id;try{const d=await J('/api/bledev?id='+id);if(detId!=id)return;
 const kv=[[L('type','тип'),d.t],[L('vendor','производитель'),d.v],[L('signal','сигнал'),d.r+L(' dBm (min ',' дБм (мин ')+d.rmin+L(', max ',', макс ')+d.rmax+')'],[L('near','рядом'),ago(d.age||0)],[L('packets','пакеты'),d.pk],d.cid?['company id',d.cid]:null,d.app?['appearance',d.app]:null,d.mfr?['mfr data',d.mfr]:null].filter(Boolean);
 $('mc').innerHTML=`<button class="x" onclick="closeM()">${L('close','закрыть')}</button><div class="lab">${L('device','устройство')}</div><div class="mid">${esc(d.n||L('(no name)','(без имени)'))}</div>
 <canvas id="dsp" height="70" style="margin:10px 0"></canvas><div class="kv">${kv.map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('')}</div>
 ${d.uu&&d.uu.length?`<div class="lab" style="margin-top:12px">${L('services','сервисы')}</div>`+d.uu.map(u=>`<div>${u.u} <span class="dim">${esc(u.o)}</span></div>`).join(''):''}
 <div class="lab" style="margin-top:12px">${L('addresses (phones change them)','адреса (телефоны их меняют)')}</div>${(d.addr||[]).map(a=>`<div>${a.mac} <span class="dim">${a.kind} · ${a.r} ${L('dBm','дБм')} · ${ago(a.ago)} ${L('ago','назад')}</span></div>`).join('')}`;
 $('modal').classList.remove('hide');line($('dsp'),rhist[id]||[],70,CV('acc'),-100,-30)}catch(e){}}
function closeM(){detId=null;$('modal').classList.add('hide')}

// ---------- wi-fi ----------
function drawW(){$('wl').innerHTML=[...wd].sort(sorter(ws)).map(n=>`<tr><td><div class="bar"><i style="width:${pct(n.r)}%"></i></div><span class="dim">${n.r}</span></td><td class="n">${esc(n.s||L('(hidden)','(скрыта)'))} <span class="${n.e=='open'?'alert':'dim'}">${esc(n.e)}</span><br><span class="dim">${n.v?esc(n.v)+' · ':''}${esc(n.b)}</span></td><td>${n.c}</td></tr>`).join('');
 const[g,W,H]=sizeC($('wch'),120),d=dp(),cw=W/15;g.fillStyle=CV('dim');g.font=10*d+'px monospace';for(let ch=1;ch<=13;ch++)g.fillText(ch,(ch+.5)*cw-3*d,H-2*d);
 wd.forEach(n=>{if(n.c>13)return;const x=(n.c+.5)*cw,h=pct(n.r)/100*(H-24*d),w=2*cw;g.strokeStyle=n.e=='open'?CV('alert'):CV('accA8');g.lineWidth=1.5*d;g.beginPath();g.moveTo(x-w,H-14*d);g.quadraticCurveTo(x,H-14*d-h*2,x+w,H-14*d);g.stroke()});
 const cnt=Array(14).fill(0);wd.forEach(n=>{for(let k=n.c-2;k<=n.c+2;k++)if(k>0&&k<14)cnt[k]++});const best=[1,6,11].sort((a,b)=>cnt[a]-cnt[b])[0];$('wb').textContent=wd.length?L('best channel: ','лучший канал: ')+best:''}
async function wifi(){try{const j=await J('/api/wifi');wd=j.nets;$('wc').textContent=j.scanned?wd.length:L('scanning…','поиск…');drawW();info()}catch(e){}}

// ---------- sub-ghz ----------
const BANDS=['315','433','868','915'];let wfImg=null,lastMode=-1,lastBand=-1;
function subUI(){$('sm').innerHTML=[L('SPECTRUM','СПЕКТР'),L('MONITOR','МОНИТОР')].map((n,i)=>`<button class="${sub.m==i?'on':''}" onclick="sub.m=${i};subUI()">${n}</button>`).join('');
 $('sb').innerHTML=BANDS.map((n,i)=>`<button class="${sub.b==i?'on':''}" onclick="sub.b=${i};subUI()">${n} ${L('MHz','МГц')}</button>`).join('');
 $('s-sp').classList.toggle('hide',sub.m!=0);$('s-mo').classList.toggle('hide',sub.m!=1)}
async function subP(){try{const j=await J(`/api/sub?m=${sub.m}&b=${sub.b}`);if(cur!='sub')return;$('s-none').classList.toggle('hide',j.ok);if(!j.ok)return;
 if(j.mode!=lastMode||j.band!=lastBand){wfImg=null;lastMode=j.mode;lastBand=j.band}
 if(j.mode==0&&j.cur){$('s-rng').textContent=j.lo+' – '+j.hi+L(' MHz',' МГц');const[g,W,H]=sizeC($('spc'),140),d=dp(),n=j.cur.length,y=v=>H-((v+120)/90)*(H-8*d);
  g.strokeStyle=CV('line');for(let v=-110;v<=-40;v+=10){g.beginPath();g.moveTo(0,y(v));g.lineTo(W,y(v));g.stroke()}
  g.fillStyle=CV('accA2');g.beginPath();g.moveTo(0,H);j.cur.forEach((v,i)=>g.lineTo(i/(n-1)*W,y(v)));g.lineTo(W,H);g.fill();
  g.strokeStyle=CV('acc');g.lineWidth=1.5*d;g.beginPath();j.cur.forEach((v,i)=>{const x=i/(n-1)*W;i?g.lineTo(x,y(v)):g.moveTo(x,y(v))});g.stroke();
  g.strokeStyle=CV('hot');g.lineWidth=1*d;g.setLineDash([3*d,3*d]);g.beginPath();j.peak.forEach((v,i)=>{const x=i/(n-1)*W;i?g.lineTo(x,y(v)):g.moveTo(x,y(v))});g.stroke();g.setLineDash([]);
  g.fillStyle=CV('steel');g.font=10*d+'px monospace';g.fillText('-40',3,y(-40)+10*d);g.fillText('-110',3,y(-110)-3*d);
  const c=$('wf'),wg=c.getContext('2d');if(!wfImg){c.width=n;c.height=80;wfImg=true;wg.fillStyle=CV('bg');wg.fillRect(0,0,n,80)}
  wg.drawImage(c,0,0,n,79,0,1,n,79);const row=wg.createImageData(n,1);j.cur.forEach((v,i)=>{const x=Math.max(0,Math.min(1,(v+105)/55));const m=heat(x).match(/\d+/g);const k=x<.08?.15:1;row.data.set([m[0]*k,m[1]*k,m[2]*k,255],i*4)});wg.putImageData(row,0,0);
  c.style.height='160px';c.style.imageRendering='pixelated'}
 if(j.mode==1&&j.rssi){$('s-ev').textContent=j.events;$('s-st').innerHTML=j.burst?'<span class="alert">'+L('SIGNAL!','СИГНАЛ!')+'</span>':L('quiet','тихо');$('s-fl').textContent=L('noise floor ','шум ')+j.floor+L(' dBm · ',' дБм · ')+j.mon+L(' MHz',' МГц');
  const[g,W,H]=sizeC($('mon'),120),d=dp(),n=j.rssi.length,y=v=>H-((v+120)/90)*(H-6*d);g.strokeStyle=CV('dim');g.setLineDash([4*d,4*d]);g.beginPath();g.moveTo(0,y(j.floor+12));g.lineTo(W,y(j.floor+12));g.stroke();g.setLineDash([]);
  g.strokeStyle=CV('acc');g.lineWidth=1.5*d;g.beginPath();j.rssi.forEach((v,i)=>{const x=i/(Math.max(n,120)-1)*W;i?g.lineTo(x,y(v)):g.moveTo(x,y(v))});g.stroke();
  $('s-tb').innerHTML=j.ev.map(e=>`<tr><td>${ago(e.ago)} ${L('ago','назад')}</td><td>${e.ms} ${L('ms','мс')}</td><td>${e.pk} ${L('dBm','дБм')}</td></tr>`).join('')||'<tr><td colspan="3" class="dim">'+L('press a car key, doorbell or gate remote nearby','нажмите рядом брелок, звонок или пульт ворот')+'</td></tr>'}
}catch(e){}}

// ---------- sound ----------
async function snd(){try{const j=await J('/api/snd');if(cur!='snd')return;$('a-on').classList.toggle('hide',!j.ok);$('a-off').classList.toggle('hide',j.ok);
 if(!j.en){$('a-msg').textContent=L('the microphone is off — turn it on on the device: Settings › Sound › Microphone','микрофон выключен — включите на устройстве: Настройки › Звук › Микрофон');return}
 if(!j.ok){$('a-msg').textContent=L('starting microphone…','запускаю микрофон…');return}
 $('a-lv').textContent=Math.round(j.lv)+' dBFS';$('a-w').innerHTML=j.lv>=-25?`<span class="alert">${j.w}</span>`:j.w;$('a-hz').textContent=j.e>.15?L('loudest ','громче всего ')+(j.hz>=1000?(j.hz/1000).toFixed(1)+L(' kHz',' кГц'):j.hz+L(' Hz',' Гц')):'';
 $('a-pk').textContent=Math.round(j.pk);$('a-mx').textContent=Math.round(j.mx);$('a-avg').textContent=Math.round(j.avg);
 const[g,W,H]=sizeC($('a-bars'),150),d=dp(),n=j.b.length,bw=W/n;j.b.forEach((v,i)=>{const h=v/100*(H-4*d);g.fillStyle=heat(.2+v/120);g.fillRect(i*bw+1*d,H-h,bw-2*d,h)});
 ahist.push(j.lv);if(ahist.length>1200)ahist.shift();line($('a-h'),ahist,90,CV('acc'),-90,0)}catch(e){}}

// ---------- system ----------
async function sys(){const j=await info();if(!j)return;const b=j.bat;$('y-soc').textContent=b.ok?b.soc+'%':L('n/a','нет');
 const st=(RU?['не заряжается','предзаряд','заряжается','заряжена']:['not charging','pre-charge','charging','full'])[b.chg]||'';const kv=b.ok?[[L('voltage','напряжение'),(b.mv/1000).toFixed(2)+L(' V',' В')],[L('current','ток'),(b.ma>0?'+':'')+b.ma+L(' mA',' мА')],[L('temperature','температура'),b.t+' °C'],
 [L('capacity','ёмкость'),b.rem+' / '+b.full+L(' mAh',' мА·ч')],[L('power','питание'),b.usb?'USB · '+st:L('battery','батарея')],b.tte>0?[L('time left','осталось'),'~'+Math.floor(b.tte/60)+L('h ',' ч ')+b.tte%60+L('m',' мин')]:null,b.ttf>0?[L('until full','до полной'),'~'+Math.floor(b.ttf/60)+L('h ',' ч ')+b.ttf%60+L('m',' мин')]:null].filter(Boolean):[];
 $('y-bat').innerHTML=kv.map(([k,v])=>`<span>${k}</span><span>${v}</span>`).join('');
 $('y-dev').innerHTML=[[L('uptime','работает'),ago(j.up)],[L('chip temp','температура чипа'),j.chip+' °C'],[L('free memory','свободно памяти'),j.heap+L(' KB',' КБ')],[L('free PSRAM','свободно PSRAM'),j.psram+L(' KB',' КБ')],[L('connected','подключено'),j.cli],[L('network','сеть'),j.ssid],[L('firmware','прошивка'),j.build]].map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('')}

// ---------- update ----------
async function ver(){try{const j=await J('/api/ver');$('u-ver').innerHTML=[[L('running','сейчас'),j.build],[L('slot','раздел'),j.slot],[L('max size','макс. размер'),(j.max/1048576).toFixed(1)+L(' MB',' МБ')]].map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('')}catch(e){}}
function upload(){const f=$('u-f').files[0];if(!f){$('u-st').textContent=L('choose a .bin file first','сначала выберите файл .bin');return}
 if(!confirm(L('Upload ','Загрузить ')+f.name+' ('+(f.size/1048576).toFixed(2)+L(' MB) and restart the device?',' МБ) и перезапустить устройство?')))return;
 gen++;$('u-go').disabled=true;const fd=new FormData();fd.append('fw',f,f.name);const x=new XMLHttpRequest();x.open('POST','/update');
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$('u-bar').style.width=p+'%';$('u-st').textContent=L('uploading ','загрузка ')+p+'%'}};
 x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
  if(j.ok){$('u-bar').style.width='100%';$('u-st').innerHTML=L('done — the device is restarting. Reconnect to its Wi-Fi in ~20 s and reload this page.','готово — устройство перезапускается. Через ~20 с снова подключитесь к его Wi-Fi и обновите страницу.')}
  else{$('u-st').innerHTML='<span class="alert">'+L('failed: ','ошибка: ')+esc(j.err||x.status)+'</span> — '+L('the previous firmware is kept','прежняя прошивка сохранена');$('u-go').disabled=false;go('upd')}};
 x.onerror=()=>{$('u-st').innerHTML='<span class="alert">'+L('connection lost','связь потеряна')+'</span> — '+L('if the device restarted, reconnect and check the version','если устройство перезапустилось, подключитесь снова и проверьте версию');$('u-go').disabled=false};
 $('u-st').textContent=L('uploading…','загрузка…');x.send(fd)}
heads('p-ble',bs,drawB);heads('p-wifi',ws,drawW);subUI();go('ov');
info();setInterval(info,8000);   // тема и батарея — на всех вкладках
</script></body></html>)HTML";
