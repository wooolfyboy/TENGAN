// ---- GALLERY: чистая математика — считает только планшет ----
// Tesseract · Torus · Life · Pendulum — те же формулы, что раньше, в версии B:
// свечение, глубина, фосфорный след. Sky и Globe — небо и Земля прямо сейчас
// (T.astro: та же математика, что в прошивке; город и орбиты присылает палка).
T.views.push({
  id: 'gal', title: 'Gallery', subs: ['Tesseract', 'Torus', 'Life', 'Pendulum', 'Sky', 'Globe', 'Weather'], sub: 0,
  want() { return this.sub === 6 ? 'wx' : 'st'; },   // радиопогоде нужна палка, остальное считает планшет
  mount(main) {
    this.root = T.el('div', 'view');
    const st = T.el('div', 'stage'); this.root.appendChild(st); this.cv = T.canvas(st);
    main.appendChild(this.root);
    this.speed = 1; this.ph = 0; this.trail = []; this.trailT = 0; this.pd = null; this.life = null;
    this.dust = T.dust(160, 1.8, 1.4); this.dcam = T.orbit(0.3, 0.35, 3.4);
    this.skyRot = 0; this.skyRotT = 0; this.card = null;
    this.gl = { lon0: 30, lat0: 25, follow: -1, last: 0 };          // follow: -1 сами крутим, -2 дом, 0.. спутник
    const cv = this.cv.cv; let down = null;
    cv.addEventListener('pointerdown', e => { down = { x: e.offsetX, y: e.offsetY, moved: false, rot: this.skyRotT, lon: this.gl.lon0, lat: this.gl.lat0 }; cv.setPointerCapture(e.pointerId); });
    cv.addEventListener('pointermove', e => {
      if (!down) return;
      const dx = e.offsetX - down.x, dy = e.offsetY - down.y;
      if (Math.abs(dx) + Math.abs(dy) > 7) down.moved = true;
      if (!down.moved) return;
      if (this.sub === 4) { this.skyRotT = down.rot - dx * 0.25; this.skyRot = this.skyRotT; }
      if (this.sub === 5) { this.gl.follow = -1; this.gl.lon0 = down.lon - dx * 0.3; this.gl.lat0 = T.clamp(down.lat + dy * 0.3, -70, 70); this.gl.last = performance.now(); }
    });
    const up = e => { if (!down) return; const d = down; down = null; if (!d.moved && e.type === 'pointerup') this.tap(e.offsetX, e.offsetY); };
    cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  onSub() { this.trail = []; this.pd = null; this.card = null; },
  tap(x, y) {
    const hit = T.hits.at(x, y);
    if (hit && hit.act) {
      if (hit.act === '-') this.speed = Math.max(0.25, +(this.speed - 0.25).toFixed(2));
      if (hit.act === '+') this.speed = Math.min(4, +(this.speed + 0.25).toFixed(2));
      if (hit.act === 'new') { if (this.sub === 2) this.life = null; if (this.sub === 3) this.pd = null; }
      return;
    }
    if (this.sub === 2) { this.life = null; return; }
    if (this.sub === 3) { this.pd = null; return; }
    if (this.sub === 4) { this.card = hit && hit.sky ? hit.sky : null; return; }
    if (this.sub === 5) { if (hit && hit.follow != null) this.gl.follow = this.gl.follow === hit.follow ? -1 : hit.follow; return; }
  },
  frame(dt, t) {
    const o = this.cv, { g, w, h } = o;
    if (w < 40 || h < 40) return;
    T.hits.clear();
    const u = T.unit(w, h), m = u * 1.6, y0 = 50;
    this.ph += dt * this.speed;
    this.dust.tick(dt * this.speed); this.dcam.yaw += dt * 0.05;
    const L = { u, m, y0, top: y0 + u * 3.2, bot: h - m * 0.6 - u * 2.2 };
    T.backdrop(g, w, h, 0.5, 0.5);
    let st = '';
    if (this.sub === 0) st = this.tess(g, w, h, L, dt);
    else if (this.sub === 1) st = this.torus(g, w, h, L);
    else if (this.sub === 2) st = this.lifeF(g, w, h, L, dt);
    else if (this.sub === 3) st = this.pend(g, w, h, L, dt);
    else if (this.sub === 4) st = this.sky(g, w, h, L, dt, t);
    else if (this.sub === 5) st = this.globe(g, w, h, L, dt, t);
    else st = this.weather(g, w, h, L, dt, t);                 // views/weather.js
    T.bloom(o, this.sub === 2 ? 0.7 : this.sub === 6 ? (T.theme === 'sumi' ? 0.35 : 0.8) : this.sub >= 4 ? 0.6 : 0.85);
    // HUD поверх — плоский и чёткий
    T.header(g, m, y0, w - 2 * m, u, this.subs[this.sub], st.status);
    T.dots(g, m, y0 + u * 2.75, u, this.subs.length, this.sub);
    if (this.sub <= 3) this.speedCtl(g, w, h, L);
    if (st.draw) st.draw();
    T.footer(g, m, h - m * 0.6 - u * 1.6, w - 2 * m, u, st.hint || '', st.right || 'math · on tablet');
    T.finish(g, w, h);
  },
  // кнопки скорости (и «новый мир») прямо в кадре, справа внизу
  speedCtl(g, w, h, L) {
    const { u, m } = L, bh = u * 2.2, y = h - m * 0.6 - u * 1.6 - bh - u * 0.8;
    let x = w - m;
    const btn = (label, act, bw, on) => {
      x -= bw;
      T.panel(g, x, y, bw, bh, u * 0.6, on ? 'rgba(62,230,255,.12)' : 'rgba(5,8,13,.55)', on ? T.C.acc : 'rgba(62,230,255,.35)');
      T.txt(g, label, x + bw / 2, y + bh * 0.68, u * 1.15, on ? T.C.hot : T.C.acc, 't', 'center');
      if (act) T.hits.rect(x, y, bw, bh, { act });
      x -= u * 0.5;
    };
    btn('+', '+', bh); btn('×' + this.speed.toFixed(2).replace(/0$/, ''), null, u * 4.6); btn('−', '-', bh);
    if (this.sub === 2 || this.sub === 3) btn(this.sub === 2 ? 'NEW WORLD' : 'NEW SWING', 'new', u * 7.5, true);
  },

  // ---------- 4D-гиперкуб: вращение в XW, YW, ZW, XY; перспектива 4D->3D->экран ----------
  tess(g, w, h, L, dt) {
    const { u, top, bot } = L;
    this.dcam.frame(w / 2, (top + bot) / 2 + u * 2, Math.min(w, h) * 1.1);
    this.dust.draw(g, this.dcam);
    const t = this.ph, A = [t * 0.9, t * 0.55, t * 0.35, t * 0.25], cx = w / 2, cy = (top + bot) / 2, K = Math.min(w, bot - top) * 0.5;
    const P = [];
    for (let i = 0; i < 16; i++) {
      let x = i & 1 ? 1 : -1, y = i & 2 ? 1 : -1, z = i & 4 ? 1 : -1, ww = i & 8 ? 1 : -1, c, s, tx;
      c = Math.cos(A[0]); s = Math.sin(A[0]); tx = x * c - ww * s; ww = x * s + ww * c; x = tx;
      c = Math.cos(A[1]); s = Math.sin(A[1]); tx = y * c - ww * s; ww = y * s + ww * c; y = tx;
      c = Math.cos(A[2]); s = Math.sin(A[2]); tx = z * c - ww * s; ww = z * s + ww * c; z = tx;
      c = Math.cos(A[3]); s = Math.sin(A[3]); tx = x * c - y * s; y = x * s + y * c; x = tx;
      const k4 = 2.6 / (3.2 - ww); x *= k4; y *= k4; z *= k4;
      c = Math.cos(0.5); s = Math.sin(0.5); tx = y * c - z * s; z = y * s + z * c; y = tx;
      const k3 = K / (5.2 - z) * 2.2;
      P.push([cx + x * k3, cy + y * k3, ww, z]);
    }
    // фосфорный след: кадр раз в 1/60 с, независимо от частоты экрана
    this.trailT += dt;
    if (this.trailT >= 1 / 60 || !this.trail.length) { this.trailT = 0; this.trail.unshift(P); if (this.trail.length > 14) this.trail.pop(); } else this.trail[0] = P;
    g.lineCap = 'round';
    for (let f = this.trail.length - 1; f >= 1; f--) {
      const Q = this.trail[f]; g.strokeStyle = `rgba(62,230,255,${0.16 * (1 - f / 14)})`; g.lineWidth = 1; g.beginPath();
      for (let i = 0; i < 16; i++) for (let b = 0; b < 4; b++) { const j = i ^ (1 << b); if (j < i) continue; g.moveTo(Q[i][0], Q[i][1]); g.lineTo(Q[j][0], Q[j][1]); }
      g.stroke();
    }
    // рёбра: дальние (по w) — тусклее и тоньше
    const E = [];
    for (let i = 0; i < 16; i++) for (let b = 0; b < 4; b++) { const j = i ^ (1 << b); if (j > i) E.push([i, j, (P[i][2] + P[j][2]) / 2]); }
    E.sort((a, b) => a[2] - b[2]);
    for (const [i, j, wv] of E) {
      const v = T.clamp(0.45 + wv * 0.35);
      g.strokeStyle = T.heat(v, 0.85); g.lineWidth = u * (0.1 + 0.12 * v);
      g.beginPath(); g.moveTo(P[i][0], P[i][1]); g.lineTo(P[j][0], P[j][1]); g.stroke();
    }
    for (const p of P) { const v = T.clamp(0.5 + p[2] * 0.4); g.fillStyle = T.heat(v); g.beginPath(); g.arc(p[0], p[1], u * (0.22 + 0.22 * v), 0, T.TAU); g.fill(); }
    g.lineCap = 'butt';
    const deg = a => String(Math.round(((a * 180 / Math.PI) % 360 + 360) % 360)).padStart(3, '0');
    return {
      status: '4D hypercube · 16 vertices · 32 edges',
      hint: '±: speed', right: 'xw ' + deg(A[0]) + '°  yw ' + deg(A[1]) + '°  zw ' + deg(A[2]) + '°  xy ' + deg(A[3]) + '°',
    };
  },

  // ---------- тор из точек: освещение и глубина (как donut.c), 3600 точек ----------
  torus(g, w, h, L) {
    const { u, top, bot } = L;
    this.dcam.frame(w / 2, (top + bot) / 2 + u * 2, Math.min(w, h) * 1.1);
    this.dust.draw(g, this.dcam);
    const Aa = this.ph * 0.8, Bb = this.ph * 0.45, cA = Math.cos(Aa), sA = Math.sin(Aa), cB = Math.cos(Bb), sB = Math.sin(Bb);
    const R1 = 1, R2 = 2.1, K2 = 6.5, K1 = Math.min(w, bot - top) * 0.88, cx = w / 2, cy = (top + bot) / 2, U = 120, V = 30;
    const pts = this.tp || (this.tp = new Float32Array(U * V * 4));
    let n = 0;
    for (let i = 0; i < V; i++) {
      const th = i * 2 * Math.PI / V, ct = Math.cos(th), st = Math.sin(th);
      for (let j = 0; j < U; j++) {
        const ph = j * 2 * Math.PI / U + i * 0.06, cp = Math.cos(ph), sp = Math.sin(ph), ox = R2 + R1 * ct, oy = R1 * st;
        const x = ox * (cB * cp + sA * sB * sp) - oy * cA * sB, y = ox * (sB * cp - sA * cB * sp) + oy * cA * cB, z = K2 + cA * ox * sp + oy * sA;
        const ny = ct * (sB * cp - sA * cB * sp) + st * cA * cB, nz = cA * ct * sp + st * sA;
        pts[n++] = cx + K1 * x / z; pts[n++] = cy - K1 * y / z; pts[n++] = z; pts[n++] = ny * 0.7071 - nz * 0.7071;
      }
    }
    const N = U * V, idx = this.ti || (this.ti = new Uint16Array(N));
    for (let i = 0; i < N; i++) idx[i] = i;
    idx.sort((a, b) => pts[b * 4 + 2] - pts[a * 4 + 2]);
    // точки по 12 уровням яркости: один путь на уровень
    const B = 12, paths = this._tb || (this._tb = Array.from({ length: B }, () => []));
    for (const p of paths) p.length = 0;
    const s = u / 16;
    for (let k = 0; k < N; k++) {
      const q = idx[k] * 4, depth = T.clamp((9.5 - pts[q + 2]) / 7, 0.15, 1), Lm = Math.max(0, pts[q + 3]);
      const v = T.clamp(0.08 + 0.92 * Lm * depth + 0.12 * depth);
      paths[Math.min(B - 1, v * B | 0)].push(q);
    }
    for (let b = 0; b < B; b++) {
      const v = (b + 0.5) / B, r = (0.9 + v * 2.6) * s;
      g.fillStyle = T.heat(v, 0.35 + 0.65 * v); g.beginPath();
      for (const q of paths[b]) g.rect(pts[q] - r / 2, pts[q + 1] - r / 2, r, r);
      g.fill();
    }
    return { status: '3600 points · light + depth', hint: '±: speed', right: 'R1 1.0  R2 2.1  K2 6.5' };
  },

  // ---------- «Жизнь» Конвея: большое поле, светящиеся клетки и угасающие призраки ----------
  lifeF(g, w, h, L, dt) {
    const { u, m, top, bot } = L;
    const portrait = h > w * 1.05, W = portrait ? 200 : 320, H = portrait ? 320 : 200;
    const aw = w - 2 * m, ah = bot - top - u * 3.4;
    const cell = Math.min(aw / W, ah / H);
    if (W < 8 || H < 8 || cell <= 0) return {};
    let Lf = this.life;
    if (!Lf || Lf.W !== W || Lf.H !== H) {
      Lf = this.life = { W, H, a: new Uint8Array(W * H), b: new Uint8Array(W * H), glow: new Float32Array(W * H), t: 0, gen: 0, still: 0, prev: -1, pop: [],
        img: new ImageData(W, H), off: Object.assign(document.createElement('canvas'), { width: W, height: H }) };
      for (let i = 0; i < W * H; i++) Lf.a[i] = Math.random() < 0.28 ? 1 : 0;
    }
    Lf.t += dt * this.speed * 12;
    let steps = 0;
    while (Lf.t >= 1 && steps++ < 4) {
      Lf.t -= 1; Lf.gen++; let pop = 0;
      for (let y = 0; y < H; y++) {
        const ym = ((y + H - 1) % H) * W, yy = y * W, yp = ((y + 1) % H) * W;
        for (let x = 0; x < W; x++) {
          const xm = (x + W - 1) % W, xp = (x + 1) % W;
          const nb = (Lf.a[ym + xm] > 0) + (Lf.a[ym + x] > 0) + (Lf.a[ym + xp] > 0) + (Lf.a[yy + xm] > 0) + (Lf.a[yy + xp] > 0) + (Lf.a[yp + xm] > 0) + (Lf.a[yp + x] > 0) + (Lf.a[yp + xp] > 0);
          const cur = Lf.a[yy + x];
          const nv = cur ? (nb === 2 || nb === 3 ? Math.min(cur + 1, 255) : 0) : (nb === 3 ? 1 : 0);
          if (cur && !nv) Lf.glow[yy + x] = 1;
          Lf.b[yy + x] = nv; if (nv) pop++;
        }
      }
      [Lf.a, Lf.b] = [Lf.b, Lf.a];
      Lf.still = Math.abs(pop - Lf.prev) <= 2 ? Lf.still + 1 : 0; Lf.prev = pop;
      Lf.pop.push(pop); if (Lf.pop.length > 240) Lf.pop.shift();
      if (pop < 30) { this.life = null; return {}; }
      if (Lf.still > 60) {                                // застой — подбросить «кляксы»
        for (let k = 0; k < 8; k++) { const cx0 = Math.random() * W | 0, cy0 = Math.random() * H | 0; for (let q = 0; q < 40; q++) Lf.a[((cy0 + (Math.random() * 7 | 0)) % H) * W + (cx0 + (Math.random() * 7 | 0)) % W] = 1; }
        Lf.still = 0;
      }
    }
    if (Lf.t > 4) Lf.t = 0;
    const D = Lf.img.data, fade = Math.pow(0.05, dt);
    for (let i = 0; i < W * H; i++) {
      const a = Lf.a[i], ink = T.theme === 'sumi';          // тушь: бумага, живые клетки — тушь, «призраки» — размывка
      let r = ink ? 236 : 5, gg = ink ? 228 : 8, bb = ink ? 212 : 13;
      if (a) { const c = a === 1 ? (ink ? [27, 24, 20] : [235, 250, 255]) : T.heatRGB(Math.max(0.42, 0.95 - a * 0.025)); r = c[0]; gg = c[1]; bb = c[2]; }
      else if (Lf.glow[i] > 0.01) { const k = Lf.glow[i] * 0.5; if (ink) { r = 236 - 70 * k; gg = 228 - 70 * k; bb = 212 - 66 * k; } else { r = 5 + 35 * k; gg = 8 + 87 * k; bb = 13 + 117 * k; } Lf.glow[i] *= fade; }
      D[i * 4] = r; D[i * 4 + 1] = gg; D[i * 4 + 2] = bb; D[i * 4 + 3] = 255;
    }
    Lf.off.getContext('2d').putImageData(Lf.img, 0, 0);
    const fw = W * cell, fh = H * cell, fx = (w - fw) / 2, fy = top + u * 0.6;
    g.imageSmoothingEnabled = false;
    g.drawImage(Lf.off, fx, fy, fw, fh);
    g.imageSmoothingEnabled = true;
    T.brackets(g, fx - u * 0.4, fy - u * 0.4, fw + u * 0.8, fh + u * 0.8, u * 1.2, 'rgba(62,230,255,.6)', 1.5);
    const pop = Lf.pop;
    return {
      status: `gen ${Lf.gen}  ·  cells ${Lf.prev < 0 ? '—' : Lf.prev}  ·  ${W}×${H}`,
      hint: 'tap: new world  ·  ±: speed', right: 'b3/s23 · torus field',
      draw: () => {
        // график населения — слева внизу, на уровне кнопок
        const pw = Math.min(u * 16, w * 0.3), ph = u * 2.2, px = m, py = h - m * 0.6 - u * 1.6 - ph - u * 0.8;
        if (pop.length > 2) {
          const lo = Math.min(...pop), hi = Math.max(...pop) + 1;
          T.panel(g, px, py, pw, ph, u * 0.6, 'rgba(5,8,13,.55)');
          T.spark(g, px + u * 0.5, py + u * 0.35, pw - u, ph - u * 0.7, pop, lo - (hi - lo) * 0.2, hi);
        }
      },
    };
  },

  // ---------- двойной маятник: Лагранж + Рунге-Кутта 4, длинный светящийся след ----------
  pend(g, w, h, L, dt) {
    const { u, top, bot } = L;
    if (!this.pd) { this.pd = { y: [2 + Math.random(), 1.5 + Math.random() * 1.6, 0, 0] }; this.trail = []; }
    const f = y => { const gr = 9.81, [t1, t2, w1, w2] = y, d = t1 - t2, den = 3 - Math.cos(2 * d);
      return [w1, w2, (-gr * 3 * Math.sin(t1) - gr * Math.sin(t1 - 2 * t2) - 2 * Math.sin(d) * (w2 * w2 + w1 * w1 * Math.cos(d))) / den,
        (2 * Math.sin(d) * (w1 * w1 * 2 + gr * 2 * Math.cos(t1) + w2 * w2 * Math.cos(d))) / den]; };
    const R = Math.min(w, bot - top) * 0.22, cx = w / 2, cy = top + (bot - top) * 0.5 - R * 0.1, steps = 8, hs = Math.min(dt, 0.05) * this.speed / steps;
    for (let s = 0; s < steps; s++) {
      const y = this.pd.y, k1 = f(y), k2 = f(y.map((v, i) => v + k1[i] * hs / 2)), k3 = f(y.map((v, i) => v + k2[i] * hs / 2)), k4 = f(y.map((v, i) => v + k3[i] * hs));
      this.pd.y = y.map((v, i) => v + hs / 6 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]));
    }
    // точка следа ~240 раз в секунду (не зависит от частоты экрана)
    this.trailT += dt;
    const [t1, t2, w1, w2] = this.pd.y, x1 = cx + R * Math.sin(t1), y1 = cy + R * Math.cos(t1), x2 = x1 + R * Math.sin(t2), y2 = y1 + R * Math.cos(t2);
    if (this.trailT >= 1 / 240) { this.trailT = 0; this.trail.push([x2 - cx, y2 - cy, Math.min(1, (Math.abs(w2) + Math.abs(w1) * 0.5) / 12)]); }
    if (this.trail.length > 2400) this.trail.splice(0, this.trail.length - 2400);
    // круг досягаемости и сетка
    g.strokeStyle = T.C.line; g.lineWidth = 1; g.beginPath(); g.arc(cx, cy, 2 * R, 0, T.TAU); g.stroke();
    g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.setLineDash([2, 6]); g.stroke(); g.setLineDash([]);
    // след: пачки по возрасту и скорости — один путь на пачку
    const n = this.trail.length, AG = 16, SP = 4;
    g.lineCap = 'round';
    for (let a = 0; a < AG; a++) for (let s = 0; s < SP; s++) {
      g.beginPath(); let any = false;
      const i0 = Math.max(1, Math.floor(a * n / AG)), i1 = Math.floor((a + 1) * n / AG);
      for (let i = i0; i < i1; i++) {
        const p = this.trail[i - 1], q = this.trail[i];
        if (Math.min(SP - 1, q[2] * SP | 0) !== s) continue;
        g.moveTo(cx + p[0], cy + p[1]); g.lineTo(cx + q[0], cy + q[1]); any = true;
      }
      if (!any) continue;
      const age = (a + 1) / AG;
      g.strokeStyle = T.heat(0.35 + 0.65 * (s + 0.5) / SP, 0.05 + 0.95 * age * age); g.lineWidth = u * (0.05 + 0.16 * age);
      g.stroke();
    }
    g.strokeStyle = T.C.label; g.lineWidth = u * 0.16; g.beginPath(); g.moveTo(cx, cy); g.lineTo(x1, y1); g.lineTo(x2, y2); g.stroke();
    g.lineCap = 'butt';
    T.diamond(g, cx, cy, u * 0.35, T.C.dim, true);
    g.fillStyle = T.C.acc; g.beginPath(); g.arc(x1, y1, u * 0.55, 0, T.TAU); g.fill();
    g.fillStyle = T.C.hot; g.beginPath(); g.arc(x2, y2, u * 0.7, 0, T.TAU); g.fill();
    // энергия (в единицах m = l = 1): видно, что RK4 держит её почти постоянной
    const E = 0.5 * (2 * w1 * w1 + w2 * w2 + 2 * w1 * w2 * Math.cos(t1 - t2)) - 9.81 * (2 * Math.cos(t1) + Math.cos(t2));
    const dg = a => Math.round((((a * 180 / Math.PI) % 360) + 540) % 360 - 180);
    return { status: `θ1 ${dg(t1)}°  θ2 ${dg(t2)}°  ·  E ${E.toFixed(2)}`, hint: 'tap: new swing  ·  ±: speed', right: 'lagrange · rk4 · chaos' };
  },

  // ======================================================================
  //  SKY — купол над городом прямо сейчас (зенит в центре, север вверху, восток слева)
  // ======================================================================
  skyCalc(now) {
    const c = this.sc;
    if (c && Math.abs(now - c.t) < 1) return c;
    const A = T.astro, D = A.data(), P = A.place(), jd = A.jd(now), gm = A.gmst(jd);
    const aa = (ra, dec) => A.altAz(ra, dec, jd, P.lat, P.lon, gm);
    const stars = D.stars.map(s => aa(s.ra, s.dec));
    const lines = D.lines.map(Lp => Lp.map(p => aa(p[0], p[1])));
    const sun = A.sun(jd), sunA = aa(sun.ra, sun.dec);
    const mo = A.moon(jd), moA = aa(mo.ra, mo.dec);
    const pl = A.PLANET_RU.map((_, i) => { const r = A.planet(i, jd); return aa(r.ra, r.dec); });
    const con = D.con.map(k => aa(k.ra, k.dec));
    // спутники: след ±10 минут по небу
    const sats = A.sats().map(s => { const tr = []; for (let k = -20; k <= 20; k++) tr.push(A.satLook(A.satPos(s, now + k * 30), P.lat, P.lon)); return { s, tr }; });
    return (this.sc = { t: now, P, D, stars, lines, sun: sunA, moon: moA, phase: mo.phase, waxing: mo.waxing, pl, con, sats });
  },
  sky(g, w, h, L, dt, tt) {
    const { u, m, top, bot } = L, A = T.astro, now = A.now(), c = this.skyCalc(now);
    this.skyRot += (this.skyRotT - this.skyRot) * T.ease(dt, 0.25);
    const portrait = h > w * 1.05;
    const R = portrait ? Math.min(w / 2 - m - u * 2, (bot - top) * 0.3) : Math.min((bot - top) / 2 - u * 1.8, w * 0.3);
    const cx = portrait ? w / 2 : m + R + u * 3, cy = portrait ? top + R + u * 2.2 : (top + bot) / 2 + u * 0.3;
    const rot = this.skyRot, D2R = Math.PI / 180;
    const xy = (a) => { if (a.alt < 0) return null; const r = R * Math.tan((90 - a.alt) * D2R / 2), q = (a.az - rot) * D2R; return [cx - r * Math.sin(q), cy - r * Math.cos(q)]; };
    const sunAlt = c.sun.alt, day = sunAlt > -6, night = sunAlt < -12;
    // купол: цвет неба по высоте Солнца
    const sky0 = day ? [16, 52, 72] : night ? [6, 14, 22] : [10, 30, 44], sky1 = day ? [8, 26, 38] : night ? [4, 9, 15] : [6, 18, 28];
    const bg = g.createRadialGradient(cx, cy, 0, cx, cy, R); bg.addColorStop(0, T.rgb(sky1)); bg.addColorStop(1, T.rgb(sky0));
    g.fillStyle = bg; g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.fill();
    // зарево у горизонта со стороны Солнца (сумерки)
    if (sunAlt > -18 && sunAlt < 8) {
      const q = (c.sun.az - rot) * D2R, gx = cx - R * Math.sin(q), gy = cy - R * Math.cos(q), k = 1 - Math.abs(sunAlt + 5) / 13;
      const gr = g.createRadialGradient(gx, gy, 0, gx, gy, R * 0.9); gr.addColorStop(0, `rgba(62,230,255,${0.22 * T.clamp(k)})`); gr.addColorStop(1, 'rgba(62,230,255,0)');
      g.save(); g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.clip(); g.fillStyle = gr; g.fillRect(cx - R, cy - R, 2 * R, 2 * R); g.restore();
    }
    // круги высоты 30° и 60°
    g.strokeStyle = 'rgba(90,112,128,.35)'; g.lineWidth = 1; g.setLineDash([2, 5]);
    for (const alt of [30, 60]) { g.beginPath(); g.arc(cx, cy, R * Math.tan((90 - alt) * D2R / 2), 0, T.TAU); g.stroke(); }
    g.setLineDash([]);
    // линии созвездий
    g.strokeStyle = `rgba(62,230,255,${day ? 0.16 : 0.32})`; g.lineWidth = 1; g.beginPath();
    for (const Lp of c.lines) { let have = false; for (const a of Lp) { const p = xy(a); if (p) { have ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); have = true; } else have = false; } }
    g.stroke();
    // звёзды: днём видны только самые яркие; яркие мерцают
    const magLim = day ? 1.5 : night ? 5.2 : 3.0, D = c.D, s = u / 16, buckets = [[], [], [], []];
    for (let i = 0; i < D.stars.length; i++) {
      const st = D.stars[i]; if (st.mag > magLim) break;
      const p = xy(c.stars[i]); if (!p) continue;
      if (st.mag < 2.2) {
        const tw = 0.75 + 0.25 * Math.sin(tt * (2.2 + (i % 7) * 0.4) + i * 1.7), r = s * Math.max(1.2, (2.6 - st.mag) * 1.6) * (0.85 + 0.15 * tw);
        g.fillStyle = T.heat(0.7 + 0.3 * tw * T.clamp((2.2 - st.mag) / 3)); g.beginPath(); g.arc(p[0], p[1], r, 0, T.TAU); g.fill();
        if (st.mag < 0.8) { g.strokeStyle = `rgba(235,250,255,${0.35 * tw})`; g.lineWidth = 1; g.beginPath(); g.moveTo(p[0] - r * 3, p[1]); g.lineTo(p[0] + r * 3, p[1]); g.moveTo(p[0], p[1] - r * 3); g.lineTo(p[0], p[1] + r * 3); g.stroke(); }
        if (D.names[i]) T.hits.circle(p[0], p[1], u * 1.4, { sky: { kind: 'star', name: D.names[i], mag: st.mag, a: c.stars[i] } });
      } else buckets[Math.min(3, (st.mag - 2.2) / 0.8 | 0)].push(p);
    }
    buckets.forEach((b, k) => { const r = s * (1.5 - k * 0.25); g.fillStyle = T.heat(0.62 - k * 0.1); g.beginPath(); for (const p of b) g.rect(p[0] - r / 2, p[1] - r / 2, r, r); g.fill(); });
    // подписи без наложений
    const used = [];
    const label = (txt, x, y, px, col, f = 'c') => {
      const tw2 = T.tw(g, txt, px, f), bx = x - tw2 / 2, by = y - px;
      for (const q of used) if (bx < q[0] + q[2] && q[0] < bx + tw2 && by < q[1] + q[3] && q[1] < by + px * 1.2) return;
      used.push([bx, by, tw2, px * 1.2]); T.txt(g, txt, x, y, px, col, f, 'center');
    };
    // планеты
    const plUp = [];
    c.pl.forEach((a, i) => {
      const p = xy(a); if (!p) return;
      plUp.push([i, a.alt]);
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(p[0], p[1], u * 0.3, 0, T.TAU); g.fill();
      g.strokeStyle = T.C.acc; g.lineWidth = 1.2; g.beginPath(); g.arc(p[0], p[1], u * 0.65, 0, T.TAU); g.stroke();
      label(A.PLANET_RU[i], p[0], p[1] - u * 1, u * 0.78, T.C.hot);
      T.hits.circle(p[0], p[1], u * 1.5, { sky: { kind: 'planet', name: A.PLANET_RU[i], a } });
    });
    // Луна с фазой
    let p = xy(c.moon);
    if (p) {
      const r = u * 0.9, k = c.phase;
      g.fillStyle = 'rgba(235,250,255,.12)'; g.beginPath(); g.arc(p[0], p[1], r, 0, T.TAU); g.fill();
      g.save(); g.translate(p[0], p[1]); if (!c.waxing) g.scale(-1, 1);
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(0, 0, r, -Math.PI / 2, Math.PI / 2); g.ellipse(0, 0, r * Math.abs(1 - 2 * k), r, 0, Math.PI / 2, -Math.PI / 2, k > 0.5); g.fill();
      g.restore();
      label('Луна ' + Math.round(k * 100) + '%', p[0], p[1] - r - u * 0.5, u * 0.78, T.C.label);
      T.hits.circle(p[0], p[1], u * 1.6, { sky: { kind: 'moon', name: 'Луна', a: c.moon, phase: k } });
    }
    // Солнце
    p = xy(c.sun);
    if (p) {
      const gr = g.createRadialGradient(p[0], p[1], 0, p[0], p[1], u * 3); gr.addColorStop(0, 'rgba(235,250,255,.9)'); gr.addColorStop(0.25, 'rgba(62,230,255,.35)'); gr.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = gr; g.beginPath(); g.arc(p[0], p[1], u * 3, 0, T.TAU); g.fill();
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(p[0], p[1], u * 0.8, 0, T.TAU); g.fill();
      T.hits.circle(p[0], p[1], u * 2, { sky: { kind: 'sun', name: 'Солнце', a: c.sun } });
    }
    // спутники над горизонтом: след и точка «сейчас»
    const satsUp = [];
    for (const { s: sat, tr } of c.sats) {
      g.strokeStyle = 'rgba(255,255,255,.0)'; g.lineWidth = 1.2;
      let have = false; g.beginPath();
      for (const a of tr) { const q = xy({ alt: a.el, az: a.az }); if (q) { have ? g.lineTo(q[0], q[1]) : g.moveTo(q[0], q[1]); have = true; } else have = false; }
      g.strokeStyle = 'rgba(62,230,255,.45)'; g.setLineDash([4, 4]); g.stroke(); g.setLineDash([]);
      const look = A.satLook(A.satPos(sat, now), c.P.lat, c.P.lon), q = xy({ alt: look.el, az: look.az });
      if (q) {
        satsUp.push([sat.name, look.el]);
        const pulse = 0.5 + 0.5 * Math.sin(tt * 5);
        T.diamond(g, q[0], q[1], u * 0.55, T.C.hot, true);
        g.strokeStyle = `rgba(235,250,255,${1 - pulse})`; g.lineWidth = 1.2; g.beginPath(); g.arc(q[0], q[1], u * (0.8 + pulse * 1.2), 0, T.TAU); g.stroke();
        label(sat.name, q[0], q[1] - u * 1.1, u * 0.78, T.C.hot, 'm');
        T.hits.circle(q[0], q[1], u * 1.6, { sky: { kind: 'sat', name: sat.name, a: { alt: look.el, az: look.az } } });
      }
    }
    // созвездия (по-русски)
    if (!day) D.con.forEach((k, i) => { const a = c.con[i]; if (a.alt < 12) return; const q = xy(a); if (q) label(k.ru, q[0], q[1], u * 0.7, 'rgba(62,230,255,.6)'); });
    // горизонт: кольцо с рисками и стороны света
    g.strokeStyle = T.C.acc; g.lineWidth = 1.6; g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.stroke();
    g.lineWidth = 1; g.beginPath();
    for (let az = 0; az < 360; az += 10) { const q = (az - rot) * D2R, l = az % 30 ? 1.025 : 1.05; g.moveTo(cx - R * Math.sin(q), cy - R * Math.cos(q)); g.lineTo(cx - R * l * Math.sin(q), cy - R * l * Math.cos(q)); }
    g.strokeStyle = T.C.dim; g.stroke();
    ['N', 'E', 'S', 'W'].forEach((sd, k) => { const q = (k * 90 - rot) * D2R; T.txt(g, sd, cx - (R + u * 1.6) * Math.sin(q), cy - (R + u * 1.6) * Math.cos(q) + u * 0.45, u * 1.2, k === 0 ? T.C.hot : T.C.acc, 't', 'center'); });
    // карточка выбранного светила
    const card = this.card;
    const d = new Date(), hh = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0') + ':' + String(d.getSeconds()).padStart(2, '0');
    const state = day ? (sunAlt > 0 ? 'DAY' : 'TWILIGHT') : night ? 'NIGHT' : 'TWILIGHT';
    return {
      status: `${c.P.city} · ${c.P.lat.toFixed(1)}° ${c.P.lon.toFixed(1)}° · ${state}`,
      hint: 'drag: turn the dome  ·  tap: star / planet', right: 'stars ' + D.stars.length + ' · jpl + meeus',
      draw: () => {
        // панель справа (или снизу в портрете)
        const pw = portrait ? w - 2 * m : Math.min(u * 22, w - cx - R - m - u * 5), px = portrait ? m : w - m - pw;
        const py = portrait ? cy + R + u * 3 : top + u * 0.4, ph = portrait ? Math.min(u * 24, Math.max(u * 10, bot - py - u * 3.4)) : Math.min(bot - top - u * 3.6, u * (18 + 1.5 * (plUp.length + A.sats().length)));
        if (pw < u * 10) return;
        T.panel(g, px, py, pw, ph, u);
        let y = py + u * 3.4;
        T.txt(g, hh, px + u, y, u * 3, T.C.hot, 't');
        T.txt(g, 'LOCAL TIME · ' + d.toLocaleDateString('ru-RU'), px + u, y + u * 1.2, u * 0.72, T.C.label);
        y += u * 3;
        const row = (k, v, col = T.C.hot) => { if (y > py + ph - u * 0.8) return; T.txt(g, k, px + u, y, u * 0.8, T.C.dim); T.txt(g, v, px + pw - u, y, u * 0.85, col, 'c', 'right'); y += u * 1.5; };
        row('SUN', `${sunAlt >= 0 ? '+' : ''}${Math.round(sunAlt)}°  ${state}`);
        row('MOON', `${Math.round(c.phase * 100)}%  ${c.moon.alt >= 0 ? '+' : ''}${Math.round(c.moon.alt)}°  ${c.waxing ? 'растёт' : 'убывает'}`);
        y += u * 0.4;
        T.txt(g, plUp.length ? 'PLANETS UP' : 'NO PLANETS UP', px + u, y, u * 0.75, T.C.acc); y += u * 1.4;
        plUp.sort((a, b) => b[1] - a[1]).forEach(([i, alt]) => row(A.PLANET_RU[i], Math.round(alt) + '°'));
        y += u * 0.4;
        if (y < py + ph - u * 2) {
          const sats = A.sats();
          T.txt(g, sats.length ? 'SATELLITES' : 'NO ORBITS · LAPTOP SENDS TLE', px + u, y, u * 0.75, T.C.acc); y += u * 1.4;
          for (const sat of sats) { const up = satsUp.find(q => q[0] === sat.name); row(sat.name, up ? 'UP ' + Math.round(up[1]) + '°' : 'below', up ? T.C.hot : T.C.dim); }
        }
        if (card) {
          const a = card.a, cw = u * 15, ch = u * 6.2, cxx = portrait ? m : cx - cw / 2, cyy = portrait ? py - ch - u * 0.6 : bot - ch - u * 3.6;
          T.panel(g, cxx, cyy, cw, ch, u, 'rgba(5,8,13,.78)', T.C.acc);
          T.txt(g, card.name, cxx + u, cyy + u * 1.9, u * 1.35, T.C.hot, card.kind === 'star' || card.kind === 'sat' ? 't' : 'c');
          const kind = { star: 'STAR', planet: 'PLANET', moon: 'MOON', sun: 'SUN', sat: 'SATELLITE' }[card.kind];
          T.txt(g, kind + (card.mag != null ? '  ·  MAG ' + card.mag.toFixed(1) : '') + (card.phase != null ? '  ·  ' + Math.round(card.phase * 100) + '%' : ''), cxx + u, cyy + u * 3.1, u * 0.75, T.C.label);
          T.txt(g, `ALT ${a.alt.toFixed(1)}°   AZ ${a.az.toFixed(1)}°`, cxx + u, cyy + u * 4.9, u * 1.05, T.C.acc);
          T.hits.rect(cxx, cyy, cw, ch, { sky: null });
        }
      },
    };
  },

  // ======================================================================
  //  GLOBE — Земля с днём и ночью, спутники на орбитах, пролёт над городом
  // ======================================================================
  globe(g, w, h, L, dt, tt) {
    const { u, m, top, bot } = L, A = T.astro, now = A.now(), P = A.place(), sats = A.sats(), gl = this.gl, D2R = Math.PI / 180;
    const portrait = h > w * 1.05;
    const R = portrait ? Math.min(w / 2 - m - u * 2, (bot - top) * 0.3) : Math.min((bot - top) / 2 - u * 2, w * 0.28);
    const cx = portrait ? w / 2 : m + R + u * 5, cy = portrait ? top + R + u * 3 : (top + bot) / 2 + u * 0.3;
    // спутники сейчас
    const pos = sats.map(s => A.satPos(s, now));
    // куда смотрим
    if (gl.follow >= 0 && gl.follow >= sats.length) gl.follow = -1;
    let tLon = null, tLat = null;
    if (gl.follow === -2) { tLon = P.lon; tLat = P.lat; } else if (gl.follow >= 0) { tLon = pos[gl.follow].lon; tLat = pos[gl.follow].lat; }
    if (tLon != null) {
      let dl = ((tLon - gl.lon0) % 360 + 540) % 360 - 180;
      gl.lon0 += dl * T.ease(dt, 0.6); gl.lat0 += (T.clamp(tLat * 0.7 + 8, -55, 60) - gl.lat0) * T.ease(dt, 0.8);
    } else if (performance.now() - gl.last > 3000) { gl.lon0 += dt * 6; gl.lat0 += (25 - gl.lat0) * T.ease(dt, 3); }
    const l0 = gl.lon0 * D2R, b0 = gl.lat0 * D2R, cb = Math.cos(b0), sb = Math.sin(b0);
    // мир -> вид: [x, y, z]; z>0 — лицевая сторона
    const view = (lat, lon, r = 1) => {
      const la = lat * D2R, dl = lon * D2R - l0, a = Math.cos(la) * Math.cos(dl), b = Math.cos(la) * Math.sin(dl), c = Math.sin(la);
      return [b * r, (cb * c - sb * a) * r, (sb * c + cb * a) * r];
    };
    const scr = v => [cx + R * v[0], cy - R * v[1]];
    // фон: звёзды
    if (!this.bgStars) { this.bgStars = []; for (let i = 0; i < 260; i++) this.bgStars.push([Math.random(), Math.random(), Math.random()]); }
    g.fillStyle = 'rgba(150,175,192,.5)'; g.beginPath();
    for (const s of this.bgStars) { const r = 0.6 + s[2] * 1.2; g.rect(s[0] * w, s[1] * h, r, r); }
    g.fill();
    // атмосфера
    const at = g.createRadialGradient(cx, cy, R * 0.96, cx, cy, R * 1.14); at.addColorStop(0, 'rgba(62,230,255,.45)'); at.addColorStop(0.3, 'rgba(62,230,255,.12)'); at.addColorStop(1, 'rgba(62,230,255,0)');
    g.fillStyle = at; g.beginPath(); g.arc(cx, cy, R * 1.14, 0, T.TAU); g.fill();
    // день и ночь: свет считается для каждой точки диска (в малом разрешении)
    const jd = A.jd(now), sun = A.sun(jd), sLon = ((sun.ra - A.gmst(jd)) % 360 + 540) % 360 - 180, sv = view(sun.dec, sLon);
    const N = 160;
    if (!this.gImg) { this.gOff = Object.assign(document.createElement('canvas'), { width: N, height: N }); this.gImg = new ImageData(N, N); }
    const Dd = this.gImg.data;
    for (let j = 0; j < N; j++) {
      const ny = 1 - (j + 0.5) / N * 2;
      for (let i = 0; i < N; i++) {
        const nx = (i + 0.5) / N * 2 - 1, q = (j * N + i) * 4, rr = nx * nx + ny * ny;
        if (rr > 1) { Dd[q + 3] = 0; continue; }
        const nz = Math.sqrt(1 - rr), lit = nx * sv[0] + ny * sv[1] + nz * sv[2];
        const k = T.clamp((lit + 0.08) / 0.2), limb = 0.55 + 0.45 * nz;
        const tw = Math.max(0, 1 - Math.abs(lit) / 0.025) * 0.55;                    // полоса терминатора
        if (T.theme === 'sumi') {                          // тушь: день — бумага, ночь — размывка тушью, терминатор — мазок
          const v = 1 - (0.55 * (1 - k) + 0.12 * (1 - limb)) - 0.5 * tw;
          Dd[q] = 27 + 209 * v; Dd[q + 1] = 24 + 204 * v; Dd[q + 2] = 20 + 192 * v; Dd[q + 3] = 255;
        } else { Dd[q] = (6 + 14 * k) * limb + 40 * tw; Dd[q + 1] = (12 + 46 * k) * limb + 120 * tw; Dd[q + 2] = (18 + 58 * k) * limb + 140 * tw; Dd[q + 3] = 255; }
      }
    }
    this.gOff.getContext('2d').putImageData(this.gImg, 0, 0);
    g.imageSmoothingEnabled = true; g.drawImage(this.gOff, cx - R, cy - R, 2 * R, 2 * R);
    // сетка 30°
    g.lineWidth = 1; g.strokeStyle = 'rgba(62,230,255,.12)'; g.beginPath();
    const poly = pts => { let have = false; for (const v of pts) { if (v[2] > 0) { const p = scr(v); have ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); have = true; } else have = false; } };
    for (let la = -60; la <= 60; la += 30) { const a = []; for (let lo = -180; lo <= 180; lo += 5) a.push(view(la, lo)); poly(a); }
    for (let lo = -180; lo < 180; lo += 30) { const a = []; for (let la = -90; la <= 90; la += 5) a.push(view(la, lo)); poly(a); }
    g.stroke();
    // материки: днём ярко, ночью тускло
    const land = A.data().land, dayP = new Path2D(), nightP = new Path2D();
    for (const Lp of land) {
      let prev = null;
      for (const ll of Lp) {
        const v = view(ll[0], ll[1]);
        if (v[2] > 0 && prev && prev[2] > 0) {
          const a = scr(prev), b = scr(v), lit = (v[0] * sv[0] + v[1] * sv[1] + v[2] * sv[2]) > 0;
          const pp = lit ? dayP : nightP; pp.moveTo(a[0], a[1]); pp.lineTo(b[0], b[1]);
        }
        prev = v;
      }
    }
    g.lineWidth = 1.3; g.strokeStyle = 'rgba(62,230,255,.9)'; g.stroke(dayP); g.strokeStyle = 'rgba(62,230,255,.35)'; g.stroke(nightP);
    g.strokeStyle = 'rgba(62,230,255,.7)'; g.lineWidth = 1.5; g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.stroke();
    // дом
    const hv = view(P.lat, P.lon);
    if (hv[2] > 0) {
      const p = scr(hv), pulse = (tt % 2) / 2;
      T.diamond(g, p[0], p[1], u * 0.45, T.C.hot, true);
      g.strokeStyle = `rgba(235,250,255,${1 - pulse})`; g.lineWidth = 1; g.beginPath(); g.arc(p[0], p[1], u * (0.6 + pulse * 1.6), 0, T.TAU); g.stroke();
      T.txt(g, P.city.toUpperCase(), p[0] + u * 0.9, p[1] + u * 0.3, u * 0.75, T.C.hot);
      T.hits.circle(p[0], p[1], u * 1.6, { follow: -2 });
    }
    // спутники: наземный след ±45 мин и сам спутник на своей высоте
    if (!this.trk || now - this.trk.t > 20 || this.trk.n !== sats.length) {
      this.trk = { t: now, n: sats.length, tr: sats.map(s => { const a = []; for (let k = -45; k <= 45; k++) { const q = A.satPos(s, now + k * 60); a.push([q.lat, q.lon, k]); } return a; }) };
    }
    sats.forEach((s, i) => {
      const tr = this.trk.tr[i], sel = gl.follow === i;
      for (const half of [0, 1]) {
        g.beginPath(); let have = false;
        for (const [la, lo, k] of tr) {
          if ((k < 0) !== (half === 0) && k !== 0) continue;
          const v = view(la, lo);
          if (v[2] > 0) { const p = scr(v); have ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); have = true; } else have = false;
        }
        g.strokeStyle = half === 0 ? `rgba(62,230,255,${sel ? 0.35 : 0.22})` : `rgba(235,250,255,${sel ? 0.7 : 0.45})`;
        g.lineWidth = sel ? 1.8 : 1.2; if (half === 1) g.setLineDash([5, 5]); g.stroke(); g.setLineDash([]);
      }
      const q = pos[i], r = 1 + q.alt / A.RE * 2.2, sv2 = view(q.lat, q.lon, r), ground = view(q.lat, q.lon);
      const front = sv2[2] > 0 || sv2[0] * sv2[0] + sv2[1] * sv2[1] > 1;
      if (!front) return;
      const p = scr(sv2), gp = scr(ground), pulse = 0.5 + 0.5 * Math.sin(tt * 5 + i);
      if (ground[2] > 0) { g.strokeStyle = 'rgba(235,250,255,.4)'; g.lineWidth = 1; g.setLineDash([2, 3]); g.beginPath(); g.moveTo(gp[0], gp[1]); g.lineTo(p[0], p[1]); g.stroke(); g.setLineDash([]); }
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(p[0], p[1], u * (0.35 + 0.15 * pulse), 0, T.TAU); g.fill();
      g.strokeStyle = `rgba(62,230,255,${0.9 - pulse * 0.6})`; g.lineWidth = 1.2; g.beginPath(); g.arc(p[0], p[1], u * (0.8 + pulse), 0, T.TAU); g.stroke();
      T.txt(g, s.name, p[0] + u * 1.2, p[1] - u * 0.5, u * 0.85, sel ? T.C.hot : T.C.acc, 't');
      T.txt(g, Math.round(q.alt) + ' km', p[0] + u * 1.2, p[1] + u * 0.6, u * 0.68, T.C.label);
      T.hits.circle(p[0], p[1], u * 1.8, { follow: i });
    });
    // ближайший пролёт над городом: раз в минуту
    const fi = gl.follow >= 0 ? gl.follow : 0, fs = sats[fi];
    if (fs && (!this.pass || this.pass.name !== fs.name || now - this.pass.t > 60 || this.pass.city !== P.city)) {
      this.pass = { name: fs.name, t: now, city: P.city, p: A.nextPass(fs, now, P.lat, P.lon, 10, 24) };
    }
    const followName = gl.follow === -2 ? 'HOME' : gl.follow >= 0 ? sats[gl.follow].name : 'AUTO';
    return {
      status: `${P.city} · ${sats.length ? sats.length + ' orbits' : 'no orbits'} · view ${followName}`,
      hint: 'drag: rotate  ·  tap satellite / home: follow', right: 'kepler + j2 · ' + new Date().toLocaleTimeString('ru-RU'),
      draw: () => {
        const pw = portrait ? w - 2 * m : Math.min(u * 22, w - cx - R - m - u * 6), px = portrait ? m : w - m - pw;
        const py = portrait ? cy + R + u * 3.2 : top + u * 0.4, ph = portrait ? Math.min(u * 24, Math.max(u * 9, bot - py - u * 3.4)) : Math.min(bot - top - u * 3.6, u * 22);
        if (pw < u * 10) return;
        T.panel(g, px, py, pw, ph, u);
        let y = py + u * 2;
        if (!fs) {
          T.txt(g, 'NO ORBITS YET', px + u, y + u, u * 1.4, T.C.hot, 't');
          T.txt(g, 'LAPTOP SENDS TLE TO THE STICK', px + u, y + u * 2.6, u * 0.75, T.C.label);
          T.txt(g, 'ONCE — THEN IT WORKS OFFLINE', px + u, y + u * 3.8, u * 0.75, T.C.label);
          return;
        }
        const q = pos[fi];
        T.txt(g, fs.name, px + u, y + u * 0.8, u * 2, T.C.hot, 't'); y += u * 2.4;
        const row = (k, v, col = T.C.hot) => { if (y > py + ph - u * 0.6) return; T.txt(g, k, px + u, y, u * 0.8, T.C.dim); T.txt(g, v, px + pw - u, y, u * 0.9, col, 'm', 'right'); y += u * 1.5; };
        row('LAT / LON', `${Math.abs(q.lat).toFixed(1)}°${q.lat >= 0 ? 'N' : 'S'}  ${Math.abs(q.lon).toFixed(1)}°${q.lon >= 0 ? 'E' : 'W'}`);
        row('ALTITUDE', Math.round(q.alt) + ' km');
        row('SPEED', q.speed.toFixed(2) + ' km/s');
        const age = (now - fs.epoch) / 86400;
        row('ORBIT AGE', age.toFixed(1) + ' d', age > 7 ? T.C.alert : T.C.hot);
        const look = A.satLook(q, P.lat, P.lon);
        row('FROM ' + P.city.toUpperCase().slice(0, 10), look.el > 0 ? 'UP ' + Math.round(look.el) + '°' : 'below horizon', look.el > 0 ? T.C.acc : T.C.label);
        y += u * 0.6;
        if (y < py + ph - u * 4) {
          T.txt(g, 'NEXT PASS · ABOVE 10°', px + u, y, u * 0.75, T.C.acc); y += u * 2.4;
          const ps = this.pass && this.pass.p;
          if (ps && ps.found) {
            const tm = s => new Date(s * 1000).toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
            const inMin = Math.max(0, Math.round((ps.rise - now) / 60));
            T.txt(g, tm(ps.rise), px + u, y, u * 2.4, inMin === 0 ? T.C.acc : T.C.hot, 't');
            T.txt(g, inMin === 0 ? 'NOW' : 'IN ' + (inMin >= 60 ? Math.floor(inMin / 60) + ' h ' + inMin % 60 + ' min' : inMin + ' min'), px + pw - u, y - u * 0.2, u * 0.85, T.C.label, 'm', 'right');
            y += u * 1.6;
            const dirs = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
            row('MAX ELEVATION', Math.round(ps.maxEl) + '°');
            row('RISES IN', dirs[Math.round(ps.azRise / 45) % 8] + ' · ' + Math.round((ps.set - ps.rise) / 60) + ' min');
          } else T.txt(g, 'NONE IN 24 H', px + u, y, u * 1.1, T.C.label, 't');
        }
      },
    };
  },
});
