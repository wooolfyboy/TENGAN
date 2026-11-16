// ---- SOUND (версия B): спектр на глянцевом полу, 3D-рельеф, реактор, осциллограф, XY ----
// Данные — живой звук микрофона палки (~25 кадров/с), между кадрами всё перетекает плавно.
(() => {
  // «послесвечение» люминофора: свой холст, каждый кадр гасится и рисуется заново
  const trail = o => {
    const c = document.createElement('canvas'), x = c.getContext('2d');
    return {
      c, x,
      fit() { if (c.width !== o.cv.width || c.height !== o.cv.height) { c.width = o.cv.width; c.height = o.cv.height; x.fillStyle = '#000'; x.fillRect(0, 0, c.width, c.height); } x.setTransform(o.dpr, 0, 0, o.dpr, 0, 0); },
      fade(a) { x.save(); x.setTransform(1, 0, 0, 1, 0, 0); x.fillStyle = `rgba(0,0,0,${a})`; x.fillRect(0, 0, c.width, c.height); x.restore(); },
      draw(g) { g.save(); g.setTransform(1, 0, 0, 1, 0, 0); g.globalCompositeOperation = 'lighter'; g.drawImage(c, 0, 0); g.restore(); },   // чёрное не добавляет света
    };
  };
  const NAMES = ['Sound spectrum', 'Terrain', 'Reactor', 'Waveform', 'XY scope'];

  T.views.push({
    id: 'snd', title: 'Sound', want: 'snd', subs: ['Spectrum', 'Terrain', 'Reactor', 'Waveform', 'XY'], sub: 0,
    mount(main) {
      this.root = T.el('div', 'view');
      const st = T.el('div', 'stage'); this.root.appendChild(st);
      this.cv = T.canvas(st, 2);
      main.appendChild(this.root);
      this.ROWS = 40; this.COLS = 72; this.rows = [];
      if (T.glOrbit && T.GL.ok) { this.ROWS = 110; this.COLS = 128; }   // WebGL-рельеф: гуще и глубже
      this.rowT = 0; this.top = 0.3;
      this.tilt = 0; this.wave = new Float32Array(64);
      this.parts = []; this.bassPrev = 0; this.spin = 0; this.lvS = -90; this.pkS = -90; this.pkHold = -90; this.pkT = 0;
      this.trW = trail(this.cv); this.trX = trail(this.cv);
      // пальцем по рельефу — наклон
      let d = null;
      const cv = this.cv.cv;
      cv.addEventListener('pointerdown', e => { d = { y: e.offsetY, t: this.tilt }; cv.setPointerCapture(e.pointerId); });
      cv.addEventListener('pointermove', e => { if (d) this.tilt = T.clamp(d.t + (e.offsetY - d.y) / (this.cv.h || 1) * 1.4, -0.6, 0.6); });
      const up = () => { d = null; }; cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
    },
    unmount() { this.cv.dispose(); this.root.remove(); },
    onSub() { this.trW.fade(1); this.trX.fade(1); },
    frame(dt, t) {
      const o = this.cv, { g, w, h } = o; o.clear();
      const u = T.unit(w, h), m = u * 1.6, y0 = 50;
      T.backdrop(g, w, h, 0.5, 0.6);
      const d = S.snd, fresh = d && performance.now() - S.sndT < 1500;
      let bad = null;
      if (!d) bad = ['SOUND', 'waiting for sound'];
      else if (!d.en) bad = ['MIC OFF', 'settings › sound › microphone: on'];
      else if (!d.ok) bad = ['STARTING', 'starting microphone'];
      else if (!fresh) bad = ['NO SOUND DATA', 'microphone is silent', true];
      if (bad) {
        T.header(g, m, y0, w - 2 * m, u, NAMES[this.sub], 'mic', false);
        T.emptyState(g, w, h, u, bad[0], bad[1], bad[2]);
        T.finish(g, w, h); return;
      }
      // уровень: плавно, пик держится секунду
      this.lvS = T.lerp(this.lvS, S.level, T.ease(dt, 0.08));
      if (S.peak >= this.pkHold || t - this.pkT > 1.2) { this.pkHold = Math.max(S.peak, this.lvS); this.pkT = t; }
      for (let i = 0; i < 64; i++) this.wave[i] = T.lerp(this.wave[i], S.wave[i], T.ease(dt, 0.03));
      const loud = this.lvS >= -15;
      const top = y0 + u * 3.2, bot = h - m * 0.6 - u * 1.8;
      const mx = w - m - u * 1.2;                         // справа — шкала уровня
      const area = { x: m, y: top, w: mx - u * 1.6 - m, h: bot - top };
      if (this.sub === 0) this.spectrum(g, area, u);
      else if (this.sub === 1) this.terrain(g, w, h, dt, u, top);
      else if (this.sub === 2) this.reactor(g, area, u, dt, t);
      else if (this.sub === 3) this.scope(g, area, u, dt);
      else this.xy(g, area, u, dt);
      if (!(T.theme === 'sumi' && this.sub === 1)) T.bloom(o, this.sub === 1 ? 0.6 : 0.85);   // рельеф тушью уже «растёкся»
      // HUD поверх
      const hz = S.hz >= 1000 ? (S.hz / 1000).toFixed(1) + ' kHz' : S.hz + ' Hz';
      const word = this.lvS < -55 ? 'quiet' : this.lvS < -40 ? 'normal' : this.lvS < -15 ? 'loud' : 'very loud';
      T.header(g, m, y0, w - 2 * m, u, NAMES[this.sub], `mic · ${Math.round(this.lvS)} dBFS · ${word}${S.energy > 0.15 ? ' · ' + hz : ''}`, loud);
      T.dots(g, m, y0 + u * 2.75, u, 5, this.sub);
      this.meter(g, mx, top + u * 0.4, u * 1.2, bot - top - u * 0.8, u);
      T.footer(g, m, bot + u * 0.2, w - 2 * m, u, ['32 bands · 25 fps', 'drag: tilt', 'bass: burst', 'phosphor trace', 'phase figure'][this.sub], 'sse snd 25 hz');
      T.finish(g, w, h);
    },
    // вертикальная шкала уровня с удержанием пика
    meter(g, x, y, mw, mh, u) {
      const v = T.clamp((this.lvS + 60) / 60), pk = T.clamp((this.pkHold + 60) / 60), n = 24, sh = mh / n;
      for (let k = 0; k < n; k++) {
        const lit = k < Math.round(v * n), hot = (k / n) * 60 - 60 >= -15;
        g.fillStyle = lit ? (hot ? T.C.alert : T.heat(0.3 + 0.7 * k / n)) : T.C.line;
        g.fillRect(x, y + mh - (k + 1) * sh + sh * 0.2, mw, sh * 0.6);
      }
      const py = y + mh * (1 - pk);
      g.fillStyle = this.pkHold >= -15 ? T.C.alert : T.C.hot; g.fillRect(x - u * 0.25, py - 1, mw + u * 0.5, 2);
      T.txt(g, 'dB', x + mw / 2, y - u * 0.5, u * 0.7, T.C.dim, 'm', 'center');
      [0, -15, -30, -45, -60].forEach(db => T.txt(g, db, x - u * 0.4, y + mh * (-db / 60) + u * 0.25, u * 0.62, db === -15 ? T.C.alert : T.C.dim, 'm', 'right'));
    },
    // 32 сегментные полосы, отражение на глянцевом полу, шапки пиков
    spectrum(g, a, u) {
      const n = 32, L = a.x + u * 1.6, bw = (a.w - u * 1.6) / n, base = a.y + a.h * 0.7, H = base - a.y - u, seg = Math.max(4, u * 0.55);
      // пол
      const fl = g.createLinearGradient(0, base, 0, a.y + a.h);
      fl.addColorStop(0, 'rgba(62,230,255,.10)'); fl.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = fl; g.fillRect(L, base, bw * n, a.y + a.h - base);
      g.strokeStyle = 'rgba(32,50,64,.5)'; g.lineWidth = 1;
      for (let k = 1; k <= 4; k++) { const y = base - H * k / 4; g.beginPath(); g.moveTo(L, y); g.lineTo(L + bw * n, y); g.stroke(); }
      for (let i = 0; i < n; i++) {
        const v = S.bands[i], x = L + i * bw, segs = Math.floor(v * H / seg);
        for (let s = 0; s < segs; s++) {
          const y = base - (s + 1) * seg, c = T.heat(0.25 + 0.75 * (s * seg) / H);
          g.fillStyle = c; g.fillRect(x + bw * 0.12, y + seg * 0.15, bw * 0.76, seg * 0.7);
          const ry = base + s * seg * 0.55 + seg * 0.2;                     // отражение: короче и тусклее
          if (ry < a.y + a.h) { g.globalAlpha = 0.16 * (1 - s / (segs + 1)); g.fillRect(x + bw * 0.12, ry, bw * 0.76, seg * 0.4); g.globalAlpha = 1; }
        }
        const cy = base - S.caps[i] * H - seg * 0.4;
        g.fillStyle = T.C.hot; g.fillRect(x + bw * 0.12, cy, bw * 0.76, Math.max(2, seg * 0.22));
      }
      g.fillStyle = T.C.acc; g.fillRect(L, base, bw * n, 1);
      [[100, 4], [500, 14], [1000, 19], [4000, 28]].forEach(([f, i]) => T.txt(g, f >= 1000 ? f / 1000 + 'k' : f, L + (i + 0.5) * bw, a.y + a.h - u * 0.3, u * 0.8, T.C.label, 'm', 'center'));
      T.txt(g, 'Hz', L + bw * n, a.y + a.h - u * 0.3, u * 0.7, T.C.dim, 'm', 'right');
    },
    // 3D-рельеф: новый хребет 40 раз в секунду, дальние — в тумане
    terrain(g, w, h, dt, u, top) {
      this.rowT += dt;
      while (this.rowT > 0.025) {
        this.rowT -= 0.025;
        const C = this.COLS, raw = new Float32Array(C);
        const canyon = C === 128;   // WebGL: каньон — басы стенами по краям, верхи в долине по центру
        for (let c = 0; c < C; c++) { const k = canyon ? (1 - Math.abs(c / (C - 1) * 2 - 1)) * 30 : c * 31 / (C - 1), k0 = Math.floor(k), f = k - k0; const b = S.bands[k0] * (1 - f) + S.bands[Math.min(31, k0 + 1)] * f; raw[c] = b * b; }
        for (let pass = 0; pass < 2; pass++) for (let c = 1; c < C - 1; c++) raw[c] = (raw[c - 1] + 2 * raw[c] + raw[c + 1]) / 4;
        let mx = 0; for (let c = 0; c < C; c++) mx = Math.max(mx, raw[c]);
        this.top = Math.max(mx, this.top * 0.992, 0.12);
        const prev = this.rows[0], row = new Float32Array(C), loud = T.clamp(S.energy * 1.6, 0, 1);
        for (let c = 0; c < C; c++) {
          const v = 1 - Math.exp(-2.2 * raw[c] / this.top), edge = canyon ? 0.3 + 0.7 * Math.pow(Math.abs(c / (C - 1) * 2 - 1), 0.7) : Math.sin(Math.PI * (c + 0.5) / C);
          const hh = v * (0.35 + 0.65 * loud) * (0.45 + 0.55 * edge) + 0.04 + 0.05 * ((T.hash(this.rows.length * 977 + c) & 255) / 255) * (0.3 + v);
          row[c] = prev ? prev[c] * (canyon ? 0.5 : 0.35) + hh * (canyon ? 0.5 : 0.65) : hh;
        }
        let rm = 0; for (let c = 0; c < C; c++) rm = Math.max(rm, row[c]); row.max = rm;
        this.rows.unshift(row); if (this.rows.length > this.ROWS) this.rows.pop();
      }
      if (T.glOrbit && T.GL.ok && this.COLS === 128) return this.terrainGL(g, w, h, u, top);
      if (T.theme === 'sumi') return this.terrainInk(g, w, h, u, top);
      const hy = top + (h - top) * (0.16 - this.tilt * 0.12), camH = 1.25 + this.tilt * 0.6, cx = w / 2;
      const F = Math.max(Math.min(w, h * 1.6) * 0.62, (h * 0.97 - hy) * 1.5 / camH);
      // солнце-резонатор на горизонте
      const sx = w * 0.68, sunR = u * (1.8 + S.energy * 2.2);
      const sg = g.createRadialGradient(sx, hy, 0, sx, hy, sunR * 2.4);
      sg.addColorStop(0, 'rgba(235,250,255,.75)'); sg.addColorStop(0.25, 'rgba(62,230,255,.45)'); sg.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = sg; g.beginPath(); g.arc(sx, hy, sunR * 2.4, 0, T.TAU); g.fill();
      for (let k = 1; k < 6; k++) { g.fillStyle = 'rgba(5,8,13,.85)'; g.fillRect(sx - sunR * 2.4, hy - k * sunR * 0.28, sunR * 4.8, Math.max(1, k * 0.6)); }
      g.fillStyle = T.C.line; g.fillRect(0, hy, w, 1);
      const sh = T.heatRGB(0.4);
      for (let r = this.rows.length - 1; r >= 0; r--) {
        const row = this.rows[r], z = 1.5 + r * 0.12, fade = 1 - r / this.ROWS * 0.85;
        const by = hy + camH * F / z, xs = 3.2 * F / z;
        g.beginPath(); g.moveTo(cx - xs, by);
        for (let c = 0; c < this.COLS; c++) g.lineTo(cx + (c / (this.COLS - 1) * 2 - 1) * xs, hy + (camH - row[c] * 1.15) * F / z);
        g.lineTo(cx + xs, by); g.closePath();
        g.fillStyle = `rgb(${Math.round(sh[0] * 0.14 * fade + 4)},${Math.round(sh[1] * 0.14 * fade + 8)},${Math.round(sh[2] * 0.14 * fade + 12)})`;
        g.fill();
        g.strokeStyle = T.heat(0.3 + 0.7 * (row.max || 0), fade); g.lineWidth = r < 3 ? 2 : 1.1; g.stroke();
      }
    },

    // WebGL: полёт над рельефом звука. Новый хребет рождается у горизонта и плывёт к камере;
    // свет по нормалям, сетка-«трон» (DENKO) или тушь и туман-сансуй (SUMI), солнце на горизонте.
    terrainGL(g, w, h, u, top) {
      const O = T.glOrbit, C = this.COLS, R = this.ROWS, sumi = T.theme === 'sumi';
      if (!this.tgP) {
        this.tgSky = T.GL.prog(T.GL.NOISE + `
uniform vec2 res; uniform float t, hy, sumi, lv; uniform vec3 sun, cBg, cAcc, cHot, cAlert;
void main(){
  vec2 p=gl_FragCoord.xy; float y=(p.y-hy)/res.y;
  vec3 col;
  float d=length(p-sun.xy)/sun.z;
  if(sumi<.5){
    col=mix(cBg+cAcc*.10,cBg*.9,smoothstep(0.,.45,y));
    vec2 q=floor(p/3.); float st=step(.9975,hash(q))*smoothstep(.02,.2,y)*(.5+.5*sin(t*2.+hash(q+7.)*40.));
    col+=vec3(st)*.8;
    vec3 sc=mix(cAcc,cHot,smoothstep(-.3,1.,(p.y-sun.y)/sun.z));
    float disk=smoothstep(1.,.985,d);
    float below=(sun.y-p.y)/sun.z;   // нижняя половина солнца режется полосами
    if(below>0.){ float band=fract(below*6.); disk*=step(below*.55,band); }
    col=mix(col,sc,disk*step(hy,p.y));
    col+=cAcc*exp(-d*1.6)*(.35+lv*.5);
  }else{
    float paper=.96+.04*fbm(p/80.);
    col=cBg*paper;
    col=mix(col,cBg*.86,smoothstep(.0,.6,y)*.35);
    float edge=.03*fbm(p/18.+3.);
    float disk=smoothstep(1.+edge,.9+edge,d);
    col=mix(col,cAlert,disk*.85);
    col=mix(col,cAlert,exp(-d*2.5)*.12);
  }
  gl_FragColor=vec4(col,1.);
}`);
        this.tgP = O.prog(T.GL.NOISE + `
uniform vec3 cBg, cAcc, cHot, cLine, cAlert; uniform float sumi, lv, t;
varying vec3 wp; varying float vd; varying float vh; varying vec2 vg;
void main(){
  float v=clamp(vh,0.,1.);
  float shade=.6;
#ifndef NODERIV
  vec3 nn=normalize(cross(dFdx(wp),dFdy(wp))); if(nn.y<0.) nn=-nn;
  shade=clamp(dot(nn,normalize(vec3(.35,.55,.75))),0.,1.);
  float ay=fwidth(vg.y), ax=fwidth(vg.x);
#else
  float ay=.05, ax=.05;
#endif
  float fog=exp(-max(0.,vd-1.)*.13);
  vec3 col;
  if(sumi<.5){
    float dr=abs(fract(vg.y+.5)-.5), dc=abs(fract(vg.x/4.+.5)-.5)*4.;
    float lr=1.-smoothstep(ay*.5,ay*1.5,dr), lc=1.-smoothstep(ax*.5,ax*1.5,dc);
    vec3 heat=v<.5?mix(cLine,cAcc,v*2.):mix(cAcc,cHot,(v-.5)*2.);
    col=cBg*.8+cAcc*.10*shade*shade+cAcc*.05;
    col+=heat*(lr*.9+lc*.22*fog)*(.25+.9*v);
    col+=cHot*pow(v,4.)*.9;
    col=mix(cBg+cAcc*.10,col,fog);
  }else{
    // тушь: темнее к гребням и на склонах, «морщины» кисти (цунь) по высоте, грани почти не видны
    float ink=.06+.16*(1.-shade)+.55*smoothstep(.15,.9,v);
    ink*=.7+.6*fbm(vec2(wp.x*6.,vh*14.+wp.z*.5));
    col=mix(cBg,cAcc,clamp(ink,0.,1.)*.75);
    float ct=abs(fract(vh*9.+.5)-.5);
#ifndef NODERIV
    float aw=fwidth(vh*9.);
#else
    float aw=.05;
#endif
    col=mix(col,cAcc,(1.-smoothstep(aw*.5,aw*1.8,ct))*.45);
    float mist=fog*(.75+.25*fbm(vec2(wp.x*.4+t*.05,vd*.3)));
    col=mix(cBg,col,clamp(mist,0.,1.));
  }
  gl_FragColor=vec4(col,1.);
}`);
        this.tgH = new Float32Array(C * R); this.tgN = -1;
        this.tcam = T.orbit(0, 0.22, 3.0);
      }
      const cam = this.tcam;
      cam.pitch = 0.22 + this.tilt * 0.2;
      const F = w * 0.55, cy = top + (h - top) * 0.45;
      cam.frame(w / 2, cy, F);
      // высоты: новый ряд (индекс 0) — на горизонте
      const G = O.grid('sndTerrain', C, R);
      if (this.tgN !== this.rows.length || this.rows[0] !== this.tgLast) {
        this.tgN = this.rows.length; this.tgLast = this.rows[0];
        const H = this.tgH;
        for (let r = 0; r < R; r++) { const row = this.rows[r]; for (let c = 0; c < C; c++) H[r * C + c] = row ? row[c] : 0; }
        G.set(H);
      }
      const hyCss = cy - F * Math.tan(cam.pitch);
      T.GL.begin(w, h);
      const k = T.GL.w / w, lv = this.rows[0] ? this.rows[0].max : 0;
      const Cc = T.C, col = O.rgb;
      const sunX = w * 0.66, sunR = u * (sumi ? 3.2 + lv * 1.5 : 3.6 + S.energy * 2.5);
      T.GL.quad(this.tgSky, { t: performance.now() / 1000, hy: (h - hyCss) * k, sumi: sumi ? 1 : 0, lv,
        sun: [sunX * k, (h - hyCss + sunR * 0.45) * k, sunR * k], cBg: col(Cc.bg), cAcc: col(Cc.acc), cHot: col(Cc.hot), cAlert: col(Cc.alert) });
      const zFar = -2.2 + (R - 1) * 0.13;
      G.draw(this.tgP, cam, [w, h], [-1, 2 / (C - 1), zFar, -0.13], {
        zoff: this.rowT / 0.025, hs: 0.75, fitw: (w / 2) / F * 1.04, sumi: sumi ? 1 : 0, lv, t: performance.now() / 1000,
        cBg: col(Cc.bg), cAcc: col(Cc.acc), cHot: col(Cc.hot), cLine: col(sumi ? Cc.dim : Cc.line), cAlert: col(Cc.alert),
      });
      T.GL.draw(g, 0, 0, w, h);
      if (sumi) T.ink.hanko(g, w - u * 6.5, h - u * 7.5, u * 2.2, ['音'], { seed: 12, rot: -0.04 });
    },
    // SUMI: история спектра — гряды гор (сансуй): ближние тёмные, дальние тают в тумане, красное солнце
    terrainInk(g, w, h, u, top) {
      // «фейк»: кистью рисуем в слой ~15 раз в секунду, в остальные кадры кладём готовую картинку
      const cv = this.cv.cv, now = performance.now();
      if (!this.inkL) { this.inkL = document.createElement('canvas'); this.inkG = this.inkL.getContext('2d'); }
      if (this.inkL.width !== cv.width || this.inkL.height !== cv.height) { this.inkL.width = cv.width; this.inkL.height = cv.height; this.inkT = 0; }
      if (now - (this.inkT || 0) > 66) {
        this.inkT = now; const lg = this.inkG; lg.setTransform(1, 0, 0, 1, 0, 0); lg.clearRect(0, 0, cv.width, cv.height); lg.setTransform(this.cv.dpr, 0, 0, this.cv.dpr, 0, 0);
        this.terrainInkDraw(lg, w, h, u, top);
      }
      g.save(); g.setTransform(1, 0, 0, 1, 0, 0); g.drawImage(this.inkL, 0, 0); g.restore();
    },
    terrainInkDraw(g, w, h, u, top) {
      const I = T.ink, hz = top + (h - top) * 0.22, N = 14, step = Math.max(1, Math.floor(this.rows.length / N));
      const lv = this.rows[0] ? this.rows[0].max : 0, sr = u * (2.6 + lv * 2.4);
      I.raw(g, 'fillStyle', I.shu(0.85)); g.beginPath(); g.arc(w * 0.7, hz + u * 1.5, sr, 0, T.TAU); g.fill();
      const idx = []; for (let k = 0; k < N && k * step < this.rows.length; k++) idx.push(k * step);
      for (let j = idx.length - 1; j >= 0; j--) {
        const row = this.rows[idx[j]], t = j / Math.max(1, N - 1), y0 = hz + u * 2 + Math.pow(1 - t, 1.6) * (h - hz) * 0.82;
        const amp = u * (5 + 13 * (1 - t)), al = 0.1 + 0.75 * Math.pow(1 - t, 1.6), C = this.COLS, pts = [];
        for (let c = 0; c < C; c += 2) pts.push([-u * 2 + (w + u * 4) * c / (C - 1), y0 - amp * row[c] * 1.4]);
        I.raw(g, 'fillStyle', '#ece4d4'); g.beginPath(); g.moveTo(-u * 2, h); for (const p of pts) g.lineTo(p[0], p[1]); g.lineTo(w + u * 2, h); g.closePath(); g.fill();
        let mn = 1e9; for (const p of pts) mn = Math.min(mn, p[1]);
        const gr = g.createLinearGradient(0, mn, 0, y0 + u * 6), tt = T.theme; T.theme = 'raw'; gr.addColorStop(0, I.ink(al * 0.55)); gr.addColorStop(1, I.ink(0)); g.fillStyle = gr; T.theme = tt; g.fill();
        I.stroke(g, pts, u * (0.18 + 0.7 * (1 - t)), { seed: idx[j] * 31 + 7, taper: [0.02, 0.08], dry: 0.9, alpha: al });
        I.mist(g, y0 + u * 1.2, u * 2.2, w, 0.35 * (t + 0.2));
      }
      I.hanko(g, w - u * 6.5, h - u * 7.5, u * 2.2, ['音'], { seed: 12, rot: -0.04 });
    },
    // реактор: ядро пульсирует уровнем, круговой спектр, частицы вылетают на басах
    reactor(g, a, u, dt, t) {
      const cx = a.x + a.w / 2, cy = a.y + a.h / 2, Rm = Math.min(a.w, a.h) * 0.42;
      const lv = T.clamp((this.lvS + 60) / 60), bass = (S.bands[0] + S.bands[1] + S.bands[2] + S.bands[3]) / 4;
      if (bass - this.bassPrev > 0.05 && !T.reduce) {
        const k = Math.min(60, 15 + (bass - this.bassPrev) * 300);
        for (let i = 0; i < k; i++) { const an = Math.random() * T.TAU, sp = Rm * (0.5 + Math.random() * 1.1) * (0.6 + bass); this.parts.push({ x: cx, y: cy, vx: Math.cos(an) * sp, vy: Math.sin(an) * sp, life: 1, s: 0.6 + Math.random() * 1.6 }); }
        if (this.parts.length > 700) this.parts.splice(0, this.parts.length - 700);
      }
      this.bassPrev = T.lerp(this.bassPrev, bass, T.ease(dt, 0.12));
      this.spin += dt * (0.25 + lv * 1.2);
      const R0 = Rm * (0.2 + lv * 0.12);
      // орбиты: наклонённые эллипсы
      for (let k = 0; k < 3; k++) {
        const rr = Rm * (0.55 + k * 0.17), tilt = 0.32 + k * 0.12, rot = this.spin * (k % 2 ? -0.4 : 0.3) + k;
        g.save(); g.translate(cx, cy); g.rotate(rot); g.scale(1, tilt);
        g.strokeStyle = k === 2 ? 'rgba(62,230,255,.55)' : 'rgba(90,112,128,.45)'; g.lineWidth = 1.2 / Math.sqrt(tilt);
        g.beginPath(); g.arc(0, 0, rr, 0, T.TAU); g.stroke();
        g.restore();
        const pa = t * (0.6 + k * 0.3) * (k % 2 ? -1 : 1), ex = Math.cos(pa) * rr, ey = Math.sin(pa) * rr * tilt;   // «спутник» — без растяжения
        g.fillStyle = T.C.hot; g.beginPath(); g.arc(cx + ex * Math.cos(rot) - ey * Math.sin(rot), cy + ex * Math.sin(rot) + ey * Math.cos(rot), u * 0.25, 0, T.TAU); g.fill();
      }
      // круговой спектр
      const n = 32;
      for (let i = 0; i < n * 2; i++) {
        const b = S.bands[i < n ? i : 2 * n - 1 - i], an = this.spin * 0.2 + i / (n * 2) * T.TAU - Math.PI / 2;
        const r1 = R0 * 1.35, r2 = r1 + b * Rm * 0.38;
        g.strokeStyle = T.heat(0.3 + 0.7 * b); g.lineWidth = Math.max(2, R0 * 0.09);
        g.beginPath(); g.moveTo(cx + Math.cos(an) * r1, cy + Math.sin(an) * r1); g.lineTo(cx + Math.cos(an) * r2, cy + Math.sin(an) * r2); g.stroke();
      }
      // ядро
      const cg = g.createRadialGradient(cx, cy, 0, cx, cy, R0 * 1.25);
      cg.addColorStop(0, 'rgba(235,250,255,1)'); cg.addColorStop(0.45, this.lvS >= -15 ? 'rgba(255,122,26,.9)' : 'rgba(62,230,255,.9)'); cg.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = cg; g.beginPath(); g.arc(cx, cy, R0 * 1.25, 0, T.TAU); g.fill();
      g.strokeStyle = T.C.acc; g.lineWidth = 2; g.beginPath(); g.arc(cx, cy, R0 * 1.25, this.spin, this.spin + Math.PI * 1.4); g.stroke();
      // частицы
      g.fillStyle = T.C.acc;
      for (let i = this.parts.length - 1; i >= 0; i--) {
        const p = this.parts[i];
        p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= Math.exp(-dt * 1.4); p.vy *= Math.exp(-dt * 1.4); p.life -= dt * 0.75;
        if (p.life <= 0) { this.parts.splice(i, 1); continue; }
        g.globalAlpha = p.life; g.fillRect(p.x - p.s, p.y - p.s, p.s * 2, p.s * 2);
      }
      g.globalAlpha = 1;
      T.bigNum(g, Math.round(this.lvS), cx, cy + Rm * 1.02, u * 2.2, this.lvS >= -15 ? T.C.alert : T.C.hot, 'dBFS');
    },
    // осциллограф: гладкая линия, люминофор держит прошлые кадры
    scope(g, a, u, dt) {
      const tr = this.trW; tr.fit(); tr.fade(Math.min(1, dt * 11));
      const x = tr.x, mid = a.y + a.h / 2, amp = a.h * 0.42, gain = 1 / Math.max(0.15, Math.min(1, Math.max(...this.wave.map(Math.abs)) * 1.1));
      // сетка (на основном холсте — не размазывается)
      g.strokeStyle = 'rgba(32,50,64,.6)'; g.lineWidth = 1;
      for (let k = 0; k <= 10; k++) { const gx = a.x + a.w * k / 10; g.beginPath(); g.moveTo(gx, a.y); g.lineTo(gx, a.y + a.h); g.stroke(); }
      for (let k = 0; k <= 8; k++) { const gy = a.y + a.h * k / 8; g.beginPath(); g.moveTo(a.x, gy); g.lineTo(a.x + a.w, gy); g.stroke(); }
      g.strokeStyle = 'rgba(90,112,128,.6)'; g.beginPath(); g.moveTo(a.x, mid); g.lineTo(a.x + a.w, mid); g.stroke();
      x.beginPath();
      const P = i => [a.x + i / 63 * a.w, mid - T.clamp(this.wave[i] * gain, -1, 1) * amp];
      let p0 = P(0); x.moveTo(p0[0], p0[1]);
      for (let i = 1; i < 64; i++) { const p = P(i), mx = (p0[0] + p[0]) / 2, my = (p0[1] + p[1]) / 2; x.quadraticCurveTo(p0[0], p0[1], mx, my); p0 = p; }
      x.lineTo(p0[0], p0[1]);
      x.strokeStyle = 'rgba(62,230,255,.9)'; x.lineWidth = Math.max(2, u * 0.16); x.stroke();
      x.strokeStyle = 'rgba(235,250,255,.9)'; x.lineWidth = Math.max(1, u * 0.06); x.stroke();
      tr.draw(g);
      T.brackets(g, a.x, a.y, a.w, a.h, u * 1.2, 'rgba(62,230,255,.6)');
      T.txt(g, `×${gain.toFixed(1)}`, a.x + u * 0.6, a.y + u * 1.4, u * 0.8, T.C.label);
    },
    // XY: фигура из волны и её сдвинутой копии, с послесвечением
    xy(g, a, u, dt) {
      const tr = this.trX; tr.fit(); tr.fade(Math.min(1, dt * 4));
      const x = tr.x, s = Math.min(a.w, a.h) * 0.46, cx = a.x + a.w / 2, cy = a.y + a.h / 2, k = 16;
      const mx = Math.max(0.15, Math.max(...this.wave.map(Math.abs)));
      g.strokeStyle = 'rgba(32,50,64,.6)'; g.lineWidth = 1;
      g.strokeRect(cx - s, cy - s, s * 2, s * 2);
      g.beginPath(); g.moveTo(cx - s, cy); g.lineTo(cx + s, cy); g.moveTo(cx, cy - s); g.lineTo(cx, cy + s); g.stroke();
      g.beginPath(); g.arc(cx, cy, s, 0, T.TAU); g.stroke();
      x.beginPath();
      for (let i = 0; i <= 64; i++) {
        const j = i % 64, px = cx + this.wave[j] / mx * s * 0.92, py = cy - this.wave[(j + k) % 64] / mx * s * 0.92;
        i ? x.lineTo(px, py) : x.moveTo(px, py);
      }
      x.strokeStyle = 'rgba(62,230,255,.85)'; x.lineWidth = Math.max(1.5, u * 0.13); x.lineJoin = 'round'; x.stroke();
      x.strokeStyle = 'rgba(235,250,255,.8)'; x.lineWidth = Math.max(0.8, u * 0.05); x.stroke();
      tr.draw(g);
      T.brackets(g, cx - s, cy - s, s * 2, s * 2, u * 1.2, 'rgba(62,230,255,.6)');
      T.txt(g, 'X: t   Y: t + 90°', cx - s + u * 0.6, cy - s + u * 1.4, u * 0.8, T.C.label);
    },
  });
})();
