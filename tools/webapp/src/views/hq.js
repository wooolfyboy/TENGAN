// ---- ШТАБ 天眼司令室 (стиль «Евангелиона»): всё, что слышит палка, на одном экране ----
// Свой фиксированный стиль (оранжевый/красный на чёрном) — не зависит от темы DENKO/SUMI.
// Центр — «радиогоризонт»: BLE точками, Wi-Fi ромбами, датчики 433 треугольниками;
// ближе к центру = сильнее сигнал. По краям — счётчики, погода, звук, пульты/карты, лог.
(() => {
const E = {
  bg: '#050302', or: '#ff8c1a', or2: '#ffb347', dim: '#5a2e08', dk: '#2a1405', red: '#ff2a1f',
  grn: '#69ff6e', cy: '#5fe3ff', txt: '#ffcf99', wht: '#fff4e6',
};
// шрифты фиксированные: штаб выглядит одинаково в DENKO и SUMI
const RAJ = "Rajdhani,'Arial Narrow',sans-serif";
const JP = () => T.SEAL_FONT, FT = () => RAJ, FM = () => T.FM, FC = () => T.FC;
const font = (g, px, f, w = '') => { g.font = `${w}${px}px ${f}`; };
const tx = (g, s, x, y, px, col, f = FM(), al = 'left', w = '') => { font(g, px, f, w); g.fillStyle = col; g.textAlign = al; g.textBaseline = 'alphabetic'; g.fillText(s, x, y); };
// «жирный» японский: системный шрифт печати один, толщину добираем обводкой
const jp = (g, s, x, y, px, col, al = 'left', bold = 0.06) => {
  font(g, px, JP()); g.textAlign = al; g.textBaseline = 'alphabetic';
  g.lineJoin = 'round'; g.lineWidth = px * bold; g.strokeStyle = col; g.strokeText(s, x, y);
  g.fillStyle = col; g.fillText(s, x, y);
};
const pad2 = n => String(n).padStart(2, '0');
const hms = d => pad2(d.getHours()) + ':' + pad2(d.getMinutes()) + ':' + pad2(d.getSeconds());
// возраст Луны (дни) — без интернета, достаточно для подписи
const moonAge = () => { const syn = 29.530588853, ref = Date.UTC(2000, 0, 6, 18, 14) / 864e5; let a = (Date.now() / 864e5 - ref) % syn; if (a < 0) a += syn; return a; };
// полосатая лента «осторожно»
const stripes = (g, x, y, w, h, col, step) => {
  g.save(); g.beginPath(); g.rect(x, y, w, h); g.clip();
  g.fillStyle = '#000'; g.fillRect(x, y, w, h); g.fillStyle = col;
  for (let k = -h; k < w + h; k += step * 2) { g.beginPath(); g.moveTo(x + k, y + h); g.lineTo(x + k + step, y + h); g.lineTo(x + k + step + h, y); g.lineTo(x + k + h, y); g.closePath(); g.fill(); }
  g.restore();
};
// панель: рамка + оранжевая шапка (EN слева, иероглифы справа) + срезанный уголок
const panel = (g, x, y, w, h, u, en, kj, right, hot) => {
  const hh = u * 1.15, col = hot ? E.red : E.or;
  g.fillStyle = 'rgba(255,140,26,.045)'; g.fillRect(x, y, w, h);
  g.strokeStyle = col; g.lineWidth = Math.max(1, u * 0.07); g.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
  g.fillStyle = col; g.fillRect(x, y, w, hh);
  tx(g, en, x + u * 0.5, y + hh * 0.78, u * 0.8, '#000', FT(), 'left', '600 ');
  let rx = x + w - u * 0.5;
  if (kj) { jp(g, kj, rx, y + hh * 0.8, u * 0.78, '#000', 'right', 0.03); font(g, u * 0.78, JP()); rx -= g.measureText(kj).width + u * 0.6; }
  if (right) tx(g, right, rx, y + hh * 0.78, u * 0.72, '#000', FT(), 'right', '600 ');
  g.beginPath(); g.moveTo(x + w, y + h - u); g.lineTo(x + w, y + h); g.lineTo(x + w - u, y + h); g.closePath(); g.fillStyle = col; g.fill();
  return y + hh;
};
const blink = (t, hz = 1) => (t * hz) % 1 < 0.6;

// ================== стиль DOMAIN 天眼領域 («магическая битва»: тушь, манга, талисманы) ==================
const D = {
  bg: '#07060c', ink: '#0d0b17', blue: '#4f7bff', vio: '#9d6bff', glow: '#7fb2ff', red: '#e3212b',
  paper: '#efe6d2', txt: '#d9d4ef', dim: '#3a3560', mute: '#8a85b0',
};
// ранги по силе сигнала
const GRADES = [[-45, '特級', 'SPECIAL', D.red], [-60, '一級', 'GRADE 1', D.vio], [-75, '二級', 'GRADE 2', D.blue], [-999, '三級', 'GRADE 3', D.mute]];
const gradeOf = rs => GRADES.findIndex(q => rs > q[0]);
// растр-скринтон (кэш узоров на холст)
const tones = new WeakMap();
const tone = (g, rgba, step) => {
  let m = tones.get(g); if (!m) { m = new Map(); tones.set(g, m); }
  const k = rgba + step; let p = m.get(k);
  if (!p) { const c = document.createElement('canvas'); c.width = c.height = step; const q = c.getContext('2d'); q.fillStyle = rgba; q.beginPath(); q.arc(step / 2, step / 2, step * 0.15, 0, T.TAU); q.fill(); p = g.createPattern(c, 'repeat'); m.set(k, p); }
  return p;
};
// свечение-ореол точки: спрайт на цвет
const auras = new Map();
const aura = col => {
  let c = auras.get(col); if (c) return c;
  c = document.createElement('canvas'); c.width = c.height = 64; const q = c.getContext('2d');
  const n = parseInt(col.slice(1), 16), rgb = `${n >> 16},${(n >> 8) & 255},${n & 255}`;
  const gr = q.createRadialGradient(32, 32, 0, 32, 32, 32); gr.addColorStop(0, `rgba(${rgb},.75)`); gr.addColorStop(0.4, `rgba(${rgb},.3)`); gr.addColorStop(1, `rgba(${rgb},0)`);
  q.fillStyle = gr; q.fillRect(0, 0, 64, 64); auras.set(col, c); return c;
};
// панель-кадр манги: косой край, двойная рамка, подпись на чёрной плашке
const mpanel = (g, x, y, w, h, u, cap, opt = {}) => {
  const sl = opt.slant || 0;
  g.save();
  g.beginPath(); g.moveTo(x, y + (sl < 0 ? -sl * h : 0)); g.lineTo(x + w, y + (sl > 0 ? sl * h : 0)); g.lineTo(x + w, y + h); g.lineTo(x, y + h); g.closePath();
  g.fillStyle = D.ink; g.fill();
  if (opt.tone) { g.fillStyle = tone(g, opt.tone, Math.max(6, Math.round(u * 0.42))); g.fill(); }
  g.lineWidth = u * 0.16; g.strokeStyle = '#000'; g.stroke();
  g.clip();
  g.strokeStyle = D.txt; g.lineWidth = Math.max(1, u * 0.07); g.strokeRect(x + u * 0.35, y + u * 0.35 + (sl ? Math.abs(sl) * h : 0), w - u * 0.7, h - u * 0.7 - (sl ? Math.abs(sl) * h : 0));
  g.restore();
  if (cap) { font(g, u * 0.78, FT(), '600 '); const cw = g.measureText(cap).width + u * 1.2, cy = y + u * 0.8 + (sl ? Math.abs(sl) * h : 0);
    g.fillStyle = '#000'; g.fillRect(x + u * 0.7, cy, cw, u * 1.2); tx(g, cap, x + u * 1.3, cy + u * 0.9, u * 0.78, D.paper, FT(), 'left', '600 '); return cy + u * 1.2; }
  return y;
};
// бумажный талисман офуда
const ofuda = (g, x, y, w, h, u, rot, kj, seal, lines, fresh) => {
  g.save(); g.translate(x + w / 2, y + h / 2); g.rotate(rot); g.translate(-w / 2, -h / 2);
  g.fillStyle = '#000'; g.fillRect(u * 0.35, u * 0.35, w, h);
  g.fillStyle = D.paper; g.fillRect(0, 0, w, h); g.strokeStyle = '#111'; g.lineWidth = u * 0.1; g.strokeRect(0, 0, w, h);
  if (fresh > 0) { g.fillStyle = `rgba(227,33,43,${0.25 * fresh})`; g.fillRect(0, 0, w, h); }
  // вертикальные иероглифы справа
  const kp = Math.min(u * 2.1, h / (kj.length + 0.6));
  for (let i = 0; i < kj.length; i++) jp(g, kj[i], w - kp * 0.6, kp * (1.05 + i), kp, '#111', 'center', 0.05);
  const tw = w - kp * 1.4;
  let yy = u * 1.3;
  for (const [s, px, col, f] of lines) { g.font = `${f === 't' ? '600 ' : ''}${px}px ${f === 't' ? FT() : FM()}`; const str = T.fit(g, s, tw - u * 0.6); tx(g, str, u * 0.6, yy, px, col, f === 't' ? FT() : FM(), 'left', f === 't' ? '600 ' : ''); yy += px * 1.25; }
  // красная печать
  const ss = Math.min(u * 2.4, w * 0.3);
  g.save(); g.translate(u * 0.7 + ss / 2, h - u * 0.7 - ss / 2); g.rotate(-0.14);
  g.strokeStyle = D.red; g.lineWidth = u * 0.16; g.strokeRect(-ss / 2, -ss / 2, ss, ss);
  jp(g, seal[0], 0, -ss * 0.04, ss * 0.42, D.red, 'center', 0.02); jp(g, seal[1], 0, ss * 0.42, ss * 0.42, D.red, 'center', 0.02);
  g.restore();
  g.restore();
};
// молния-трещина 黒閃
const crack = (g, pts, col, lw) => { g.strokeStyle = col; g.lineWidth = lw; g.lineJoin = 'miter'; g.beginPath(); pts.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1])); g.stroke(); };
// глаз домена — шейдер (WebGL); вокруг — дымный ореол и тёмное ядро
const EYE_FS = () => `precision highp float;uniform vec2 res;uniform float t;uniform float open;uniform float red;uniform float flare;uniform vec2 look;
${T.GL.NOISE}
float fq3(vec3 p){float v=0.,a=.5;for(int i=0;i<3;i++){v+=a*noise3(p);p=p*2.03+vec3(1.7,9.2,3.1);a*=.5;}return v*1.14;}
float fb3(vec2 p){float v=0.,a=.5;for(int i=0;i<3;i++){v+=a*noise(p);p=p*2.03+vec2(1.7,9.2);a*=.5;}return v*1.14;}
void main(){
 vec2 q=(gl_FragCoord.xy/res)*2.-1.;float r=length(q);if(r>1.){gl_FragColor=vec4(0.);return;}float ang=atan(q.y,q.x);
 vec3 col=vec3(0.);float a=0.;
 vec2 cs=vec2(cos(ang),sin(ang));float sm=fq3(vec3(cs*1.6,r*5.-t*.25));
 float halo=smoothstep(.6,.8,r)*smoothstep(1.,.8,r)*(.3+sm*.95);
 vec3 hc=mix(vec3(.25,.35,1.),vec3(.62,.38,1.),sm);hc=mix(hc,vec3(1.,.13,.1),red*.55);
 col+=hc*halo;a+=halo;
 float core=smoothstep(.8,.74,r);
 vec3 cc=mix(vec3(0.),vec3(.043,.031,.125),smoothstep(0.,.55,r));cc=mix(cc,vec3(.106,.08,.31),smoothstep(.45,.78,r));
 cc+=vec3(.25,.15,.6)*pow(fb3(q*3.+t*.03),3.)*.5;
 col=mix(col,cc,core);a=mix(a,1.,core);
 // глаз: только радужка со зрачком, без век — висит в центре домена
 vec2 p=q/.42;float x=p.x,y=p.y;
 vec2 c=look*.09;float R=.255*(1.+.12*red);
 float d=length(p-c)-R;
 float rr=length(p);float pa=atan(p.y,p.x);
 vec3 fq=vec3(cos(pa)*1.4,sin(pa)*1.4,rr*3.-t*(.55+flare*.8));
 float fall=smoothstep(1.+flare*.6,.25,rr);float fl=0.,e=0.;
 if(fall>0.){fl=fq3(fq*2.)*fq3(fq*1.3+3.);e=pow(fl*2.1,2.4)*fall*smoothstep(-.02,.08,d)*(1.4+flare*2.5);}
 vec3 eC=mix(vec3(.18,.25,1.),vec3(.62,.36,1.),smoothstep(.2,.7,rr))+vec3(.5,.6,1.)*pow(fl,6.)*3.;
 eC=mix(eC,vec3(1.,.2,.12),red*.6);
 col+=eC*e;a+=e*.8;
 float hl=exp(-max(d,0.)*14.)*.6;
 col+=mix(vec3(.35,.45,1.),vec3(1.,.25,.2),red)*hl;a+=hl*.7;
 if(d<.01){
  vec3 sc=vec3(0.);
  vec2 ip=p-c;float ir=length(ip);float ia=atan(ip.y,ip.x);
  float fib=fb3(vec2(ia*14.,ir*7.-t*.15))*.7+fb3(vec2(ia*40.,ir*3.))*.5;
  vec3 iC=mix(vec3(.55,.85,1.),vec3(.12,.25,.95),smoothstep(.06,R,ir));
  iC=mix(iC,vec3(.6,.35,1.),smoothstep(.15,R,ir)*.6);
  iC=mix(iC,vec3(1.,.25,.15),red*.75);
  iC*=.35+pow(fib,1.6)*1.5;
  float ring=smoothstep(.006,0.,abs(ir-.175))*(.6+.4*step(.5,fract(ia*6./3.14159+t*.2)));
  iC+=vec3(.9,.95,1.)*ring*.9;
  iC*=1.-smoothstep(R-.04,R,ir)*.85;
  float pr=.075+.006*sin(t*1.3)-red*.025;
  iC=mix(iC,vec3(0.),smoothstep(pr,pr-.008,ir));
  iC+=mix(vec3(.4,.6,1.),vec3(1.,.3,.2),red)*smoothstep(.02,0.,abs(ir-pr-.01))*.8;
  vec3 eye=mix(sc,iC,smoothstep(R,R-.006,ir));
  eye+=vec3(1.)*smoothstep(.035,0.,length(ip-vec2(-.07,.07)))*.9;
  eye+=vec3(.7,.8,1.)*smoothstep(.015,0.,length(ip-vec2(.05,-.05)))*.6;
  float k=smoothstep(.01,-.004,d);
  col=mix(col,eye,k);a=max(a,k);
 }
 gl_FragColor=vec4(col,clamp(a,0.,1.));}`;
