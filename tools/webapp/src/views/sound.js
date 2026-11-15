// ---- SOUND: спектр, 3D-горы, шумомер — по живому звуку микрофона палки (~25 кадров/с) ----
T.views.push({
  id: 'snd', title: 'Sound', want: 'snd', subs: ['Spectrum', 'Terrain', 'Level'], sub: 1, full: true,
  mount(main) {
    const v = this.root = T.el('div', 'view full');
    const st = T.el('div', 'stage'); v.appendChild(st); this.cv = T.canvas(st, 1.5);   // горы — много заливки: 1.5x хватает глазу и вдвое легче
    this.msg = T.el('div', 'empty hide'); st.appendChild(this.msg);
    main.appendChild(v);
    this.ROWS = 40; this.COLS = 72; this.rows = []; this.rowT = 0; this.top = 0.3;
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  frame(dt, t) {
    const { g, w, h } = this.cv; this.cv.clear();
    const d = S.snd, fresh = d && performance.now() - S.sndT < 1500;
    const bad = !d ? 'waiting for sound…' : !d.en ? 'microphone is off on the device' : !d.ok ? 'starting microphone…' : !fresh ? 'no sound data' : '';
    this.msg.classList.toggle('hide', !bad);
    if (bad) { this.msg.innerHTML = `<b>${bad.toUpperCase()}</b>${d && !d.en ? '<span>Settings › Sound › Microphone: on</span>' : ''}`; return; }
    if (this.sub === 0) this.spectrum(g, w, h); else if (this.sub === 1) this.terrain(g, w, h, dt); else this.level(g, w, h);
    T.corners(g, 8, 8, w - 16, h - 16);
    const hz = S.hz >= 1000 ? (S.hz / 1000).toFixed(1) + ' kHz' : S.hz + ' Hz';
    T.text(g, ['// SPECTRUM', '// TERRAIN', '// NOISE METER'][this.sub], 18, 34, 15, T.C.acc, 'left', 'alphabetic', true);
    T.text(g, `${Math.round(S.level)} dBFS   ${S.energy > 0.15 ? hz : ''}`, w - 22, 34, 15, S.level > -25 ? T.C.alert : T.C.hot, 'right', 'alphabetic', true);
  },
  spectrum(g, w, h) {
    const L = 40, Rr = 40, top = 70, base = h * 0.72, n = 32, bw = (w - L - Rr) / n, seg = 9;
    for (let i = 0; i < n; i++) {
      const v = S.bands[i], x = L + i * bw, bh = v * (base - top), segs = Math.floor(bh / seg);
      for (let s = 0; s < segs; s++) {
        const y = base - (s + 1) * seg; g.fillStyle = T.heat(0.2 + 0.8 * (s * seg) / (base - top));
        g.fillRect(x + 2, y + 1, bw - 4, seg - 2);
        g.fillStyle = T.heat(0.2 + 0.8 * (s * seg) / (base - top), 0.12); g.fillRect(x + 2, base + (base - y) * 0.45 - seg + 2, bw - 4, seg - 3);   // отражение
      }
      const cy = base - S.caps[i] * (base - top) - 4; g.fillStyle = T.C.hot; g.fillRect(x + 2, cy, bw - 4, 3);
    }
    g.strokeStyle = T.C.line; g.beginPath(); g.moveTo(L, base + 1); g.lineTo(w - Rr, base + 1); g.stroke();
    [[100, 4], [500, 14], [1000, 19], [4000, 28]].forEach(([f, i]) => T.text(g, f >= 1000 ? f / 1000 + 'k' : f, L + (i + 0.5) * bw, h - 24, 13, T.C.steel, 'center'));
  },
  terrain(g, w, h, dt) {
    // новый хребет 40 раз в секунду из сглаженных полос; басы слева, высокие справа
    this.rowT += dt;
    while (this.rowT > 0.025) {
      this.rowT -= 0.025;
      const raw = new Float32Array(this.COLS);
      for (let c = 0; c < this.COLS; c++) { const k = c * 31 / (this.COLS - 1), k0 = Math.floor(k), f = k - k0; const b = S.bands[k0] * (1 - f) + S.bands[Math.min(31, k0 + 1)] * f; raw[c] = b * b; }
      for (let pass = 0; pass < 2; pass++) for (let c = 1; c < this.COLS - 1; c++) raw[c] = (raw[c - 1] + 2 * raw[c] + raw[c + 1]) / 4;
      const mx = Math.max(...raw); this.top = Math.max(mx, this.top * 0.992, 0.12);
      const prev = this.rows[0], row = new Float32Array(this.COLS), loud = T.clamp(S.energy * 1.6, 0, 1);
      for (let c = 0; c < this.COLS; c++) {
        const v = 1 - Math.exp(-2.2 * raw[c] / this.top), edge = Math.sin(Math.PI * (c + 0.5) / this.COLS);
        let hh = v * (0.35 + 0.65 * loud) * (0.45 + 0.55 * edge) + 0.04 + 0.05 * ((T.hash(this.rows.length * 977 + c) & 255) / 255) * (0.3 + v);
        row[c] = prev ? prev[c] * 0.35 + hh * 0.65 : hh;
      }
      let rm = 0; for (let c = 0; c < this.COLS; c++) rm = Math.max(rm, row[c]); row.max = rm;   // для цвета гребня
      this.rows.unshift(row); if (this.rows.length > this.ROWS) this.rows.pop();
    }
    const hy = h * 0.3, camH = 1.25, cx = w / 2;
    const F = Math.max(Math.min(w, h * 1.6) * 0.62, (h * 0.97 - hy) * 1.5 / camH);   // в портрете горы — до низа экрана
    const sunR = 26 + S.energy * 30;
    const sg = g.createRadialGradient(w * 0.68, hy, 0, w * 0.68, hy, sunR * 2.2);
    sg.addColorStop(0, T.rgba(T.C.acc, 0.55)); sg.addColorStop(0.45, T.rgba(T.C.acc, 0.18)); sg.addColorStop(1, T.rgba(T.C.acc, 0));
    g.fillStyle = sg; g.beginPath(); g.arc(w * 0.68, hy, sunR * 2.2, 0, Math.PI * 2); g.fill();
    g.strokeStyle = T.C.line; g.beginPath(); g.moveTo(0, hy); g.lineTo(w, hy); g.stroke();
    for (let r = this.rows.length - 1; r >= 0; r--) {
      const row = this.rows[r], z = 1.5 + r * 0.12, fade = 1 - r / this.ROWS * 0.8;
      const pts = [];
      for (let c = 0; c < this.COLS; c++) { const x = (c / (this.COLS - 1) * 2 - 1) * 3.2; pts.push([cx + x * F / z, hy + (camH - row[c] * 1.15) * F / z]); }
      const by = hy + camH * F / z;
      // один путь на хребет: заливка склона (сплошной тёмный тон — без градиента, быстрее) + гребень
      g.beginPath(); g.moveTo(pts[0][0], by); for (const p of pts) g.lineTo(p[0], p[1]); g.lineTo(pts[pts.length - 1][0], by); g.closePath();
      const sh = T.heatRGB(0.4); g.fillStyle = `rgba(${Math.round(sh[0] * 0.16 * fade + 2)},${Math.round(sh[1] * 0.16 * fade + 9)},${Math.round(sh[2] * 0.16 * fade + 11)},1)`; g.fill();
      g.beginPath(); for (let i = 0; i < pts.length; i++) i ? g.lineTo(pts[i][0], pts[i][1]) : g.moveTo(pts[i][0], pts[i][1]);
      g.strokeStyle = T.heat(0.3 + 0.7 * (row.max || 0), fade); g.lineWidth = r < 3 ? 2 : 1.2; g.stroke();
    }
  },
  level(g, w, h) {
    const cx = w / 2, cy = h * 0.55, R = Math.min(w * 0.32, h * 0.38), v = T.clamp((S.level + 90) / 90, 0, 1), pk = T.clamp((S.peak + 90) / 90, 0, 1);
    const a0 = Math.PI * 0.8, a1 = Math.PI * 2.2;
    g.lineWidth = 18; g.strokeStyle = T.C.line; g.beginPath(); g.arc(cx, cy, R, a0, a1); g.stroke();
    g.strokeStyle = S.level > -25 ? T.C.alert : T.C.acc; g.beginPath(); g.arc(cx, cy, R, a0, a0 + (a1 - a0) * v); g.stroke();
    g.lineWidth = 3; g.strokeStyle = T.C.hot; const pa = a0 + (a1 - a0) * pk; g.beginPath(); g.arc(cx, cy, R + 14, pa - 0.02, pa + 0.02); g.stroke();
    for (let k = 0; k <= 9; k++) { const a = a0 + (a1 - a0) * k / 9; g.lineWidth = 1; g.strokeStyle = T.C.dim; g.beginPath(); g.moveTo(cx + Math.cos(a) * (R - 22), cy + Math.sin(a) * (R - 22)); g.lineTo(cx + Math.cos(a) * (R - 30), cy + Math.sin(a) * (R - 30)); g.stroke(); T.text(g, -90 + k * 10, cx + Math.cos(a) * (R - 46), cy + Math.sin(a) * (R - 46) + 4, 11, T.C.dim, 'center'); }
    T.text(g, Math.round(S.level), cx, cy + 10, Math.round(R * 0.5), T.C.hot, 'center', 'alphabetic', true);
    const word = S.level < -55 ? 'QUIET' : S.level < -40 ? 'NORMAL' : S.level < -25 ? 'LOUD' : 'VERY LOUD';
    T.text(g, 'dBFS · ' + word, cx, cy + 44, 18, S.level > -25 ? T.C.alert : T.C.steel, 'center');
    // график громкости за 2 минуты
    const H0 = h - 70, gh = 50, a = S.lvHist; if (a.length > 2) {
      g.beginPath(); a.forEach((lv, i) => { const x = 30 + i / (25 * 120) * (w - 60), y = H0 + gh - T.clamp((lv + 90) / 90, 0, 1) * gh; i ? g.lineTo(x, y) : g.moveTo(x, y); });
      g.strokeStyle = T.C.acc; g.lineWidth = 1.5; g.stroke(); T.text(g, '2 MIN', 30, H0 - 6, 11, T.C.dim);
    }
  },
});
