// ---- GALLERY: чистая математика, считает только планшет (палка не нужна) ----
T.views.push({
  id: 'gal', title: 'Gallery', want: '', subs: ['Tesseract', 'Torus', 'Life', 'Pendulum'], sub: 0, full: true,
  mount(main) {
    const v = this.root = T.el('div', 'view full');
    const st = T.el('div', 'stage'); v.appendChild(st); this.cv = T.canvas(st);
    const c = T.el('div', 'ctrl', `<button class="btn" data-a="-">−</button><button class="btn" data-a="+">+</button><button class="btn" data-a="go" id="g-act">NEW</button>`);
    st.appendChild(c);
    c.addEventListener('click', e => { const a = e.target.dataset.a; if (a === '-') this.speed = Math.max(0, this.speed - 0.25); if (a === '+') this.speed = Math.min(4, this.speed + 0.25); if (a === 'go') this.action(); });
    main.appendChild(v);
    this.speed = 1; this.ph = 0; this.trail = []; this.pd = null; this.life = null;
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  onSub() { this.trail = []; this.pd = null; },
  action() { if (this.sub === 2) this.life = null; if (this.sub === 3) this.pd = null; },
  frame(dt) {
    const { g, w, h } = this.cv;
    this.ph += dt * this.speed;
    document.getElementById('g-act').style.visibility = this.sub >= 2 ? 'visible' : 'hidden';
    if (this.sub === 0) this.tess(g, w, h); else if (this.sub === 1) this.torus(g, w, h); else if (this.sub === 2) this.lifeF(g, w, h, dt); else this.pend(g, w, h, dt);
    T.corners(g, 8, 8, w - 16, h - 16);
    T.text(g, '// ' + this.subs[this.sub].toUpperCase(), 18, 34, 15, T.C.acc, 'left', 'alphabetic', true);
    T.text(g, 'SPEED ×' + this.speed.toFixed(2), w - 22, 34, 14, T.C.steel, 'right');
  },
  // 4D-гиперкуб: вращение в XW, YW, ZW, XY; перспектива 4D->3D->экран; след прошлых кадров
  tess(g, w, h) {
    this.cv.clear();
    const t = this.ph, A = [t * 0.9, t * 0.55, t * 0.35, t * 0.25], cx = w / 2, cy = h / 2 + 10, K = Math.min(w, h) * 0.42;
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
      P.push([cx + x * k3, cy + y * k3, ww]);
    }
    this.trail.unshift(P); if (this.trail.length > 10) this.trail.pop();
    for (let f = this.trail.length - 1; f >= 0; f--) {
      const Q = this.trail[f];
      for (let i = 0; i < 16; i++) for (let b = 0; b < 4; b++) {
        const j = i ^ (1 << b); if (j < i) continue;
        const wv = (Q[i][2] + Q[j][2]) / 2;
        if (f === 0) {
          g.strokeStyle = T.heat(0.35 + wv * 0.35, 0.25); g.lineWidth = 7; g.beginPath(); g.moveTo(Q[i][0], Q[i][1]); g.lineTo(Q[j][0], Q[j][1]); g.stroke();
          g.strokeStyle = T.heat(0.35 + wv * 0.35); g.lineWidth = 2.2;
        } else { g.strokeStyle = T.rgba(T.C.acc, 0.22 * (1 - f / 10)); g.lineWidth = 1; }
        g.beginPath(); g.moveTo(Q[i][0], Q[i][1]); g.lineTo(Q[j][0], Q[j][1]); g.stroke();
      }
    }
    for (const p of P) { g.fillStyle = p[2] > 0 ? T.C.hot : T.C.acc; g.beginPath(); g.arc(p[0], p[1], p[2] > 0.3 ? 6 : 4, 0, Math.PI * 2); g.fill(); }
  },
  // тор из точек: освещение и глубина (как donut.c), 3600 точек
  torus(g, w, h) {
    this.cv.clear();
    const Aa = this.ph * 0.8, Bb = this.ph * 0.45, cA = Math.cos(Aa), sA = Math.sin(Aa), cB = Math.cos(Bb), sB = Math.sin(Bb);
    const R1 = 1, R2 = 2.1, K2 = 6.5, K1 = Math.min(w, h) * 0.95, cx = w / 2, cy = h / 2 + 10, U = 120, V = 30;
    const pts = this.tp || (this.tp = new Float32Array(U * V * 4));
    let n = 0;
    for (let i = 0; i < V; i++) {
      const th = i * 2 * Math.PI / V, ct = Math.cos(th), st = Math.sin(th);
      for (let j = 0; j < U; j++) {
        const ph = j * 2 * Math.PI / U + i * 0.06, cp = Math.cos(ph), sp = Math.sin(ph), ox = R2 + R1 * ct, oy = R1 * st;
        const x = ox * (cB * cp + sA * sB * sp) - oy * cA * sB, y = ox * (sB * cp - sA * cB * sp) + oy * cA * cB, z = K2 + cA * ox * sp + oy * sA;
        const nx = ct * (cB * cp + sA * sB * sp) - st * cA * sB, ny = ct * (sB * cp - sA * cB * sp) + st * cA * cB, nz = cA * ct * sp + st * sA;
        pts[n++] = cx + K1 * x / z; pts[n++] = cy - K1 * y / z; pts[n++] = z; pts[n++] = ny * 0.7071 - nz * 0.7071;
      }
    }
    const N = U * V, idx = this.ti || (this.ti = new Uint16Array(N));
    for (let i = 0; i < N; i++) idx[i] = i;
    idx.sort((a, b) => pts[b * 4 + 2] - pts[a * 4 + 2]);
    for (let k = 0; k < N; k++) {
      const q = idx[k] * 4, depth = T.clamp((9.5 - pts[q + 2]) / 7, 0.15, 1), L = Math.max(0, pts[q + 3]);
      const v = T.clamp(0.08 + 0.92 * L * depth + 0.12 * depth, 0, 1), r = 0.8 + v * 2.2;
      g.fillStyle = T.heat(v); g.fillRect(pts[q] - r / 2, pts[q + 1] - r / 2, r, r);
    }
  },
  // «Жизнь» Конвея на весь экран: поле в пикселях-клетках, послесвечение умерших
  lifeF(g, w, h, dt) {
    const cell = Math.max(5, Math.round(Math.min(w, h) / 110));
    const W = Math.floor((w - 20) / cell), H = Math.floor((h - 60) / cell);
    if (W < 8 || H < 8) return;                           // экран ещё не получил размер
    let L = this.life;
    if (!L || L.W !== W || L.H !== H) {
      L = this.life = { W, H, a: new Uint8Array(W * H), b: new Uint8Array(W * H), glow: new Float32Array(W * H), t: 0, gen: 0, still: 0, prev: -1,
        img: new ImageData(W, H), off: Object.assign(document.createElement('canvas'), { width: W, height: H }) };
      for (let i = 0; i < W * H; i++) L.a[i] = Math.random() < 0.28 ? 1 : 0;
    }
    L.t += dt * this.speed * 12;
    while (L.t >= 1) {
      L.t -= 1; L.gen++; let pop = 0;
      for (let y = 0; y < H; y++) {
        const ym = ((y + H - 1) % H) * W, yy = y * W, yp = ((y + 1) % H) * W;
        for (let x = 0; x < W; x++) {
          const xm = (x + W - 1) % W, xp = (x + 1) % W;
          const nb = (L.a[ym + xm] > 0) + (L.a[ym + x] > 0) + (L.a[ym + xp] > 0) + (L.a[yy + xm] > 0) + (L.a[yy + xp] > 0) + (L.a[yp + xm] > 0) + (L.a[yp + x] > 0) + (L.a[yp + xp] > 0);
          const cur = L.a[yy + x];
          const nv = cur ? (nb === 2 || nb === 3 ? Math.min(cur + 1, 255) : 0) : (nb === 3 ? 1 : 0);
          if (cur && !nv) L.glow[yy + x] = 1;
          L.b[yy + x] = nv; if (nv) pop++;
        }
      }
      [L.a, L.b] = [L.b, L.a];
      L.still = Math.abs(pop - L.prev) <= 2 ? L.still + 1 : 0; L.prev = pop;
      if (pop < 20) { this.life = null; return; }
      if (L.still > 60) {                                // застой — подбросить «кляксы»
        for (let k = 0; k < 5; k++) { const cx0 = Math.random() * W | 0, cy0 = Math.random() * H | 0; for (let q = 0; q < 40; q++) L.a[((cy0 + (Math.random() * 7 | 0)) % H) * W + (cx0 + (Math.random() * 7 | 0)) % W] = 1; }
        L.still = 0;
      }
    }
    const D = L.img.data;
    for (let i = 0; i < W * H; i++) {
      const a = L.a[i]; let r = 2, gg = 9, bb = 11;
      if (a) { const c = a === 1 ? [234, 252, 255] : T.heatRGB(Math.max(0.3, 0.9 - a * 0.03)); r = c[0]; gg = c[1]; bb = c[2]; }
      else if (L.glow[i] > 0.01) { const k = L.glow[i] * 0.45; r = 2 + 63 * k; gg = 9 + 208 * k; bb = 11 + 244 * k; L.glow[i] *= Math.pow(0.02, dt); }
      D[i * 4] = r; D[i * 4 + 1] = gg; D[i * 4 + 2] = bb; D[i * 4 + 3] = 255;
    }
    L.off.getContext('2d').putImageData(L.img, 0, 0);
    this.cv.clear();
    g.imageSmoothingEnabled = false;
    g.drawImage(L.off, (w - W * cell) / 2, 46, W * cell, H * cell);
    T.text(g, `GEN ${L.gen}   CELLS ${L.prev}`, w / 2, 34, 14, T.C.steel, 'center');
  },
  // двойной маятник: Лагранж + Рунге-Кутта 4, длинный светящийся след
  pend(g, w, h, dt) {
    if (!this.pd) { this.pd = { y: [2 + Math.random(), 1.5 + Math.random() * 1.6, 0, 0] }; this.trail = []; }
    const f = y => { const gr = 9.81, [t1, t2, w1, w2] = y, d = t1 - t2, den = 3 - Math.cos(2 * d);
      return [w1, w2, (-gr * 3 * Math.sin(t1) - gr * Math.sin(t1 - 2 * t2) - 2 * Math.sin(d) * (w2 * w2 + w1 * w1 * Math.cos(d))) / den,
        (2 * Math.sin(d) * (w1 * w1 * 2 + gr * 2 * Math.cos(t1) + w2 * w2 * Math.cos(d))) / den]; };
    const R = Math.min(w, h) * 0.2, cx = w / 2, cy = h / 2 - R * 0.2, steps = 8, hs = Math.min(dt, 0.05) * this.speed / steps;
    for (let s = 0; s < steps; s++) {
      const y = this.pd.y, k1 = f(y), k2 = f(y.map((v, i) => v + k1[i] * hs / 2)), k3 = f(y.map((v, i) => v + k2[i] * hs / 2)), k4 = f(y.map((v, i) => v + k3[i] * hs));
      this.pd.y = y.map((v, i) => v + hs / 6 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]));
      const [t1, t2, w1, w2] = this.pd.y, x1 = cx + R * Math.sin(t1), y1 = cy + R * Math.cos(t1);
      this.trail.push([x1 + R * Math.sin(t2), y1 + R * Math.cos(t2), Math.min(1, (Math.abs(w2) + Math.abs(w1) * 0.5) / 12)]);
    }
    if (this.trail.length > 2400) this.trail.splice(0, this.trail.length - 2400);
    this.cv.clear();
    const n = this.trail.length;
    g.lineCap = 'round';
    for (let i = 1; i < n; i++) {
      const a = this.trail[i - 1], b = this.trail[i], age = i / n;
      g.strokeStyle = T.heat(0.35 + 0.65 * b[2], 0.06 + 0.94 * age * age); g.lineWidth = 1 + 2 * age;
      g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.stroke();
    }
    const [t1, t2] = this.pd.y, x1 = cx + R * Math.sin(t1), y1 = cy + R * Math.cos(t1), x2 = x1 + R * Math.sin(t2), y2 = y1 + R * Math.cos(t2);
    g.strokeStyle = T.C.line; g.lineWidth = 1; g.beginPath(); g.arc(cx, cy, 2 * R, 0, Math.PI * 2); g.stroke();
    g.strokeStyle = T.C.steel; g.lineWidth = 2.5; g.beginPath(); g.moveTo(cx, cy); g.lineTo(x1, y1); g.lineTo(x2, y2); g.stroke();
    g.fillStyle = T.C.dim; g.beginPath(); g.arc(cx, cy, 4, 0, 7); g.fill();
    g.fillStyle = T.C.acc; g.beginPath(); g.arc(x1, y1, 9, 0, 7); g.fill();
    g.fillStyle = T.C.hot; g.beginPath(); g.arc(x2, y2, 11, 0, 7); g.fill();
  },
});
