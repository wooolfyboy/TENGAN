// ---- SUB-GHZ (CC1101, только приём): 3D-водопад и монитор всплесков ----
// Spectrum: время уходит вглубь рельефом, спереди — светящаяся линия спектра.
// Monitor: уровень на частоте полосы, всплески и их список.
(() => {
const BANDS = ['315', '433.92', '868', '915'];
const lvl = v => T.clamp((v + 120 - 10) / 70);          // как wfColor на палке
const ROWS = 36, BINS = 96;

T.views.push({
  id: 'sub', title: 'Sub-GHz', subs: ['Spectrum', 'Monitor'], sub: 0,
  want() { return `sub&sm=${this.sub}&sb=${this.band}`; },
  mount(main) {
    try { const b = localStorage.getItem('sub_band'); this.band = b != null ? +b : 1; } catch (e) { this.band = this.band ?? 1; }
    if (!(this.band >= 0 && this.band < 4)) this.band = 1;
    this.root = T.el('div', 'view');
    const st = T.el('div', 'stage'); this.root.appendChild(st);
    this.cv = T.canvas(st); main.appendChild(this.root);
    this.cam = T.orbit(0, 0.72, 3.3);
    this.drag = T.drag(this.cv.cv, this.cam, { minPitch: 0.35, maxPitch: 1.15 });
    this.drag.onTap = (x, y) => { const h = T.hits.at(x, y); if (h && h.k === 'band') this.setBand(h.i); };
    this.peak = new Float32Array(BINS).fill(-120);
    this.flash = 0; this.t = 0; this.chart = null;
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  reset() { S.subWf.length = 0; S.sub = null; this.peak.fill(-120); },
  onSub(i) { this.sub = i; this.reset(); },
  setBand(i) {
    if (i === this.band) return;
    this.band = i; try { localStorage.setItem('sub_band', i); } catch (e) {}
    this.reset(); T.app.rewant();
  },
  chips(g, w, u, m, y) {                                   // выбор полосы — справа под шапкой
    const cw = u * 5.2, chh = u * 1.9, gap = u * 0.4;
    let x = w - m - BANDS.length * cw - (BANDS.length - 1) * gap;
    T.txt(g, 'BAND MHZ', x - u * 0.8, y + chh * 0.66, u * 0.7, T.C.dim, 'm', 'right');
    BANDS.forEach((b, i) => {
      const on = i === this.band;
      T.cutPath(g, x, y, cw, chh, u * 0.5);
      g.fillStyle = on ? 'rgba(62,230,255,.14)' : 'rgba(5,8,13,.5)'; g.fill();
      g.strokeStyle = on ? T.C.acc : T.C.line; g.lineWidth = 1; g.stroke();
      T.txt(g, b, x + cw / 2, y + chh * 0.7, u * 1.0, on ? T.C.hot : T.C.label, 't', 'center');
      T.hits.rect(x, y - u * 0.3, cw, chh + u * 0.6, { k: 'band', i });
      x += cw + gap;
    });
  },
  frame(dt) {
    this.t += dt;
    const { g, w, h } = this.cv;
    T.hits.clear();
    T.backdrop(g, w, h, 0.5, 0.62);
    const u = T.unit(w, h), m = u * 1.6, y0 = 50;
    const d = S.sub;
    const title = (this.sub ? 'Monitor ' : 'Spectrum ') + BANDS[this.band];
    if (!d || d.mode !== this.sub) {
      T.header(g, m, y0, w - 2 * m, u, title, 'waiting');
      this.chips(g, w, u, m, y0 + u * 3);
      T.emptyState(g, w, h, u, 'WAITING', 'starting CC1101 receiver');
      T.finish(g, w, h); return;
    }
    if (!d.ok) {
      T.header(g, m, y0, w - 2 * m, u, 'Sub-GHz', 'radio not found', true);
      T.emptyState(g, w, h, u, 'NO RADIO', 'CC1101 not found', true);
      T.finish(g, w, h); return;
    }
    if (this.sub === 0) this.spectrum(g, w, h, u, m, y0, dt, d, title);
    else this.monitor(g, w, h, u, m, y0, dt, d, title);
    T.finish(g, w, h);
  },

  // ---------- SPECTRUM: 3D-водопад ----------
  spectrum(g, w, h, u, m, y0, dt, d, title) {
    const cam = this.cam, portrait = h > w * 1.05, cur = d.cur || [], n = cur.length || BINS;
    this.drag.tick(dt);
    if (this.drag.idle()) cam.yaw = T.lerp(cam.yaw, 0.18 * Math.sin(this.t * 0.07), T.ease(dt, 2));
    cam.yaw = T.clamp(cam.yaw, -0.8, 0.8);
    cam.frame(w / 2, portrait ? h * 0.42 : h * 0.5, portrait ? w * 1.05 : Math.min(w * 0.7, h * 1.2));
    const X = i => (i / (n - 1) - 0.5) * 3.0, Z = r => -1.0 + r * (2.4 / ROWS), HY = 0.75;
    // пиковое удержание (медленно опускается)
    let mi = 0;
    for (let i = 0; i < n; i++) { this.peak[i] = Math.max(cur[i] ?? -120, this.peak[i] - dt * 6); if (cur[i] > cur[mi]) mi = i; }
    const pk = cur[mi] ?? -120, burst = pk > -70;
    this.flash = Math.max(burst ? 1 : 0, this.flash - dt * 1.5);
    T.floorGrid(g, cam, 1.8, 0.2);
    // рельеф: дальние строки первыми
    const rows = S.subWf, R = Math.min(ROWS, rows.length);
    for (let r = R - 1; r >= 0; r--) {
      const row = rows[r], z = Z(r), fog = T.clamp(1 - r / ROWS * 0.85, 0.12, 1);
      const pts = new Array(n);
      for (let i = 0; i < n; i++) pts[i] = cam.p(X(i), HY * Math.pow(lvl(row[i] ?? -120), 1.2), z);
      const b0 = cam.p(X(0), 0, z), b1 = cam.p(X(n - 1), 0, z), top = cam.p(0, HY, z);
      g.beginPath(); g.moveTo(b0[0], b0[1]); for (const p of pts) g.lineTo(p[0], p[1]); g.lineTo(b1[0], b1[1]); g.closePath();
      g.fillStyle = 'rgba(5,9,14,.9)'; g.fill();
      let stroke;
      if (r < 8) {                                         // ближние строки — цвет по высоте, дальние — одним цветом (дешевле)
        stroke = g.createLinearGradient(0, b0[1], 0, top[1]);
        stroke.addColorStop(0, T.heat(0.12, 0.5 * fog)); stroke.addColorStop(0.3, T.heat(0.45, 0.85 * fog)); stroke.addColorStop(0.65, T.heat(0.8, fog)); stroke.addColorStop(1, T.heat(1, fog));
      } else stroke = T.heat(0.3 + 0.45 * fog, 0.35 + 0.6 * fog);
      g.beginPath(); pts.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]));
      g.strokeStyle = stroke; g.lineWidth = r === 0 ? 2.2 : 1.1; g.stroke();
    }
    // передняя линия + пики
    if (cur.length) {
      const z = Z(0) - 0.02;
      g.beginPath();
      for (let i = 0; i < n; i++) { const p = cam.p(X(i), HY * Math.pow(lvl(cur[i]), 1.2), z); i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); }
      g.strokeStyle = T.C.hot; g.lineWidth = 2; g.stroke();
      g.fillStyle = 'rgba(235,250,255,.75)';
      for (let i = 0; i < n; i += 2) { const p = cam.p(X(i), HY * Math.pow(lvl(this.peak[i]), 1.2), z); g.fillRect(p[0] - 1, p[1] - 1, 2, 2); }
    }
    // ось частот
    const zF = Z(0) - 0.15;
    for (let k = 0; k <= 8; k++) {
      const i = k / 8 * (n - 1), p = cam.p(X(i), 0, zF), f = d.lo + (d.hi - d.lo) * k / 8;
      g.fillStyle = k % 4 ? T.C.dim : T.C.acc; g.fillRect(p[0], p[1] - u * 0.3, 1, u * 0.6);
      if (k % 2 === 0) T.txt(g, f.toFixed(2), p[0], p[1] + u * 1.2, u * (k % 4 ? 0.65 : 0.8), k % 4 ? T.C.label : T.C.hot, 'm', 'center');
    }
    T.bloom(this.cv, 0.8 + 0.5 * this.flash);
    // выноска пиковой частоты
    const fpk = d.lo + (d.hi - d.lo) * mi / (n - 1);
    if (cur.length) {
      const p = cam.p(X(mi), HY * Math.pow(lvl(pk), 1.2), Z(0));
      const lx = T.clamp(p[0] + u * 3, u * 2, w - u * 15), ly = Math.max(y0 + u * 6, p[1] - u * 5);
      T.diamond(g, p[0], p[1], u * 0.6, burst ? T.C.alert : T.C.hot, false);
      g.strokeStyle = burst ? T.C.alert : 'rgba(235,250,255,.7)'; g.lineWidth = 1;
      g.beginPath(); g.moveTo(p[0] + u * 0.5, p[1] - u * 0.5); g.lineTo(lx, ly); g.lineTo(lx + u * 10, ly); g.stroke();
      T.txt(g, fpk.toFixed(3) + ' MHZ', lx, ly - u * 0.5, u * 1.25, burst ? T.C.alert : T.C.hot, 't');
      T.txt(g, pk + ' DBM' + (burst ? '  · BURST' : ''), lx, ly + u * 1.1, u * 0.75, burst ? T.C.alert : T.C.label);
    }
    T.header(g, m, y0, w - 2 * m, u, title, `peak ${fpk.toFixed(2)} MHz  ${pk} dBm`, burst);
    this.chips(g, w, u, m, y0 + u * 3);
    // шкала цвета
    const sx = m, sy = y0 + u * 3.3, sw = u * 9;
    for (let i = 0; i < 40; i++) { g.fillStyle = T.heat(i / 39); g.fillRect(sx + i * sw / 40, sy, sw / 40 + 0.5, u * 0.45); }
    T.txt(g, '-110', sx, sy + u * 1.4, u * 0.62, T.C.dim); T.txt(g, '-40 DBM', sx + sw, sy + u * 1.4, u * 0.62, T.C.dim, 'm', 'right');
    T.footer(g, m, h - u * 2.2, w - 2 * m, u, 'drag: orbit  ·  time flows into depth', `${R} rows · receive only`);
  },

  // ---------- MONITOR: уровень и всплески ----------
  monitor(g, w, h, u, m, y0, dt, d, title) {
    const portrait = h > w * 1.05, burst = !!d.burst, rssi = d.rssi || [];
    this.flash = Math.max(burst ? 1 : 0, this.flash - dt * 1.2);
    T.header(g, m, y0, w - 2 * m, u, title, `${(d.mon || 0).toFixed(2)} MHz · floor ${d.floor} dBm · ${d.events || 0} events`, burst);
    this.chips(g, w, u, m, y0 + u * 3);
    const top = y0 + u * 6, bot = h - u * 3;
    const cx = m, cw = portrait ? w - 2 * m : (w - 2 * m) * 0.66, ch = portrait ? (bot - top) * 0.6 : bot - top;
    // график
    T.panel(g, cx, top, cw, ch, u);
    const L = cx + u * 3.4, Rr = cx + cw - u * 1.2, Tp = top + u * 4.2, B = top + ch - u * 1.6;
    const Y = v => B - T.clamp((v + 110) / 80) * (B - Tp);
    g.lineWidth = 1;
    for (let v = -110; v <= -30; v += 10) { g.strokeStyle = v % 20 ? 'rgba(32,50,64,.5)' : T.C.line; g.beginPath(); g.moveTo(L, Y(v)); g.lineTo(Rr, Y(v)); g.stroke(); if (v % 20 === 0) T.txt(g, v, L - u * 0.5, Y(v) + u * 0.3, u * 0.65, T.C.dim, 'm', 'right'); }
    // порог (пол + запас) пунктиром
    if (d.floor != null) {
      g.setLineDash([u * 0.5, u * 0.4]); g.strokeStyle = 'rgba(150,175,192,.6)';
      g.beginPath(); g.moveTo(L, Y(d.floor)); g.lineTo(Rr, Y(d.floor)); g.stroke(); g.setLineDash([]);
      T.txt(g, 'FLOOR', Rr, Y(d.floor) - u * 0.35, u * 0.6, T.C.label, 'm', 'right');
    }
    // сглаживание линии между пакетами
    if (!this.chart || this.chart.length !== rssi.length) this.chart = Float32Array.from(rssi);
    for (let i = 0; i < rssi.length; i++) this.chart[i] = T.lerp(this.chart[i], rssi[i], T.ease(dt, 0.15));
    if (this.chart.length > 1) {
      const N = this.chart.length, px = i => L + i / (N - 1) * (Rr - L);
      g.beginPath(); for (let i = 0; i < N; i++) { const yy = Y(this.chart[i]); i ? g.lineTo(px(i), yy) : g.moveTo(px(i), yy); }
      g.strokeStyle = burst ? T.C.alert : T.C.acc; g.lineWidth = 2; g.stroke();
      g.lineTo(Rr, B); g.lineTo(L, B); g.closePath();
      const gr = g.createLinearGradient(0, Tp, 0, B); gr.addColorStop(0, burst ? 'rgba(255,122,26,.3)' : 'rgba(62,230,255,.28)'); gr.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = gr; g.fill();
      const last = this.chart[N - 1];
      T.diamond(g, Rr, Y(last), u * 0.45, burst ? T.C.alert : T.C.hot, true);
    }
    T.bloom(this.cv, 0.55 + 0.6 * this.flash);
    // крупное значение и баннер
    const last = rssi.length ? rssi[rssi.length - 1] : -120;
    T.txt(g, '// LEVEL', cx + u, top + u * 1.6, u * 1.0, T.C.acc, 't');
    T.bigNum(g, last, cx + u * 6.5, top + u * 3.3, u * 2.8, burst ? T.C.alert : T.heat(0.3 + 0.7 * lvl(last)), 'DBM');
    {
      const bw = u * 13, bh = u * 2.4, bx = cx + cw - bw - u, by = top + u * 0.9, pulse = burst ? 0.6 + 0.4 * Math.sin(this.t * 10) : 1;
      T.cutPath(g, bx, by, bw, bh, u * 0.6);
      g.fillStyle = burst ? `rgba(255,122,26,${0.22 * pulse})` : 'rgba(62,230,255,.06)'; g.fill();
      g.strokeStyle = burst ? T.C.alert : T.C.line; g.lineWidth = 1.2; g.stroke();
      T.txt(g, burst ? '// BURST' : '// LISTENING', bx + bw / 2, by + bh * 0.68, u * 1.15, burst ? T.C.alert : T.C.label, 't', 'center');
    }
    // список событий
    const ex = portrait ? m : cx + cw + u, ey = portrait ? top + ch + u : top, ew = portrait ? w - 2 * m : w - m - ex, eh = portrait ? bot - ey : ch;
    T.panel(g, ex, ey, ew, eh, u);
    T.txt(g, '// EVENTS', ex + u, ey + u * 1.6, u * 1.0, T.C.acc, 't');
    T.txt(g, d.events || 0, ex + ew - u, ey + u * 1.8, u * 1.8, T.C.hot, 't', 'right');
    let yy = ey + u * 3.4;
    const evs = d.ev || [];
    if (!evs.length) T.txt(g, 'NO BURSTS YET', ex + u, yy + u, u * 0.8, T.C.dim);
    for (const e of evs) {
      if (yy + u * 2.6 > ey + eh) break;
      const v = lvl(e.pk);
      g.fillStyle = T.C.line; g.fillRect(ex + u, yy + u * 2.3, ew - 2 * u, 1);
      T.txt(g, T.ago(e.ago) + ' AGO', ex + u, yy + u * 0.9, u * 0.75, T.C.label);
      T.txt(g, e.ms + ' MS', ex + u, yy + u * 1.9, u * 0.65, T.C.dim);
      T.segBar(g, ex + ew * 0.42, yy + u * 0.8, ew * 0.3, u * 0.45, v, 8, false, e.pk > -55);
      T.txt(g, e.pk, ex + ew - u, yy + u * 1.6, u * 1.4, e.pk > -55 ? T.C.alert : T.heat(0.3 + 0.7 * v), 't', 'right');
      yy += u * 2.6;
    }
    T.footer(g, m, h - u * 2.2, w - 2 * m, u, 'level at band frequency · tap band to switch', 'receive only');
  },
});
})();