let EYE = null, eyeFail = false;
const RING_JP = '天眼 領域展開 電波 受信 解析 近接 感知 赤外 網 波 音 機 星 空 ';
const RING_EN = 'BLE · WI-FI · 433 MHz · SOUND · IR · NFC · SKY · PC · ';

T.views.push({
  id: 'hq', title: 'HQ', want: 'ble,wifi,snd,sens,wx,pc,ir,nfc', subs: ['NERV', 'DOMAIN'], sub: 0,
  mount(main) {
    this.root = T.el('div', 'view'); const st = T.el('div', 'stage'); this.root.appendChild(st); this.cv = T.canvas(st); main.appendChild(this.root);
    this.sweep = 0; this.wf = []; this.wfPk = -1; this.lvl = 0; this.alertT = -99; this.alertTxt = null;
    this.prevIr = 0; this.prevNfc = 0; this.flashIr = -99; this.flashNfc = -99; this.scan = null; this.seenAt = new Map();
    this.cv.cv.addEventListener('pointerup', e => { const h = T.hits.at(e.offsetX, e.offsetY); if (h && h.go) T.app.go(h.go); });
  },
  unmount() { this.cv.dispose(); this.root.remove(); },

  // свои цвета без перекраски SUMI: на время кадра тема «сырая» (см. T.hookCanvas)
  frame(dt, t) {
    const th = T.theme; T.theme = 'raw';
    try { this.frame2(dt, t); } finally { T.theme = th; }
  },
  frame2(dt, t) {
    // полоса вкладок NERV/DOMAIN сверху: сцена сдвигается под неё (касания — тоже)
    const off = document.body.classList.contains('hassubs') ? 44 : 0, o = this.cv;
    o.g.fillStyle = this.sub === 1 ? D.bg : E.bg; o.g.fillRect(0, 0, o.w, off);
    o.g.save(); o.g.translate(0, off);
    try { this.frame3(dt, t, o.g, o.w, o.h - off); } finally { o.g.restore(); }
    if (off) for (const q of T.hits.list) q.y += off;
  },
  frame3(dt, t, g, w, h) {
    T.hits.clear();
    const now = performance.now(), u = Math.min(w / 96, h / 60), m = u * 0.8;
    this.sweep = (this.sweep + dt * 1.6) % T.TAU;
    this.track(now, t); this.wfTick(dt); this.actTick(dt, t);
    if (this.sub === 1) { this.domain(g, w, h, u, dt, t, now); return; }
    g.fillStyle = E.bg; g.fillRect(0, 0, w, h);

    // ---------- раскладка ----------
    const topH = u * 5.2, logH = u * 2.4, botH = u * 6;
    const y1 = m + topH + m, yLog = h - m - logH, yBot = yLog - m - botH;
    const portrait = h > w * 1.05;
    let L, C, R;
    if (!portrait) {
      const sw = Math.min(u * 22, w * 0.23);
      L = { x: m, y: y1, w: sw, h: yBot - m - y1 };
      R = { x: w - m - sw, y: y1, w: sw, h: L.h };
      C = { x: L.x + sw + m, y: y1, w: R.x - m - (L.x + sw + m), h: L.h };
    } else {
      const ch = (yBot - m - y1) * 0.5;
      C = { x: m, y: y1, w: w - 2 * m, h: ch };
      const sw = (w - 3 * m) / 2;
      L = { x: m, y: y1 + ch + m, w: sw, h: yBot - m - (y1 + ch + m) };
      R = { x: m * 2 + sw, y: L.y, w: sw, h: L.h };
    }

    this.top(g, m, m, w - 2 * m, topH, u, t);
    // левая колонка: BLE / Wi-Fi / 433
    const lh = (L.h - 2 * m);
    this.blePanel(g, L.x, L.y, L.w, lh * 0.38, u, t);
    this.wifiPanel(g, L.x, L.y + lh * 0.38 + m, L.w, lh * 0.28, u);
    this.sensPanel(g, L.x, L.y + lh * 0.66 + 2 * m, L.w, lh * 0.34, u, t);
    // центр: радар
    this.radar(g, C.x, C.y, C.w, C.h, u, t, now);
    // правая колонка: погода / звук / пульты и карты
    this.wxPanel(g, R.x, R.y, R.w, lh * 0.33, u);
    this.audioPanel(g, R.x, R.y + lh * 0.33 + m, R.w, lh * 0.22, u);
    this.probePanel(g, R.x, R.y + lh * 0.55 + 2 * m, R.w, lh * 0.45, u, t, now);
    // низ: 433 поток + шкала активности
    const bw = (w - 3 * m) * 0.58;
    this.wfPanel(g, m, yBot, bw, botH, u, dt);
    this.lvlPanel(g, m * 2 + bw, yBot, w - 3 * m - bw, botH, u, dt, t);
    this.log(g, m, yLog, w - 2 * m, logH, u, t);
    // сканлайны
    if (!this.scan || this.scan.h !== h) {
      const c = document.createElement('canvas'); c.width = 4; c.height = 3; const q = c.getContext('2d');
      q.fillStyle = 'rgba(0,0,0,.28)'; q.fillRect(0, 0, 4, 1); this.scan = { p: g.createPattern(c, 'repeat'), h };
    }
    if (T.Q.scan) { g.fillStyle = this.scan.p; g.fillRect(0, 0, w, h); }
  },

  // события: новое близкое устройство, пульт, карта
  track(now, t) {
    const I = S.ir, N = S.nfc;
    if (I && I.t !== this.prevIr) { this.prevIr = I.t; if (I.t) this.flashIr = t; }
    if (N && N.t !== this.prevNfc) { this.prevNfc = N.t; if (N.t) this.flashNfc = t; }
    if (S.devInit) for (const e of S.dev.values()) {
      if (!e.alive || this.seenAt.has(e.id)) continue;
      this.seenAt.set(e.id, now);
      if (!e.first && (e.r > -58 || (e.type === 'tracker' && e.r > -70))) { this.alertT = t; this.alertTxt = { n: e.label || e.name || e.type || 'device', r: e.r, tr: e.type === 'tracker' }; }
    }
    if (this.seenAt.size > 400) this.seenAt.clear();
  },

  top(g, x, y, w, h, u, t) {
    stripes(g, x, y, u * 2.6, h, E.or, u * 0.7);
    // заголовок
    let cx = x + u * 3.4;
    const tw0 = (() => { font(g, h * 0.72, JP()); return g.measureText('天眼司令室').width; })();
    const bw = tw0 + u * 13.5;
    g.strokeStyle = E.or; g.lineWidth = u * 0.12; g.strokeRect(cx, y, bw, h);
    jp(g, '天眼司令室', cx + u * 0.7, y + h * 0.8, h * 0.72, E.or, 'left', 0.05);
    tx(g, 'TENGAN HQ', cx + tw0 + u * 1.6, y + h * 0.45, u * 1.5, E.or2, FT(), 'left', '600 ');
    tx(g, 'PASSIVE MONITOR · ' + (T.VERSION || ''), cx + tw0 + u * 1.6, y + h * 0.75, u * 0.7, E.or);
    cx += bw + u;
    // часы справа
    const d = new Date(), cw = u * 13;
    const kx = x + w - cw;
    g.strokeStyle = E.or; g.strokeRect(kx, y, cw, h);
    tx(g, hms(d), kx + cw - u * 0.8, y + h * 0.62, h * 0.55, E.or2, FT(), 'right', '600 ');
    tx(g, d.getFullYear() + '.' + pad2(d.getMonth() + 1) + '.' + pad2(d.getDate()) + ' · 月齢 ' + moonAge().toFixed(0), kx + cw - u * 0.8, y + h * 0.88, u * 0.72, E.or, FM(), 'right');
    // статус
    const sx = cx, sw = kx - u - cx;
    if (sw > u * 18) {
      g.strokeStyle = E.or; g.lineWidth = u * 0.07; g.strokeRect(sx, y, sw, h);
      const live = T.Stream.mode === 'live' || T.Stream.mode === 'usb', poll = T.Stream.mode === 'poll' || T.Stream.mode === 'usbwait', s = S.st || {};
      const r = s.radio || '', srcs = ['b', 'w', 's', 'm', 'p', 'i', 'n'].filter(k => r.includes(k)).length;
      const cols = [
        ['STATUS 状態', live ? '受信中' : poll ? '低速' : '断線', live ? E.grn : poll ? E.or2 : E.red, true],
        ['SOURCES 系統', srcs + ' / 7 ONLINE', E.or2, false],
        ['STICK 棒', s.bat >= 0 ? 'BAT ' + s.bat + '%' + (s.usb ? ' ⚡' : '') : 'BAT —', s.bat >= 0 && s.bat <= 15 ? E.red : E.or2, false],
      ];
      const cw2 = sw / cols.length;
      cols.forEach(([k, v, col, j], i) => {
        const px = sx + cw2 * i + u;
        tx(g, k, px, y + h * 0.33, u * 0.7, E.or);
        if (j) { jp(g, v, px, y + h * 0.82, h * 0.42, col, 'left', 0.04); if (live && blink(t)) { font(g, h * 0.42, JP()); g.fillStyle = col; g.beginPath(); g.arc(px + g.measureText(v).width + u * 1.1, y + h * 0.66, u * 0.45, 0, T.TAU); g.fill(); } }
        else tx(g, v, px, y + h * 0.8, h * 0.4, col, FT(), 'left', '600 ');
      });
    }
  },

  blePanel(g, x, y, w, h, u, t) {
    const y0 = panel(g, x, y, w, h, u, 'BLE · 近距離', '機器');
    T.hits.rect(x, y, w, h, { go: 'ble' });
    const devs = [...S.dev.values()].filter(e => e.alive).sort((a, b) => b.rs - a.rs);
    const n = S.ble ? S.ble.n : devs.length, now = performance.now();
    const fresh = devs.filter(e => !e.first && now - e.born < 300000).length;
    tx(g, S.ble ? String(n) : '—', x + u * 0.7, y0 + u * 4.2, u * 4.6, E.or2, FT(), 'left', '600 ');
    font(g, u * 4.6, FT(), '600 '); const nx = x + u * 1.4 + g.measureText(S.ble ? String(n) : '—').width;
    tx(g, 'DEVICES', nx, y0 + u * 2.8, u * 0.7, E.or);
    tx(g, '+' + fresh + ' NEW / 5 MIN', nx, y0 + u * 3.9, u * 0.7, fresh ? E.grn : E.dim);
    // соты: по ячейке на устройство, цвет = сила, новые — красные
    const top = y0 + u * 5.2, s = u * 0.62, dx = s * 1.75, dy = s * 1.52;
    const cols = Math.max(1, Math.floor((w - u * 1.2) / dx)), rows = Math.max(1, Math.floor((y + h - top - u * 0.4) / dy));
    let i = 0;
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++, i++) {
      const cx = x + u * 0.6 + s + c * dx + (r % 2) * dx / 2, cy = top + s + r * dy;
      if (cx + s > x + w - u * 0.3) continue;
      const e = devs[i];
      g.beginPath();
      for (let k = 0; k < 6; k++) { const a = Math.PI / 3 * k + Math.PI / 6; g.lineTo(cx + s * Math.cos(a), cy + s * Math.sin(a)); }
      g.closePath();
      if (e) {
        const p = T.pct(e.rs), nw = !e.first && now - e.born < 30000 && e.rs > -70, trk = e.type === 'tracker' && e.rs > -70;
        g.fillStyle = nw || trk ? (blink(t, 2) || !nw ? E.red : E.dk) : p > 0.55 ? E.or : p > 0.25 ? '#b35f10' : E.dim;
        g.globalAlpha = 0.35 + 0.65 * e.h; g.fill(); g.globalAlpha = 1;
      }
      g.strokeStyle = 'rgba(255,140,26,.3)'; g.lineWidth = 1; g.stroke();
    }
  },

  wifiPanel(g, x, y, w, h, u) {
    const y0 = panel(g, x, y, w, h, u, 'WI-FI · 2.4 GHz', '網');
    T.hits.rect(x, y, w, h, { go: 'wifi' });
    const best = new Float32Array(14).fill(-100);
    for (const a of S.aps.values()) if (a.alive) { const c = a.c | 0; if (c >= 1 && c <= 13) best[c] = Math.max(best[c], a.rs); }
    const bx = x + u * 0.6, bw = (w - u * 1.2) / 13, bb = y + h - u * 1.4, bh = bb - y0 - u * 0.6, segs = 6, sh = bh / segs;
    for (let c = 1; c <= 13; c++) {
      const lit = Math.round(T.pct(best[c]) * segs), xx = bx + (c - 1) * bw;
      for (let k = 0; k < segs; k++) { g.fillStyle = k < lit ? (k >= 4 ? E.red : E.or) : E.dk; g.fillRect(xx + bw * 0.14, bb - (k + 1) * sh + sh * 0.12, bw * 0.72, sh * 0.76); }
      tx(g, String(c), xx + bw / 2, bb + u * 0.95, u * 0.62, E.or, FM(), 'center');
    }
  },

  sensPanel(g, x, y, w, h, u, t) {
    const y0 = panel(g, x, y, w, h, u, '433 MHz · SENSORS', '感知');
    T.hits.rect(x, y, w, h, { go: 'sub' });
    const d = S.sens, list = (d && d.sensors) || [];
    let ry = y0 + u * 1.3;
    tx(g, 'ID', x + u * 0.6, ry, u * 0.62, E.or); tx(g, 'TEMP · HUM', x + w * 0.62, ry, u * 0.62, E.or, FM(), 'right'); tx(g, 'SEEN', x + w - u * 0.6, ry, u * 0.62, E.or, FM(), 'right');
    ry += u * 1.45;
    if (!list.length) { tx(g, d ? 'LISTENING · NO SENSORS YET' : 'NO DATA', x + u * 0.6, ry, u * 0.7, E.dim); }
    for (const s of list) {
      if (ry > y + h - u * 0.5) break;
      const rem = s.kind === 'remote', id = rem ? 'REMOTE ⌁ #' + String(s.id).split('-').pop().toUpperCase() : (s.p + '·' + s.ch).toUpperCase();
      const col = rem ? E.cy : E.txt, fresh = s.ago < 3;
      font(g, u * 0.72, FM()); tx(g, T.fit(g, id, w * 0.4), x + u * 0.6, ry, u * 0.72, col);
      if (!rem && s.t != null) tx(g, (s.t > 0 ? '+' : '') + s.t.toFixed(1) + '°' + (s.h != null ? ' ' + s.h + '%' : ''), x + w * 0.62, ry, u * 0.9, E.or2, FT(), 'right', '600 ');
      else tx(g, rem ? 'BTN ' + (s.btn ?? '—') : '—', x + w * 0.62, ry, u * 0.72, E.dim, FM(), 'right');
      tx(g, fresh && blink(t, 2) ? 'NOW' : T.ago(s.ago), x + w - u * 0.6, ry, u * 0.72, fresh ? E.grn : E.or, FM(), 'right');
      if (s.bat === 0) tx(g, 'BAT', x + w * 0.66, ry, u * 0.6, E.red);
      ry += u * 1.35;
    }
  },

  radar(g, x, y, w, h, u, t, now) {
    const nB = [...S.dev.values()].filter(e => e.alive).length, nA = [...S.aps.values()].filter(a => a.alive).length, nR = ((S.sens && S.sens.sensors) || []).length;
    const y0 = panel(g, x, y, w, h, u, 'RADIO HORIZON · 電波地平', '', 'RANGE ≈ RSSI · ' + nB + ' BLE · ' + nA + ' AP · ' + nR + ' RF');
    const cx = x + w / 2, cy = y0 + (y + h - y0) / 2, R = Math.min(w * 0.46, (y + h - y0) * 0.44);
    if (!(R > 8)) return;                                // окно ещё не разложилось (поворот/ресайз)
    const gr = g.createRadialGradient(cx, cy, 0, cx, cy, R * 1.25); gr.addColorStop(0, 'rgba(255,140,26,.10)'); gr.addColorStop(1, 'rgba(255,140,26,0)');
    g.fillStyle = gr; g.fillRect(x + 1, y0, w - 2, y + h - y0 - 1);
    jp(g, '索敵', x + u * 0.8, y0 + u * 3.2, u * 2.8, E.or, 'left', 0.05);
    tx(g, 'SCANNING ALL BANDS', x + u * 0.9, y0 + u * 4.3, u * 0.7, E.or);
    // кольца, лучи, шкала
    g.lineWidth = 1;
    for (let i = 1; i <= 5; i++) {
      g.strokeStyle = `rgba(255,140,26,${i === 5 ? 0.8 : 0.3})`; g.setLineDash(i % 2 ? [] : [u * 0.2, u * 0.3]);
      g.beginPath(); g.arc(cx, cy, R * i / 5, 0, T.TAU); g.stroke();
    }
    g.setLineDash([]);
    g.strokeStyle = 'rgba(255,140,26,.16)';
    for (let a = 0; a < 12; a++) { const q = a * Math.PI / 6; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + R * Math.cos(q), cy + R * Math.sin(q)); g.stroke(); }
    g.strokeStyle = E.or;
    for (let a = 0; a < 72; a++) { const q = a * Math.PI / 36, l = a % 6 ? u * 0.35 : u * 0.8; g.beginPath(); g.moveTo(cx + (R + u * 0.4) * Math.cos(q), cy + (R + u * 0.4) * Math.sin(q)); g.lineTo(cx + (R + u * 0.4 + l) * Math.cos(q), cy + (R + u * 0.4 + l) * Math.sin(q)); g.stroke(); }
    ['-45', '-60', '-75', '-90'].forEach((s, i) => tx(g, s + ' dBm', cx + u * 0.3, cy - R * (i + 1) / 5 - u * 0.4, u * 0.6, E.or));
    // луч
    const sw = this.sweep, N = 22;
    for (let i = 0; i < N; i++) {
      const a1 = sw - i * 0.035, a2 = a1 - 0.036;
      g.fillStyle = `rgba(255,140,26,${(0.26 * (1 - i / N)).toFixed(3)})`;
      g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, R, a2, a1); g.closePath(); g.fill();
    }
    g.strokeStyle = E.or2; g.lineWidth = u * 0.12; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + R * Math.cos(sw), cy + R * Math.sin(sw)); g.stroke();
    // точки: радиус по силе, угол — постоянный от id; ярче, когда луч прошёл
    const lit = a => { let d = (sw - a) % T.TAU; if (d < 0) d += T.TAU; return 1 - Math.min(1, d / 2.6); };
    const pos = (id, rs) => { const a = (T.hash(id) % 3600) / 3600 * T.TAU; const rr = R * (0.08 + 0.92 * (1 - T.pct(rs))); return [a, cx + rr * Math.cos(a), cy + rr * Math.sin(a)]; };
    const labels = [];
    for (const a of S.aps.values()) {
      if (!a.alive && a.h < 0.05) continue;
      const [q, px, py] = pos('ap' + a.k, a.rs), s = u * 0.4, k = 0.45 + 0.55 * lit(q);
      g.globalAlpha = a.h * k; g.fillStyle = E.cy; g.beginPath(); g.moveTo(px, py - s); g.lineTo(px + s, py); g.lineTo(px, py + s); g.lineTo(px - s, py); g.closePath(); g.fill();
    }
    for (const s of ((S.sens && S.sens.sensors) || [])) {
      if (s.rssi == null) continue;
      const [q, px, py] = pos('rf' + s.id, s.rssi), z = u * 0.5, k = 0.5 + 0.5 * lit(q);
      g.globalAlpha = k; g.fillStyle = E.wht; g.beginPath(); g.moveTo(px, py - z); g.lineTo(px + z * 0.9, py + z * 0.7); g.lineTo(px - z * 0.9, py + z * 0.7); g.closePath(); g.fill();
      if (s.t != null) labels.push([px, py, (s.t > 0 ? '+' : '') + s.t.toFixed(1) + '°', E.wht]);
    }
    for (const e of S.dev.values()) {
      if (e.h < 0.05) continue;
      const [q, px, py] = pos(e.id, e.rs), nw = !e.first && now - e.born < 30000 && e.rs > -70, k = 0.4 + 0.6 * lit(q);
      const col = e.label ? E.grn : nw || (e.type === 'tracker' && e.rs > -70) ? E.red : E.or2, r = u * (nw ? 0.42 : 0.3);
      g.globalAlpha = e.h * (nw ? 1 : k); g.fillStyle = col; g.beginPath(); g.arc(px, py, r, 0, T.TAU); g.fill();
      if (nw) {
        g.globalAlpha = blink(t, 2) ? 1 : 0.3; g.strokeStyle = col; g.lineWidth = u * 0.1; g.beginPath(); g.arc(px, py, u * 1.1, 0, T.TAU); g.stroke();
        g.globalAlpha = 0.8; g.setLineDash([u * 0.5, u * 2]); g.strokeRect(px - u * 1.6, py - u * 1.6, u * 3.2, u * 3.2); g.setLineDash([]);
      }
      if (e.label || e.name || nw) labels.push([px, py, e.label || e.name || e.type || 'new', col]);
      T.hits.circle(px, py, u * 1.2, { go: 'ble' });
    }
    g.globalAlpha = 1;
    // подписи без наложений: занятые прямоугольники пропускаем
    const boxes = [];
    for (const [px, py, s, col] of labels) {
      if (boxes.length >= 14) break;
      g.font = `500 ${u * 0.72}px ${FC()}`; const str = T.fit(g, s, u * 9), bw = g.measureText(str).width;
      const b = [px + u * 0.7, py - u * 0.55, bw, u * 1.0];
      if (boxes.some(q => b[0] < q[0] + q[2] && q[0] < b[0] + b[2] && b[1] < q[1] + q[3] && q[1] < b[1] + b[3])) continue;
      boxes.push(b); tx(g, str, b[0], py + u * 0.25, u * 0.72, col, FC(), 'left', '500 ');
    }
    // центр — глаз
    g.fillStyle = '#000'; g.beginPath(); g.arc(cx, cy, u * 0.9, 0, T.TAU); g.fill();
    g.strokeStyle = E.or; g.lineWidth = u * 0.15; g.stroke();
    jp(g, '眼', cx, cy + u * 0.4, u * 1.1, E.or, 'center', 0.02);
    // легенда
    const ly = y + h - u * 0.8; let lx = x + u * 0.8;
    for (const [c, s] of [[E.grn, 'SAVED 自'], [E.or2, 'KNOWN 既'], [E.red, 'NEW 新'], [E.cy, '◆ WI-FI AP'], [E.wht, '▲ 433 RF']]) {
      g.fillStyle = c; g.beginPath(); g.arc(lx + u * 0.25, ly - u * 0.25, u * 0.22, 0, T.TAU); g.fill();
      tx(g, s, lx + u * 0.7, ly, u * 0.62, E.or); font(g, u * 0.62, FM()); lx += u * 1.6 + g.measureText(s).width;
    }
    // предупреждение 接近警告
    const age = t - this.alertT;
    if (age < 20 && this.alertTxt) {
      const bw = Math.min(u * 15, w * 0.36), bh = u * 7.6, bx = x + w - bw - u * 0.8, by = y0 + u * 0.8;
      g.fillStyle = 'rgba(40,0,0,.88)'; g.fillRect(bx, by, bw, bh);
      stripes(g, bx, by, bw, u * 0.55, E.red, u * 0.5); stripes(g, bx, by + bh - u * 0.55, bw, u * 0.55, E.red, u * 0.5);
      g.strokeStyle = E.red; g.lineWidth = u * 0.1; g.strokeRect(bx, by, bw, bh);
      if (age < 4 ? blink(t, 2) : true) jp(g, '接近警告', bx + u * 0.7, by + u * 3.3, u * 2.4, E.red, 'left', 0.05);
      tx(g, this.alertTxt.tr ? 'TRACKER · NEARBY' : 'PROXIMITY · NEW DEVICE', bx + u * 0.7, by + u * 4.5, u * 0.8, E.red, FT(), 'left', '600 ');
      g.font = `500 ${u * 0.75}px ${FC()}`;
      tx(g, T.fit(g, String(this.alertTxt.n), bw - u * 1.4) , bx + u * 0.7, by + u * 5.7, u * 0.75, E.txt, FC(), 'left', '500 ');
      tx(g, this.alertTxt.r + ' dBm · ' + Math.round(age) + 's AGO', bx + u * 0.7, by + u * 6.7, u * 0.7, E.txt);
    }
  },

  wxPanel(g, x, y, w, h, u) {
    const y0 = panel(g, x, y, w, h, u, 'OUTSIDE · 外気', '天気');
    T.hits.rect(x, y, w, h, { go: 'gal' });
    const d = S.wx || {}, has = d.temp != null;
    const big = has ? (d.temp > 0 ? '+' : '') + Number(d.temp).toFixed(1) : '—';
    tx(g, big, x + u * 0.8, y0 + u * 4.6, u * 4.6, E.or2, FT(), 'left', '600 ');
    if (has) { font(g, u * 4.6, FT(), '600 '); tx(g, '°C', x + u * 1.0 + g.measureText(big).width, y0 + u * 4.6, u * 2, E.or2, FT(), 'left', '600 '); }
    const W = S.wxs, calm = W.storm > 0.5 ? ['STORM', '嵐', E.red] : W.rain > 0.4 ? ['RAIN', '雨', E.or2] : W.wind > 0.4 ? ['WIND', '風', E.or2] : W.clouds > 0.5 ? ['CLOUDY', '曇', E.or2] : ['CALM', '静', E.grn];
    const rows = [['HUMIDITY', d.hum != null ? d.hum + '%' : '—', E.txt], ['SOURCE', has ? '433 · ' + (d.tsrc || '?') : 'NO OUTDOOR SENSOR', has ? E.txt : E.dim], ['RADIO WEATHER', calm[0] + ' · ' + calm[1], calm[2]]];
    let ry = y0 + u * 6.2;
    for (const [k, v, c] of rows) { if (ry > y + h - u * 0.4) break; tx(g, k, x + u * 0.8, ry, u * 0.7, E.txt); tx(g, v, x + w - u * 0.8, ry, u * 0.7, c, FC(), 'right'); ry += u * 1.25; }
  },

  audioPanel(g, x, y, w, h, u) {
    const sn = S.snd && S.snd.ok;
    const y0 = panel(g, x, y, w, h, u, 'AUDIO · 音', '', sn ? Math.round(S.level) + ' dB' : 'MIC OFF');
    T.hits.rect(x, y, w, h, { go: 'snd' });
    const top = y0 + u * 0.4, hh = y + h - top - u * 0.4, mid = top + hh / 2;
    g.strokeStyle = 'rgba(255,140,26,.15)'; g.lineWidth = 1;
    for (let i = 0; i <= 8; i++) { const xx = x + w * i / 8; g.beginPath(); g.moveTo(xx, top); g.lineTo(xx, top + hh); g.stroke(); }
    g.beginPath(); g.moveTo(x, mid); g.lineTo(x + w, mid); g.stroke();
    g.strokeStyle = E.grn; g.lineWidth = u * 0.12; g.beginPath();
    for (let i = 0; i < 64; i++) { const xx = x + u * 0.3 + (w - u * 0.6) * i / 63, yy = mid - (sn ? S.wave[i] : 0) * hh * 0.45; i ? g.lineTo(xx, yy) : g.moveTo(xx, yy); }
    g.stroke();
  },

  probePanel(g, x, y, w, h, u, t, now) {
    const y0 = panel(g, x, y, w, h, u, 'PROBES · 探針', '解析');
    const bx = x + u * 0.6, bw = w - u * 1.2, bh = (y + h - y0 - u * 2.6) / 2;
    // ИК
    const I = S.ir && S.ir.last, fI = T.clamp(1 - (t - this.flashIr) / 1.2);
    let by = y0 + u * 0.6;
    g.fillStyle = `rgba(95,227,255,${0.25 * fI})`; g.fillRect(bx, by, bw, bh);
    g.strokeStyle = E.cy; g.lineWidth = u * (0.06 + 0.1 * fI); g.strokeRect(bx, by, bw, bh);
    jp(g, '赤外線', bx + u * 0.5, by + u * 1.2, u * 0.75, E.cy, 'left', 0.02);
    tx(g, 'IR' + (I ? ' · ' + T.ago((now - S.ir.t) / 1000) + ' AGO' : ' · LISTENING'), bx + u * 3.2, by + u * 1.2, u * 0.65, E.cy);
    if (I) {
      g.font = `600 ${u * 1.5}px ${FT()}`; tx(g, T.fit(g, (I.brand || I.p).toUpperCase(), bw - u), bx + u * 0.5, by + u * 2.9, u * 1.5, E.cy, FT(), 'left', '600 ');
      tx(g, I.p + ' · ' + T.hexs(I.addr, 4) + ' · ' + T.hexs(I.cmd) + (I.bits ? ' · ' + I.bits + 'b' : '') + (S.ir.hist[0] && S.ir.hist[0].x > 1 ? ' · ×' + S.ir.hist[0].x : ''), bx + u * 0.5, by + u * 4, u * 0.62, E.txt);
      const raw = I.raw || [];
      if (raw.length > 2 && bh > u * 5.2) {
        let tot = 0; for (const v of raw) tot += v;
        let xx = bx + u * 0.5; const ww = bw - u, yh = by + u * 4.6, yl = Math.min(by + bh - u * 0.4, yh + u * 1.1);
        g.strokeStyle = E.cy; g.lineWidth = u * 0.08; g.beginPath(); g.moveTo(xx, yl);
        raw.forEach((v, i) => { const nx = xx + ww * v / tot, yy = i % 2 ? yl : yh; g.lineTo(xx, yy); g.lineTo(nx, yy); xx = nx; });
        g.stroke();
      }
    } else tx(g, 'POINT A REMOTE AT THE STICK', bx + u * 0.5, by + u * 2.8, u * 0.7, E.dim);
    T.hits.rect(bx, by, bw, bh, { go: 'probe' });
    // NFC
    by += bh + u * 0.6;
    const N = S.nfc && S.nfc.last, fN = T.clamp(1 - (t - this.flashNfc) / 1.2);
    g.fillStyle = `rgba(255,179,71,${0.25 * fN})`; g.fillRect(bx, by, bw, bh);
    g.strokeStyle = E.or2; g.lineWidth = u * (0.06 + 0.1 * fN); g.strokeRect(bx, by, bw, bh);
    jp(g, '近接', bx + u * 0.5, by + u * 1.2, u * 0.75, E.or2, 'left', 0.02);
    tx(g, 'NFC' + (N ? ' · ' + T.ago((now - S.nfc.t) / 1000) + ' AGO' : S.nfc && S.nfc.err ? ' · NO READER' : ' · WAITING'), bx + u * 2.4, by + u * 1.2, u * 0.65, E.or2);
    if (N) {
      g.font = `600 ${u * 1.5}px ${FT()}`; tx(g, T.fit(g, String(N.fam || 'CARD').toUpperCase(), bw - u), bx + u * 0.5, by + u * 2.9, u * 1.5, E.or2, FT(), 'left', '600 ');
      g.font = `500 ${u * 0.62}px ${FC()}`; tx(g, T.fit(g, (N.hint ? N.hint + ' · ' : '') + 'UID ' + (N.uid || '—') + (N.sak ? ' · SAK ' + N.sak : ''), bw - u), bx + u * 0.5, by + u * 4, u * 0.62, E.txt, FC(), 'left', '500 ');
    } else tx(g, 'TOUCH A CARD TO THE STICK', bx + u * 0.5, by + u * 2.8, u * 0.7, E.dim);
    T.hits.rect(bx, by, bw, bh, { go: 'probe' });
    // ПК
    const p = S.pc, py = y + h - u * 0.7;
    tx(g, 'PC 機', bx, py, u * 0.65, E.txt);
    const ps = p && p.live ? 'CPU ' + Math.round(p.cpu || 0) + '%' + (p.ram != null ? ' · RAM ' + Math.round(p.ram) + '%' : '') + (p.trk ? ' · ♪ ' + (p.trk.play ? 'PLAY' : 'PAUSE') : '') : 'OFFLINE';
    tx(g, ps, bx + bw, py, u * 0.65, p && p.live ? E.txt : E.dim, FM(), 'right');
  },

  // поток 433: каждая новая посылка — полоса импульсов, лента едет вниз
  wfPanel(g, x, y, w, h, u, dt) {
    const d = S.sens, last = d && d.last;
    const y0 = panel(g, x, y, w, h, u, '433.92 MHz · PULSE FLOW', '滝', d ? (d.pk || 0) + ' PKTS' : 'OFF');
    T.hits.rect(x, y, w, h, { go: 'sub' });
    const top = y0 + u * 0.3, H = y + h - top - u * 0.3, rowH = H / 5;
    g.fillStyle = '#0d0602'; g.fillRect(x + 1, top, w - 2, H);
    this.wf.forEach((r, i) => {
      if (i >= 5) return;
      r.a = Math.max(0.25, r.a - dt * 0.05);
      const yy = top + i * rowH; let tot = 0; for (const v of r.pt) tot += v;
      const col = r.cls === 'sensor' ? E.or : r.cls === 'remote' ? E.cy : r.cls === 'noise' ? E.dim : E.red;
      let xx = x + u * 6; const ww = w - u * 6.6;
      g.globalAlpha = r.a; g.fillStyle = col;
      r.pt.forEach((v, k) => { const nw = ww * v / tot; if (k % 2 === 0) g.fillRect(xx, yy + rowH * 0.2, Math.max(1, nw), rowH * 0.6); xx += nw; });
      g.globalAlpha = 1;
      g.font = `${u * 0.6}px ${FM()}`; tx(g, T.fit(g, String(r.p || r.cls || '?').toUpperCase(), u * 5.2), x + u * 0.5, yy + rowH * 0.72, u * 0.6, col);
    });
    if (!this.wf.length) tx(g, d ? 'WAITING FOR 433 MHz PACKETS' : 'SENSORS STREAM OFF', x + w / 2, top + H / 2 + u * 0.3, u * 0.8, E.dim, FM(), 'center');
  },

  // новые посылки 433 — в ленту (общая для обоих стилей)
  wfTick(dt) {
    const d = S.sens, last = d && d.last;
    if (d && d.pk !== this.wfPk && last && last.pt) {
      if (this.wfPk >= 0) this.wf.unshift({ pt: last.pt, cls: last.cls, p: last.p, a: 1 });
      this.wfPk = d.pk; if (this.wf.length > 5) this.wf.pop();
    }
  },
  // активность эфира: новые BLE за минуту, посылки 433, громкость, пульты/карты
  actTick(dt, t) {
    const now = performance.now();
    let nb = 0; for (const e of S.dev.values()) if (!e.first && now - e.born < 60000) nb++;
    const pkRate = S.sens ? T.clamp((now - S.sensPkT) < 5000 ? 0.6 : 0.1) : 0;
    const snd = S.snd && S.snd.ok ? T.clamp((S.level + 70) / 50) : 0;
    const probe = T.clamp(1 - (t - Math.max(this.flashIr, this.flashNfc)) / 10);
    const target = T.clamp(0.1 + nb * 0.1 + pkRate * 0.15 + snd * 0.2 + probe * 0.15 + T.clamp(S.dev.size / 80) * 0.2);
    this.lvl += (target - this.lvl) * T.ease(dt, 1.5);
  },
  lvlPanel(g, x, y, w, h, u, dt, t) {
    const v = this.lvl, hot = v > 0.62, st = v > 0.8 ? ['ALERT', '警報', E.red] : hot ? ['ELEVATED', '注意', E.red] : v > 0.35 ? ['ACTIVE', '活発', E.or2] : ['QUIET', '平穏', E.grn];
    const y0 = panel(g, x, y, w, h, u, 'ACTIVITY LEVEL · 活動度', '', st[0], hot);
    jp(g, st[1], x + w - u * 0.6, y + h - u * 0.6, u * 1.6, st[2], 'right', 0.04);
    const n = 30, bx = x + u * 0.6, bw = w - u * 5.5, gap = u * 0.15, sw = (bw - gap * (n - 1)) / n, top = y0 + u * 0.8, bh = y + h - top - u * 0.8;
    const lit = Math.round(v * n), warm = Math.round(0.62 * n);
    for (let i = 0; i < n; i++) {
      const on = i < lit, red = i >= warm;
      g.fillStyle = on ? (red ? (blink(t, 1.5) ? E.red : '#7a120c') : E.or) : E.dk;
      g.fillRect(bx + i * (sw + gap), top, sw, bh);
    }
  },

  log(g, x, y, w, h, u, t) {
    g.fillStyle = E.or; g.fillRect(x, y, w, u * 0.12);
    const my = y + h * 0.72;
    stripes(g, x, y + u * 0.5, u * 2, h - u * 0.7, E.or, u * 0.45);
    jp(g, '記録', x + u * 2.6, my, u * 1.2, E.or, 'left', 0.03);
    let lx = x + u * 5.6;
    g.save(); g.beginPath(); g.rect(lx, y, w - (lx - x), h); g.clip();
    const tagCol = { BLE: E.red, IR: E.cy, NFC: E.or2, '433': E.grn, SUB: E.red, PC: E.txt, PWR: E.red };
    if (!S.events.length) tx(g, 'QUIET SO FAR', lx, my, u * 0.8, E.dim);
    for (const ev of S.events.slice(0, 8)) {
      const s1 = hms(new Date(ev.t)) + ' ', s2 = ev.tag + ' ', s3 = String(ev.text);
      font(g, u * 0.78, FC(), '500 ');
      tx(g, s1, lx, my, u * 0.78, E.txt, FC(), 'left', '500 '); lx += g.measureText(s1).width;
      tx(g, s2, lx, my, u * 0.78, ev.alert ? E.red : tagCol[ev.tag] || E.or, FC(), 'left', '500 '); lx += g.measureText(s2).width;
      tx(g, s3, lx, my, u * 0.78, E.txt, FC(), 'left', '500 '); lx += g.measureText(s3).width + u * 0.8;
      tx(g, '▍', lx, my, u * 0.78, E.dim); lx += u * 1.4;
      if (lx > x + w) break;
    }
    if (blink(t)) tx(g, '_', lx, my, u * 0.78, E.or);
    g.restore();
  },
  // ================= DOMAIN =================
  domain(g, w, h, u, dt, t, now) {
    if (w < 4 || h < 4) return;
    g.fillStyle = D.bg; g.fillRect(0, 0, w, h);
    const portrait = h > w * 1.05, m = u * 0.9;
    const topH = u * 6.4, barH = u * 4.2, logH = u * 3.6;
    const yBar = h - m - logH - u * 0.6 - barH, yLog = yBar + barH + u * 0.6;
    let sw = Math.min(u * 22, w * 0.23), cx, cy, R, L, Rc;
    if (!portrait) {
      cx = w / 2; cy = topH + (yBar - topH) / 2 + u * 0.4; R = Math.min((w - 2 * sw - 4 * m) * 0.47, (yBar - topH) * 0.48);
      L = { x: m, y: topH + u * 1.2, w: sw, h: yBar - m - topH - u * 1.2 }; Rc = { x: w - m - sw, y: L.y, w: sw, h: L.h };
    } else {
      const dh = (yBar - topH) * 0.52; cx = w / 2; cy = topH + dh / 2 + u; R = Math.min(w * 0.44, dh * 0.47);
      sw = (w - 3 * m) / 2; L = { x: m, y: topH + dh + u * 1.5, w: sw, h: yBar - m - (topH + dh + u * 1.5) }; Rc = { x: 2 * m + sw, y: L.y, w: sw, h: L.h };
    }
    this.speed(g, w, h, cx, cy, R, u);
    this.domHead(g, w, topH, u, t);
    this.domCore(g, w, h, u, dt, t, now, cx, cy, R);
    // кистевые вертикальные надписи по бокам домена
    if (!portrait) {
      for (const [s, x, col, px] of [['領域展開', cx - R * 1.13, D.red, u * 3.2], ['天眼無窮', cx + R * 1.1, D.glow, u * 2]]) {
        const y = x < cx ? cy - R * 0.85 : cy + R * 0.95 - px * s.length;
        for (let i = 0; i < s.length; i++) jp(g, s[i], x, y + px * (i + 1), px, col, 'center', 0.07);
      }
    }
    // левая колонка
    this.domGrades(g, L.x, L.y, L.w, L.h * 0.52, u, t, now);
    this.domSens(g, L.x, L.y + L.h * 0.52 + m, L.w, L.h * 0.48 - m, u, dt, t);
    // правая колонка
    this.domRight(g, Rc.x, Rc.y, Rc.w, Rc.h, u, t, now);
    this.domBar(g, m, yBar, w - 2 * m, barH, u, t);
    this.domLog(g, m, yLog, w - 2 * m, logH, u);
    this.domAlert(g, w, u, t, cx, cy, R, topH);
  },
  // скорость-линии от домена (кэш на размер)
  speed(g, w, h, cx, cy, R, u) {
    const k = [w, h, cx | 0, cy | 0, R | 0].join();
    if (!this.spd || this.spd.k !== k) {
      const c = document.createElement('canvas'), d = Math.min(2, this.cv.dpr || 1); c.width = Math.round(w * d); c.height = Math.round(h * d);
      const q = c.getContext('2d'); q.scale(d, d); let seed = 11; const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
      for (let i = 0; i < 240; i++) {
        const a = rnd() * T.TAU, r1 = R * (1.05 + rnd() * 0.3), r2 = r1 + R * (0.4 + rnd() * 1.6);
        q.strokeStyle = rnd() > 0.85 ? 'rgba(157,107,255,.55)' : 'rgba(42,37,72,.9)'; q.lineWidth = 0.5 + rnd() * 2.5;
        q.beginPath(); q.moveTo(cx + r1 * Math.cos(a), cy + r1 * Math.sin(a)); q.lineTo(cx + r2 * Math.cos(a), cy + r2 * Math.sin(a)); q.stroke();
      }
      this.spd = { k, c };
    }
    g.drawImage(this.spd.c, 0, 0, w, h);
  },
  domHead(g, w, H, u, t) {
    const pw = Math.min(w * 0.6, u * 62);
    g.fillStyle = '#000'; g.beginPath(); g.moveTo(0, 0); g.lineTo(pw, 0); g.lineTo(pw - H * 0.6, H); g.lineTo(0, H); g.closePath(); g.fill();
    const px = H * 0.78;
    jp(g, '天眼', u * 1.2, H * 0.84, px, D.paper, 'left', 0.08);
    font(g, px, JP()); const w1 = g.measureText('天眼').width;
    jp(g, '領域', u * 1.2 + w1, H * 0.84, px, D.red, 'left', 0.08);
    const x2 = u * 2.6 + w1 * 2;
    if (x2 + u * 14 < pw) {
      tx(g, 'T E N G A N', x2, H * 0.38, u * 1.5, D.paper, FT(), 'left', '600 ');
      tx(g, 'DOMAIN · 領域展開', x2, H * 0.62, u * 0.95, D.glow, FT(), 'left', '600 ');
      tx(g, 'ALL SIGNALS WITHIN REACH ARE SEEN', x2, H * 0.82, u * 0.6, D.glow);
    }
    const d = new Date(), s = S.st || {};
    tx(g, pad2(d.getHours()) + ':' + pad2(d.getMinutes()), w - u * 1.2, H * 0.68, H * 0.62, D.paper, FT(), 'right', '600 ');
    const live = T.Stream.mode === 'live' || T.Stream.mode === 'usb';
    tx(g, (live ? '受信中' : '断線') + ' · ' + (s.bat >= 0 ? 'BAT ' + s.bat + '%' : 'BAT —') + ' · 月齢 ' + moonAge().toFixed(0), w - u * 1.2, H * 0.92, u * 0.72, live ? D.glow : D.red, FC(), 'right');
  },
  // центр: глаз-шейдер + кольца-печати + присутствия
  domCore(g, w, h, u, dt, t, now, cx, cy, R) {
    const devs = [...S.dev.values()].filter(e => e.h > 0.05 && e.rs != null);
    const pos = (id, rs) => { const a = (T.hash(id) % 3600) / 3600 * T.TAU, rr = R * (0.36 + 0.5 * (1 - T.pct(rs))); return [a, cx + rr * Math.cos(a), cy + rr * Math.sin(a)]; };
    // куда смотрит зрачок: на сильнейший сигнал
    let best = null; for (const e of devs) if (e.alive && (!best || e.rs > best.rs)) best = e;
    const la = best ? (T.hash(best.id) % 3600) / 3600 * T.TAU : t * 0.2;
    this.look = this.look || [0, 0];
    const lk = T.ease(dt, 0.8); this.look[0] += (Math.cos(la) * 0.9 - this.look[0]) * lk; this.look[1] += (-Math.sin(la) * 0.6 - this.look[1]) * lk;
    const special = devs.some(e => e.alive && e.rs > -45), alertA = T.clamp(1 - (t - this.alertT) / 6);
    const redT = Math.max(alertA, special ? 0.35 : 0);
    this.redV = (this.redV || 0) + (redT - (this.redV || 0)) * T.ease(dt, 0.5);
    const fl = T.clamp(1 - (t - Math.max(this.flashIr, this.flashNfc)) / 1.6);
    const S0 = R * 2.3;
    if (T.GL && T.GL.ok && !eyeFail) {
      if (!EYE) { EYE = T.GL.prog(EYE_FS()); if (!EYE) eyeFail = true; }
      if (EYE) {
        // глаз считается через кадр (30 раз в секунду — глазу незаметно), между ними рисуется копия
        this.eyeN = (this.eyeN || 0) + 1;
        if (!this.eyeC) { this.eyeC = document.createElement('canvas'); this.eyeG = this.eyeC.getContext('2d'); }
        if ((this.eyeN & 1) || this.eyeS !== S0) {
          T.GL.begin(S0, S0, 0.6);
          T.GL.quad(EYE, { t, open: 0.3 + 0.1 * this.redV, red: this.redV, flare: fl, look: this.look });
          if (this.eyeC.width !== T.GL.w || this.eyeC.height !== T.GL.h) { this.eyeC.width = T.GL.w; this.eyeC.height = T.GL.h; }
          this.eyeG.globalCompositeOperation = 'copy'; this.eyeG.drawImage(T.GL.cv, 0, 0); this.eyeS = S0;
        }
        g.drawImage(this.eyeC, cx - S0 / 2, cy - S0 / 2, S0, S0);
      }
    }
    if (!T.GL || !T.GL.ok || eyeFail) {                    // запасной путь: ядро градиентом, глаз эллипсами
      const gr = g.createRadialGradient(cx, cy, 0, cx, cy, R * 0.95); gr.addColorStop(0, '#000'); gr.addColorStop(0.6, '#0b0820'); gr.addColorStop(0.85, '#1b1450'); gr.addColorStop(1, 'rgba(79,123,255,0)');
      g.fillStyle = gr; g.beginPath(); g.arc(cx, cy, R * 0.95, 0, T.TAU); g.fill();
      g.fillStyle = '#000'; g.strokeStyle = D.paper; g.lineWidth = u * 0.2; g.beginPath(); g.ellipse(cx, cy, R * 0.34, R * 0.14, 0, 0, T.TAU); g.fill(); g.stroke();
      g.fillStyle = this.redV > 0.5 ? D.red : D.blue; g.beginPath(); g.arc(cx + this.look[0] * R * 0.04, cy - this.look[1] * R * 0.04, R * 0.1, 0, T.TAU); g.fill();
      g.fillStyle = '#000'; g.beginPath(); g.arc(cx + this.look[0] * R * 0.04, cy - this.look[1] * R * 0.04, R * 0.04, 0, T.TAU); g.fill();
    }
    // кольцо-печать с иероглифами (медленно вращается) и внутреннее кольцо латиницей навстречу
    g.strokeStyle = D.txt; g.lineWidth = u * 0.12; g.beginPath(); g.arc(cx, cy, R * 1.0, 0, T.TAU); g.stroke();
    g.lineWidth = u * 0.06; g.beginPath(); g.arc(cx, cy, R * 0.86, 0, T.TAU); g.stroke();
    const ringTxt = (txt, rad, px, col, rot, f) => {
      g.save(); g.translate(cx, cy); g.rotate(rot);
      font(g, px, f); g.fillStyle = col; g.textAlign = 'center'; g.textBaseline = 'middle';
      const chars = [...txt], step = px * 1.05 / rad, n = Math.floor(T.TAU / step), k = T.TAU / n;
      for (let i = 0; i < n; i++) { const ch = chars[i % chars.length]; if (ch === ' ') continue; g.save(); g.rotate(i * k); g.translate(0, -rad); g.fillText(ch, 0, 0); g.restore(); }
      g.restore();
    };
    ringTxt(RING_JP, R * 0.93, R * 0.075, D.txt, t * 0.06, JP());
    ringTxt(RING_EN, R * 0.8, R * 0.032, D.blue, -t * 0.09, RAJ);
    // круги рангов
    g.setLineDash([u * 0.1, u * 0.5]); g.lineWidth = u * 0.12;
    [[-45, D.red], [-60, D.vio], [-75, D.blue]].forEach(([db, col]) => { g.strokeStyle = col; g.globalAlpha = 0.55; g.beginPath(); g.arc(cx, cy, R * (0.36 + 0.5 * (1 - T.pct(db))), 0, T.TAU); g.stroke(); });
    g.setLineDash([]); g.globalAlpha = 1;
    // Wi-Fi: чёрные ромбы-печати на внешнем кольце (сильнейшие 10)
    const aps = [...S.aps.values()].filter(a => a.alive).sort((a, b) => b.rs - a.rs).slice(0, 10);
    for (const a of aps) { const q = (T.hash('ap' + a.k) % 3600) / 3600 * T.TAU, x = cx + R * Math.cos(q), y = cy + R * Math.sin(q), s = u * (0.35 + 0.4 * T.pct(a.rs));
      g.save(); g.translate(x, y); g.rotate(Math.PI / 4); g.fillStyle = '#000'; g.fillRect(-s, -s, 2 * s, 2 * s); g.strokeStyle = D.red; g.lineWidth = u * 0.12; g.strokeRect(-s, -s, 2 * s, 2 * s); g.restore(); }
    // присутствия: огоньки с ореолом, цвет — ранг
    const labels = [];
    for (const e of devs) {
      const [, x, y] = pos(e.id, e.rs), gi = gradeOf(e.rs), col = GRADES[gi][3], lv = 3 - gi, sz = u * (1.4 + lv * 0.7);
      g.globalAlpha = T.clamp(e.h); g.drawImage(aura(col), x - sz, y - sz, sz * 2, sz * 2);
      g.fillStyle = '#fff'; g.beginPath(); g.arc(x, y, u * (0.18 + lv * 0.08), 0, T.TAU); g.fill(); g.globalAlpha = 1;
      if (gi === 0) { const j = (Math.sin(t * 7 + e.id) > 0.3) ? 1 : 0.6; crack(g, [[x - u * 2.2, y + u * 1.6], [x - u * 0.6, y + u * 0.3], [x - u * 1.2, y - u * 0.2], [x + u * 0.9, y - u * 1.6], [x + u * 0.4, y - u * 2], [x + u * 2.4, y - u * 3.2]], `rgba(227,33,43,${j})`, u * 0.22); }
      if (e.label || e.name || gi === 0) labels.push([x, y, e.label || e.name || e.type || 'presence', gi === 0 ? D.red : D.paper]);
      T.hits.circle(x, y, u * 1.4, { go: 'ble' });
    }
    const boxes = [];
    for (const [x, y, s, col] of labels.slice(0, 16)) {
      g.font = `500 ${u * 0.72}px ${FC()}`; const str = T.fit(g, s, u * 9), bw = g.measureText(str).width, b = [x + u * 1.3, y - u * 2.2, bw, u * 1];
      if (boxes.some(q => b[0] < q[0] + q[2] && q[0] < b[0] + b[2] && b[1] < q[1] + q[3] && q[1] < b[1] + b[3])) continue;
      boxes.push(b); g.strokeStyle = D.txt; g.lineWidth = 1; g.beginPath(); g.moveTo(x, y); g.lineTo(x + u * 1.1, y - u * 1.4); g.stroke();
      tx(g, str, b[0], y - u * 1.45, u * 0.72, col, FC(), 'left', '500 ');
    }
  },
  domGrades(g, x, y, w, h, u, t, now) {
    const y0 = mpanel(g, x, y, w, h, u, '呪力 · SIGNAL GRADES', { slant: 0.04, tone: 'rgba(157,107,255,.22)' });
    T.hits.rect(x, y, w, h, { go: 'ble' });
    const devs = [...S.dev.values()].filter(e => e.alive && e.rs != null), cnt = [0, 0, 0, 0];
    for (const e of devs) cnt[gradeOf(e.rs)]++;
    const fresh = devs.filter(e => !e.first && now - e.born < 300000).length;
    const n = S.ble ? S.ble.n : devs.length, big = Math.min(u * 4.8, h * 0.28);
    jp(g, S.ble ? String(n) : '—', x + u * 1.2, y0 + big * 1.05, big, D.paper, 'left', 0.05);
    font(g, big, JP()); const nx = x + u * 2 + g.measureText(S.ble ? String(n) : '—').width;
    tx(g, 'BLE PRESENCES', nx, y0 + big * 0.6, u * 0.62, D.glow);
    tx(g, '+' + fresh + ' NEW', nx, y0 + big * 0.95, u * 0.62, fresh ? D.red : D.dim);
    let ry = y0 + big * 1.05 + u * 0.6; const rh = Math.min(u * 2, (y + h - ry - u * 0.6) / 4);
    GRADES.forEach(([db, kj, en, col], i) => {
      if (ry + rh > y + h - u * 0.3) return;
      g.strokeStyle = D.dim; g.setLineDash([u * 0.2, u * 0.2]); g.lineWidth = 1; g.beginPath(); g.moveTo(x + u * 1.2, ry + rh); g.lineTo(x + w - u * 1.2, ry + rh); g.stroke(); g.setLineDash([]);
      jp(g, kj, x + u * 1.2, ry + rh * 0.82, rh * 0.75, col, 'left', 0.06);
      tx(g, en + (i === 0 ? ' · > −45' : i === 1 ? ' · −45…−60' : i === 2 ? ' · −60…−75' : ' · weaker'), x + u * 1.6 + rh * 1.6, ry + rh * 0.72, u * 0.62, D.txt);
      tx(g, String(cnt[i]), x + w - u * 1.2, ry + rh * 0.78, rh * 0.62, col, FT(), 'right', '600 ');
      ry += rh;
    });
  },
  domSens(g, x, y, w, h, u, dt, t) {
    const y0 = mpanel(g, x, y, w, h, u, '感知 · 433 MHz SENSORS');
    T.hits.rect(x, y, w, h, { go: 'sub' });
    const d = S.sens, list = (d && d.sensors) || [];
    const wfH = Math.min(u * 4, h * 0.3), yw = y + h - u * 0.8 - wfH;
    let ry = y0 + u * 1.6;
    if (!list.length) tx(g, d ? 'LISTENING · NO SENSORS YET' : 'NO DATA', x + u * 1.2, ry, u * 0.7, D.dim);
    for (const s of list) {
      if (ry > yw - u * 0.3) break;
      const rem = s.kind === 'remote', id = rem ? 'REMOTE ⌁ #' + String(s.id).split('-').pop().toUpperCase() : (s.p + ' · ch' + s.ch).toUpperCase();
      g.font = `${u * 0.68}px ${FM()}`; tx(g, T.fit(g, id, w * 0.45), x + u * 1.2, ry, u * 0.68, rem ? D.glow : D.txt);
      if (!rem && s.t != null) tx(g, (s.t > 0 ? '+' : '') + s.t.toFixed(1) + '°' + (s.h != null ? ' · ' + s.h + '%' : ''), x + w - u * 1.2, ry, u * 1.05, D.paper, FT(), 'right', '600 ');
      else tx(g, s.ago < 3 ? 'NOW' : T.ago(s.ago) + ' ago', x + w - u * 1.2, ry, u * 0.68, s.ago < 3 ? D.red : D.glow, FM(), 'right');
      g.strokeStyle = D.dim; g.setLineDash([u * 0.2, u * 0.2]); g.beginPath(); g.moveTo(x + u * 1.2, ry + u * 0.5); g.lineTo(x + w - u * 1.2, ry + u * 0.5); g.stroke(); g.setLineDash([]);
      ry += u * 1.45;
    }
    // поток посылок — фиолетовым
    const ww = w - u * 2.4, rowH = wfH / 5;
    g.fillStyle = '#05040a'; g.fillRect(x + u * 1.2, yw, ww, wfH);
    this.wf.forEach((r, i) => {
      if (i >= 5) return; let tot = 0; for (const v of r.pt) tot += v;
      const col = r.cls === 'sensor' ? D.vio : r.cls === 'remote' ? D.glow : r.cls === 'noise' ? D.dim : D.red;
      let xx = x + u * 1.2; g.fillStyle = col; g.globalAlpha = Math.max(0.3, 1 - i * 0.18);
      r.pt.forEach((v, k) => { const nw = ww * v / tot; if (k % 2 === 0) g.fillRect(xx, yw + i * rowH + rowH * 0.2, Math.max(1, nw), rowH * 0.6); xx += nw; });
      g.globalAlpha = 1;
    });
  },
  domRight(g, x, y, w, h, u, t, now) {
    // 外気: крупная кистевая температура на красном растре
    const wxH = h * 0.36, d = S.wx || {}, has = d.temp != null;
    const y0 = mpanel(g, x, y, w, wxH, u, '外気 · OUTSIDE', { slant: -0.04, tone: 'rgba(227,33,43,.35)' });
    T.hits.rect(x, y, w, wxH, { go: 'gal' });
    const big = has ? (d.temp > 0 ? '+' : '') + Number(d.temp).toFixed(1) + '°' : '—';
    const bp = Math.min(u * 5.6, wxH * 0.42, w / (big.length * 0.62));
    jp(g, big, x + u * 1.2, y0 + bp * 1.05, bp, D.paper, 'left', 0.07);
    let ry = y0 + bp * 1.05 + u * 1.2;
    for (const [k, v, c] of [['HUMIDITY', d.hum != null ? d.hum + '%' : '—', D.txt], ['SOURCE', has ? '433 · ' + (d.tsrc || '?') : 'NO OUTDOOR SENSOR', has ? D.txt : D.dim]]) {
      if (ry > y + wxH - u * 0.5) break; tx(g, k, x + u * 1.2, ry, u * 0.65, D.txt); tx(g, v, x + w - u * 1.2, ry, u * 0.65, c, FC(), 'right'); ry += u * 1.2;
    }
    // талисманы: последний пульт и последняя карта
    const oy = y + wxH + u * 1.2, oh = h * 0.42, ow = (w - u * 1.2) / 2;
    const I = S.ir && S.ir.last, N = S.nfc && S.nfc.last;
    const fI = T.clamp(1 - (t - this.flashIr) / 1.2), fN = T.clamp(1 - (t - this.flashNfc) / 1.2);
    const ir = I ? [['IR · ' + T.ago((now - S.ir.t) / 1000) + ' AGO', u * 0.62, '#333', 'm'], [(I.brand || I.p).toUpperCase(), u * 1.15, '#111', 't'], [T.hexs(I.cmd), u * 1.3, D.red, 't'], [I.p, u * 0.6, '#333', 'm'], [T.hexs(I.addr, 4), u * 0.6, '#333', 'm']]
      : [['IR · LISTENING', u * 0.62, '#333', 'm'], ['POINT A', u * 0.9, '#111', 't'], ['REMOTE', u * 0.9, '#111', 't']];
    const nf = N ? [['NFC · ' + T.ago((now - S.nfc.t) / 1000) + ' AGO', u * 0.62, '#333', 'm'], [String(N.fam || 'CARD').toUpperCase(), u * 1.1, '#111', 't'], [N.hint || '', u * 0.55, D.red, 'm'], ['UID ' + (N.uid || '—'), u * 0.6, '#333', 'm'], [N.sak ? 'SAK ' + N.sak : '', u * 0.6, '#333', 'm']]
      : [['NFC · ' + (S.nfc && S.nfc.err ? 'NO READER' : 'WAITING'), u * 0.62, '#333', 'm'], ['TOUCH', u * 0.9, '#111', 't'], ['A CARD', u * 0.9, '#111', 't']];
    ofuda(g, x, oy, ow, oh, u, -0.05 - 0.04 * fI, '赤外封', ['天', '眼'], ir, fI);
    ofuda(g, x + ow + u * 1.2, oy + u * 0.6, ow, oh, u, 0.06 + 0.04 * fN, '近接札', ['解', '析'], nf, fN);
    T.hits.rect(x, oy, w, oh, { go: 'probe' });
    // звук
    const ay = oy + oh + u * 1.6, ah = y + h - ay;
    if (ah > u * 3) {
      const sn = S.snd && S.snd.ok;
      const y1 = mpanel(g, x, ay, w, ah, u, '音 · AUDIO ' + (sn ? Math.round(S.level) + ' dB' : 'OFF'));
      T.hits.rect(x, ay, w, ah, { go: 'snd' });
      const mid = (y1 + ay + ah) / 2, amp = (ay + ah - y1) * 0.38;
      for (const [lw, col] of [[u * 0.5, 'rgba(127,178,255,.3)'], [u * 0.12, D.paper]]) {
        g.strokeStyle = col; g.lineWidth = lw; g.beginPath();
        for (let i = 0; i < 64; i++) { const xx = x + u * 1.2 + (w - u * 2.4) * i / 63, yy = mid - (sn ? S.wave[i] : 0) * amp; i ? g.lineTo(xx, yy) : g.moveTo(xx, yy); }
        g.stroke();
      }
    }
  },
  domBar(g, x, y, w, h, u, t) {
    g.fillStyle = '#000'; g.beginPath(); g.moveTo(x, y); g.lineTo(x + w, y + h * 0.14); g.lineTo(x + w, y + h); g.lineTo(x, y + h); g.closePath(); g.fill();
    jp(g, '呪力', x + u * 1, y + h * 0.82, h * 0.62, D.paper, 'left', 0.07);
    font(g, h * 0.62, JP()); const x1 = x + u * 2 + g.measureText('呪力').width;
    const v = this.lvl, st = v > 0.8 ? ['ALERT · 警報', D.red] : v > 0.62 ? ['ELEVATED · 注意', D.red] : v > 0.35 ? ['ACTIVE · 活発', D.vio] : ['QUIET · 平穏', D.glow];
    tx(g, 'AIR ACTIVITY', x1, y + h * 0.42, u * 0.65, D.glow);
    tx(g, st[0], x1, y + h * 0.78, u * 1.25, st[1], FT(), 'left', '600 ');
    const bx = x1 + u * 13, bw = x + w - u * 1 - bx, n = 40, gap = u * 0.12, sw = (bw - gap * (n - 1)) / n, lit = Math.round(v * n), warm = Math.round(0.62 * n);
    for (let i = 0; i < n; i++) {
      const on = i < lit, hot = on && i >= warm, bh = (on ? 1 : 0.6) * h * 0.56, xx = bx + i * (sw + gap), yy = y + h * 0.84 - bh;
      g.fillStyle = hot ? (blink(t, 1.5) ? D.red : '#7a0f14') : on ? D.blue : '#1a1730';
      g.beginPath(); g.moveTo(xx + bh * 0.36, yy); g.lineTo(xx + sw + bh * 0.36, yy); g.lineTo(xx + sw, yy + bh); g.lineTo(xx, yy + bh); g.closePath(); g.fill();
    }
  },
  domLog(g, x, y, w, h, u) {
    g.fillStyle = D.red; g.fillRect(x, y, h, h);
    jp(g, '記', x + h / 2, y + h * 0.78, h * 0.66, '#000', 'center', 0.06);
    const tagJ = { BLE: ['近接', D.red], IR: ['赤外', D.glow], NFC: ['近接', D.paper], '433': ['感知', D.vio], SUB: ['電波', D.red], PC: ['機', D.txt], PWR: ['電源', D.red] };
    const cw = (w - h - u * 2) / 2, rows = Math.max(1, Math.floor(h / (u * 1.15)));
    if (!S.events.length) tx(g, 'QUIET SO FAR', x + h + u, y + u * 1.2, u * 0.8, D.dim);
    S.events.slice(0, rows * 2).forEach((ev, i) => {
      const col = i < rows ? 0 : 1, row = i % rows, xx = x + h + u + col * (cw + u), yy = y + u * 1.0 + row * u * 1.15;
      const [kj, c] = tagJ[ev.tag] || [ev.tag, D.txt], ts = hms(new Date(ev.t)) + ' ';
      font(g, u * 0.78, FC(), '500 '); tx(g, ts, xx, yy, u * 0.78, ev.alert ? D.red : c, FC(), 'left', '500 ');
      const x2 = xx + g.measureText(ts).width; jp(g, kj, x2, yy, u * 0.78, ev.alert ? D.red : c, 'left', 0.02);
      font(g, u * 0.78, JP()); const x3 = x2 + g.measureText(kj).width + u * 0.5;
      g.font = `500 ${u * 0.78}px ${FC()}`; tx(g, T.fit(g, String(ev.text), cw - (x3 - xx)), x3, yy, u * 0.78, D.txt, FC(), 'left', '500 ');
    });
  },
  // 接近: чёрная косая плашка с красной трещиной (новое близкое устройство / трекер)
  domAlert(g, w, u, t, cx, cy, R, topH) {
    const age = t - this.alertT; if (age > 20 || !this.alertTxt) return;
    const bw = Math.min(u * 24, w * 0.3), bh = u * 7.2, bx = Math.min(w - bw - u, cx + R * 0.45), by = topH + u * 1.2;
    g.save(); g.translate(bx + bw / 2, by + bh / 2); g.rotate(-0.07); g.translate(-bw / 2, -bh / 2);
    g.fillStyle = '#000'; g.beginPath(); g.moveTo(0, bh * 0.12); g.lineTo(bw, 0); g.lineTo(bw * 0.97, bh); g.lineTo(bw * 0.03, bh * 0.92); g.closePath(); g.fill();
    if (age > 4 || blink(t, 2)) jp(g, '接近', u * 1.2, bh * 0.62, bh * 0.5, D.red, 'left', 0.08);
    tx(g, this.alertTxt.tr ? 'TRACKER NEARBY' : 'SPECIAL GRADE', bw * 0.48, bh * 0.3, u * 1, D.paper, FT(), 'left', '600 ');
    tx(g, 'PRESENCE DETECTED', bw * 0.48, bh * 0.5, u * 0.9, D.red, FT(), 'left', '600 ');
    g.font = `500 ${u * 0.7}px ${FC()}`;
    tx(g, T.fit(g, String(this.alertTxt.n), bw * 0.45) + ' · ' + this.alertTxt.r + ' dBm · ' + Math.round(age) + 's', u * 1.2, bh * 0.85, u * 0.7, D.paper, FC(), 'left', '500 ');
    crack(g, [[bw * 0.5, bh * 0.92], [bw * 0.62, bh * 0.7], [bw * 0.6, bh * 0.8], [bw * 0.78, bh * 0.6], [bw * 0.76, bh * 0.68], [bw * 0.98, bh * 0.5]], D.red, u * 0.25);
    g.restore();
  },
});
})();
