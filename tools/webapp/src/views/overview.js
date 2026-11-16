// ---- ОБЗОР (версия B): радар в глубину, крупные числа, каналы Wi-Fi, состояние палки, лента событий ----
T.views.push({
  id: 'ov', title: 'Overview', want: 'ble,wifi',
  mount(main) {
    this.root = T.el('div', 'view'); const st = T.el('div', 'stage'); this.root.appendChild(st); this.cv = T.canvas(st); main.appendChild(this.root);
    this.cam = T.orbit(0.4, 1.02, 3.3);
    this.sw = null; this.dust = T.dust(120);
    this.nDev = null; this.nNet = null; this.near = null; this.chH = new Float32Array(14);
    this.cv.cv.addEventListener('pointerup', e => { const h = T.hits.at(e.offsetX, e.offsetY); if (h && h.go) T.app.go(h.go); });
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  frame(dt, t) {
    const BU = T.bleUI, { g, w, h } = this.cv; T.hits.clear();
    if (!this.sw) { this.sw = BU.sweeper(3.6); this.nDev = BU.hold(); this.nNet = BU.hold(); this.near = T.steady(3, 0.8); }
    const u = T.unit(w, h), m = u * 1.6, y0 = 12, portrait = h > w * 1.05, now = performance.now();
    this.sw.tick(dt, t); this.dust.tick(dt);
    T.backdrop(g, w, h, portrait ? 0.5 : 0.3, portrait ? 0.3 : 0.55);
    const devs = BU.list(), aps = [...S.aps.values()].filter(a => a.alive);
    // радар слева (или сверху в портрете)
    const top = y0 + u * 4;
    const rr = portrait ? { x: m, y: top, w: w - 2 * m, h: (h - top) * 0.42 } : { x: m, y: top, w: w * 0.44, h: h - top - u * 3 };
    this.cam.yaw += dt * 0.06;
    this.cam.frame(rr.x + rr.w / 2, rr.y + rr.h * 0.56, Math.min(rr.w * 0.95, rr.h * 1.55));
    BU.sceneAny(g, this.cam, this.sw, { u, t, sel: null, labels: 2, hits: false });
    this.dust.draw(g, this.cam);
    T.bloom(this.cv, 0.75);
    T.hits.rect(rr.x, rr.y, rr.w, rr.h, { go: 'ble' });
    T.header(g, m, y0, w - 2 * m, u, 'Overview', S.st ? `${S.st.name || 'TENGAN'} · up ${T.ago(S.st.up || 0)}` : 'waiting for tenGAN', !S.st);
    T.txt(g, '// BLE SCOPE  ›', rr.x, rr.y + u * 1, u * 0.95, T.C.acc, 't');
    // правая колонка
    const cx = portrait ? m : rr.x + rr.w + u * 2, cw = portrait ? w - 2 * m : w - m - cx;
    let cy = portrait ? rr.y + rr.h + u : top;
    // крупные числа
    const nd = this.nDev.set(S.ble ? S.ble.n : 0, now), nn = this.nNet.set(aps.length, now);
    const best = devs.length ? Math.max(...devs.map(e => e.rs)) : null; if (best != null) this.near.set(best); const nv = this.near.tick(dt);
    const tw = (cw - u * 2) / 3, th = u * 6;
    const tiles = [['DEVICES', S.ble ? String(nd) : '—', T.C.hot, 'ble'], ['NETWORKS', S.wifi ? String(nn) : '—', T.C.hot, 'wifi'], ['NEAREST', nv != null && devs.length ? String(Math.round(nv)) : '—', nv != null ? T.heat(0.4 + 0.6 * T.pct(nv)) : T.C.dim, 'ble']];
    tiles.forEach(([k, v, col, go], i) => {
      const x = cx + i * (tw + u);
      T.panel(g, x, cy, tw, th, u, `rgba(${T.RGB.panel},.28)`, T.C.line);
      T.txt(g, k, x + u, cy + u * 1.5, u * 0.8, T.C.dim);
      T.bigNum(g, v, x + u, cy + th - u * 1.1, u * 3.3, col, i === 2 && v !== '—' ? 'DBM' : '');
      T.hits.rect(x, cy, tw, th, { go });
    });
    cy += th + u;
    // каналы Wi-Fi: сильнейшая сеть на канале, полосы «жара»
    const chH = u * 7.2;
    T.panel(g, cx, cy, cw, chH, u, `rgba(${T.RGB.panel},.22)`, T.C.line);
    T.txt(g, '// WI-FI CHANNELS  ›', cx + u, cy + u * 1.5, u * 0.95, T.C.acc, 't');
    const best2 = new Float32Array(14).fill(-100), cnt = new Uint8Array(14);
    for (const a of aps) { const c = a.c | 0; if (c >= 1 && c <= 13) { best2[c] = Math.max(best2[c], a.rs); cnt[c]++; } }
    const bx = cx + u * 1.2, bw = (cw - u * 2.4) / 13, bb = cy + chH - u * 1.6, bh = chH - u * 3.8;
    for (let c = 1; c <= 13; c++) {
      this.chH[c] += (T.pct(best2[c]) - this.chH[c]) * T.ease(dt, 0.6);
      const v = this.chH[c], x = bx + (c - 1) * bw, segs = 8, lit = Math.round(v * segs), sh = bh / segs;
      for (let k = 0; k < segs; k++) { g.fillStyle = k < lit ? T.heat(0.3 + 0.7 * k / segs) : T.C.line; g.fillRect(x + bw * 0.18, bb - (k + 1) * sh + sh * 0.2, bw * 0.64, sh * 0.6); }
      T.txt(g, c, x + bw / 2, bb + u * 1.1, u * 0.7, cnt[c] ? T.C.label : T.C.dim, 'm', 'center');
    }
    T.hits.rect(cx, cy, cw, chH, { go: 'wifi' });
    cy += chH + u;
    // состояние палки + история числа устройств
    const sH = u * 6.6, sw2 = (cw - u) / 2;
    T.panel(g, cx, cy, sw2, sH, u, `rgba(${T.RGB.panel},.22)`, T.C.line);
    T.txt(g, '// TENGAN', cx + u, cy + u * 1.5, u * 0.95, T.C.acc, 't');
    const s = S.st || {};
    T.txt(g, 'BAT', cx + u, cy + u * 3, u * 0.75, T.C.dim);
    T.segBar(g, cx + u * 3.4, cy + u * 2.45, sw2 - u * 8, u * 0.55, s.bat >= 0 ? s.bat / 100 : 0, 10, false, s.bat >= 0 && s.bat <= 15);
    T.txt(g, s.bat >= 0 ? s.bat + '%' + (s.usb ? ' ⚡' : '') : '—', cx + sw2 - u, cy + u * 3, u * 0.85, T.C.hot, 'm', 'right');
    T.txt(g, 'CLIENTS ' + (s.cli ?? '—') + '   HEAP ' + (s.heap ?? '—') + 'K', cx + u, cy + u * 4.4, u * 0.75, T.C.label);
    const r = s.radio || '';
    let rx = cx + u;
    for (const [k, n] of [['b', 'BLE'], ['w', 'WIFI'], ['s', 'SUB'], ['m', 'MIC'], ['p', 'PC'], ['i', 'IR'], ['n', 'NFC']]) {
      const on = r.includes(k); T.diamond(g, rx + u * 0.3, cy + u * 5.55, u * 0.28, on ? T.C.acc : T.C.line, true);
      T.txt(g, n, rx + u * 0.8, cy + u * 5.8, u * 0.72, on ? T.C.hot : T.C.dim); rx += u * 1.2 + T.tw(g, n, u * 0.72);
    }
    const hx = cx + sw2 + u;
    T.panel(g, hx, cy, sw2, sH, u, `rgba(${T.RGB.panel},.22)`, T.C.line);
    T.txt(g, '// DEVICES · 3 MIN', hx + u, cy + u * 1.5, u * 0.95, T.C.acc, 't');
    const hi = Math.max(5, ...S.bleHist);
    T.spark(g, hx + u, cy + u * 2.3, sw2 - u * 2, sH - u * 3.2, S.bleHist, 0, hi * 1.15);
    cy += sH + u;
    // лента событий
    const eh = h - cy - u * 1.2;
    if (eh > u * 3) {
      T.panel(g, cx, cy, cw, eh, u, `rgba(${T.RGB.panel},.22)`, T.C.line);
      T.txt(g, '// EVENTS', cx + u, cy + u * 1.5, u * 0.95, T.C.acc, 't');
      let ey = cy + u * 2.9;
      const nowMs = Date.now();
      if (!S.events.length) T.txt(g, 'QUIET SO FAR', cx + u, ey + u * 0.3, u * 0.8, T.C.dim);
      for (let i = 0; i < S.events.length && ey < cy + eh - u * 0.5; i++) {
        const ev = S.events[i], d = new Date(ev.t), ts = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0') + ':' + String(d.getSeconds()).padStart(2, '0');
        const fresh = T.clamp(1 - (nowMs - ev.t) / 4000);
        g.globalAlpha = Math.max(0.4, 1 - i * 0.08);
        if (fresh > 0) { g.fillStyle = `rgba(${T.RGB.acc},${0.12 * fresh})`; g.fillRect(cx + u * 0.5, ey - u * 0.95, cw - u, u * 1.35); }
        T.txt(g, ts, cx + u, ey, u * 0.78, T.C.dim);
        T.txt(g, ev.tag, cx + u * 6.2, ey, u * 0.78, ev.alert ? T.C.alert : T.C.acc);
        g.font = `500 ${u * 0.78}px ${T.FC}`; T.txt(g, T.fit(g, String(ev.text).toUpperCase(), cw - u * 10.5), cx + u * 9.2, ey, u * 0.78, ev.alert ? T.C.alert : (i === 0 ? T.C.hot : T.C.label), 'c');
        g.globalAlpha = 1;
        ey += u * 1.45;
      }
    }
    T.finish(g, w, h);
  },
});
