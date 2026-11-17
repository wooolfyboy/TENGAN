// ---- WI-FI: channels - 3D ridges going into the distance; networks - glass table with a card ----
// Version B: depth, fog, glow. Beauty over accuracy: heights 'glide', row order is sticky,
// between scans (every ~20 s) the scene quietly 'breathes'.
(() => {
const WF = {
  // best channel of 1/6/11: least interference from neighbors (like on the stick)
  // sticky: changes only if another channel is noticeably (20%) freer
  bestCh(nets) {
    const sc = {};
    for (const c of [1, 6, 11]) {
      let s = 0;
      for (const n of nets) { const d = Math.abs(n.c - c); if (d < 5) s += (5 - d) * (Math.max(0, (n.rs ?? n.r) + 100)); }
      sc[c] = s;
    }
    let best = WF._best || 1;
    for (const c of [1, 6, 11]) if (sc[c] < sc[best] * 0.8) best = c;
    return (WF._best = best);
  },
  sig: r => T.clamp((r + 95) / 55),
  ssid: n => n.s ? n.s : '<hidden>',
  open: n => n.e === 'open',
  encG: n => n.e === 'open' ? 'O' : /ENT/.test(n.e || '') ? 'E' : /3/.test(n.e || '') ? '3' : /2/.test(n.e || '') ? '2' : 'W',
  dist: r => Math.pow(10, (-40 - r) / 30),
  list: () => [...S.aps.values()].filter(n => n.rs != null),
};

T.views.push({
  id: 'wifi', title: 'Wi-Fi', want: 'wifi', subs: ['Channels', 'Networks'], sub: 0,
  mount(main) {
    this.root = T.el('div', 'view');
    const st = T.el('div', 'stage'); this.root.appendChild(st);
    this.cv = T.canvas(st); main.appendChild(this.root);
    this.cam = T.orbit(0, 0.5, 3.4);
    this.drag = T.drag(this.cv.cv, this.cam, { minPitch: 0.25, maxPitch: 1.0 });
    this.drag.onTap = (x, y) => this.tap(x, y);
    this.dust = T.dust(140, 1.9, 1.2);
    this.z = {};                // smoothed depth of each network
    this.order = []; this.orderT = 0;
    this.sel = null; this.t = 0;
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  onSub(i) { this.sub = i; },
  tap(x, y) {
    const h = T.hits.at(x, y);
    if (!h) { this.sel = null; return; }
    if (h.k === 'net') this.sel = this.sel === h.id ? null : h.id;
    else if (h.k === 'close') this.sel = null;
  },
  frame(dt, t) {
    this.t += dt;
    const { g, w, h } = this.cv;
    T.hits.clear();
    T.backdrop(g, w, h, 0.5, 0.6);
    const u = T.unit(w, h), m = u * 1.6, y0 = 50;
    const nets = WF.list();
    if (this.sel && !S.aps.has(this.sel)) this.sel = null;
    if (!S.wifi && !nets.length) {
      T.header(g, m, y0, w - 2 * m, u, this.sub ? 'Networks' : 'Channels', 'waiting', false);
      T.emptyState(g, w, h, u, 'SCANNING', 'first scan takes a few seconds');
      T.finish(g, w, h); return;
    }
    if (this.sub === 0) this.channels(g, w, h, u, m, y0, dt, nets);
    else this.networks(g, w, h, u, m, y0, dt, nets);
    T.finish(g, w, h);
  },

  // ---------- CHANNELS: ridges ----------
  channels(g, w, h, u, m, y0, dt, nets) {
    const cam = this.cam, tt = this.t, portrait = h > w * 1.05;
    this.drag.tick(dt);
    if (this.drag.idle()) cam.yaw = T.lerp(cam.yaw, 0.22 * Math.sin(tt * 0.08), T.ease(dt, 2));
    cam.yaw = T.clamp(cam.yaw, -0.7, 0.7);
    cam.frame(w / 2, portrait ? h * 0.5 : h * 0.54, portrait ? w * 0.95 : Math.min(w * 0.74, h * 1.3));
    const X = c => (c - 7) * 0.2;                     // channel -> x
    const sorted = [...nets].sort((a, b) => b.rs - a.rs);
    sorted.forEach((n, i) => {                        // strong in front, weak farther
      const tz = -0.55 + (sorted.length > 1 ? i / (sorted.length - 1) : 0) * 1.9;
      this.z[n.k] = this.z[n.k] == null ? tz : T.lerp(this.z[n.k], tz, T.ease(dt, 1.2));
    });
    // floor, channel axis
    T.floorGrid(g, cam, 1.9, 0.25);
    const zF = -0.85;
    for (let c = 1; c <= 13; c++) {
      const p = cam.p(X(c), 0, zF), q = cam.p(X(c), 0, zF + 0.06);
      g.strokeStyle = c === 1 || c === 6 || c === 11 ? T.C.acc : T.C.dim; g.lineWidth = 1;
      g.beginPath(); g.moveTo(p[0], p[1]); g.lineTo(q[0], q[1]); g.stroke();
      T.txt(g, c, p[0], p[1] + u * 1.25, u * (c === 1 || c === 6 || c === 11 ? 1.0 : 0.8), c === 1 || c === 6 || c === 11 ? T.C.hot : T.C.label, 't', 'center');
    }
    { const a = cam.p(X(1) - 0.5, 0, zF), b = cam.p(X(13) + 0.5, 0, zF); g.strokeStyle = `rgba(${T.RGB.acc},.35)`; g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.stroke(); }
    // best channel - diamond on the floor
    const best = WF.bestCh(nets);
    { const p = cam.p(X(best), 0, zF - 0.18); T.diamond(g, p[0], p[1], u * 0.55, T.C.acc, true); T.txt(g, 'BEST', p[0] + u * 0.9, p[1] + u * 0.3, u * 0.72, T.C.acc); }
    // passing 'scan beam': quiet flicker between scans
    {
      const sx = -1.6 + ((tt * 0.28) % 1) * 3.2, a = cam.p(sx, 0, zF), b = cam.p(sx, 0, 1.5), c2 = cam.p(sx, 0.9, 1.5), d = cam.p(sx, 0.9, zF);
      const gr = g.createLinearGradient(a[0], a[1], d[0], d[1]); gr.addColorStop(0, `rgba(${T.RGB.acc},.10)`); gr.addColorStop(1, `rgba(${T.RGB.acc},0)`);
      g.fillStyle = gr; g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.lineTo(c2[0], c2[1]); g.lineTo(d[0], d[1]); g.closePath(); g.fill();
      g.strokeStyle = `rgba(${T.RGB.acc},.25)`; g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.stroke();
    }
    // ridges: far ones first
    const draw = sorted.map(n => ({ n, z: this.z[n.k], d: cam.p(X(n.c), 0, this.z[n.k])[3] })).sort((a, b) => b.d - a.d);
    const tops = [];
    for (const { n, z } of draw) {
      const s = WF.sig(n.rs), H = (0.06 + 0.8 * s) * n.h, hw = 0.44, cx = X(n.c);
      const fog = T.clamp(1.15 - (z + 1) * 0.3, 0.3, 1), breath = 0.85 + 0.15 * Math.sin(this.t * 1.6 + (T.hash(n.k) % 100));
      const col = WF.open(n) ? T.ALERT : T.heatRGB(0.25 + 0.75 * s), sel = this.sel === n.k;
      const pts = [];
      for (let i = 0; i <= 28; i++) { const k = i / 28 * 2 - 1, y = H * Math.pow(Math.cos(k * Math.PI / 2), 2); pts.push(cam.p(cx + k * hw, y, z)); }
      const b0 = cam.p(cx - hw, 0, z), b1 = cam.p(cx + hw, 0, z), top = cam.p(cx, H, z);
      // curtain: fill (covers far ones) + color gradient
      g.beginPath(); g.moveTo(b0[0], b0[1]); for (const p of pts) g.lineTo(p[0], p[1]); g.lineTo(b1[0], b1[1]); g.closePath();
      g.fillStyle = `rgba(${T.RGB.bg2},.82)`; g.fill();
      const gr = g.createLinearGradient(0, top[1], 0, b0[1]);
      gr.addColorStop(0, T.rgb(col, (sel ? 0.42 : 0.26) * fog * breath)); gr.addColorStop(1, T.rgb(col, 0));
      g.fillStyle = gr; g.fill();
      g.beginPath(); pts.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]));
      g.strokeStyle = T.rgb(sel ? [235, 250, 255] : col, fog * (0.6 + 0.4 * breath)); g.lineWidth = sel ? 2.4 : 1.5; g.stroke();
      // reflection on the floor
      g.beginPath(); g.moveTo(b0[0], b0[1]); g.lineTo(b1[0], b1[1]); g.strokeStyle = T.rgb(col, 0.35 * fog); g.lineWidth = 1; g.stroke();
      tops.push({ n, top, s, fog, H, z, cx });
      T.hits.circle(top[0], top[1], Math.max(u * 1.6, top[2] * 0.06), { k: 'net', id: n.k });
    }
    this.dust.tick(dt); this.dust.draw(g, cam);
    T.bloom(this.cv, 0.8);
    // flags on the peaks of the strongest
    const flags = [...tops].sort((a, b) => b.s - a.s).slice(0, portrait ? 3 : 5);
    const placed = [];                                // flags do not overlap
    for (const f of [...tops].sort((a, b) => a.top[1] - b.top[1])) {
      if (!flags.includes(f) && this.sel !== f.n.k) continue;
      const p = f.top, open = WF.open(f.n);
      let fy = p[1] - u * 2.2;
      for (let tries = 0; tries < 6; tries++) {
        const hit = placed.find(r => p[0] + u * 12 > r[0] && p[0] < r[0] + u * 12 && Math.abs(fy - r[1]) < u * 2.2);
        if (!hit) break; fy = hit[1] - u * 2.3;
      }
      placed.push([p[0], fy]);
      g.strokeStyle = open ? T.C.alert : `rgba(${T.RGB.hot},.7)`; g.lineWidth = 1;
      g.beginPath(); g.moveTo(p[0], p[1] - 2); g.lineTo(p[0], fy); g.lineTo(p[0] + u * 0.7, fy); g.stroke();
      T.diamond(g, p[0], p[1], u * 0.3, open ? T.C.alert : T.C.hot, true);
      g.font = `500 ${u * 0.8}px ${T.FC}`;
      T.txt(g, T.fit(g, WF.ssid(f.n), u * 11), p[0] + u * 0.9, fy + u * 0.3, u * 0.8, open ? T.C.alert : T.C.hot, 'c');
      T.txt(g, `CH ${f.n.c} · ${Math.round(f.n.rs)} DBM`, p[0] + u * 0.9, fy + u * 1.25, u * 0.65, T.C.label);
    }
    // header
    const age = S.wifiT ? (performance.now() - S.wifiT) / 1000 : 0;
    T.header(g, m, y0, w - 2 * m, u, 'Channels', `${nets.length} networks · best ch ${best} · scan ${T.ago(age)} ago`);
    // channel load: mini panel
    const pw = Math.min(u * 17, w * 0.32), ph = u * 6, px = w - m - pw, py = y0 + u * 3.2;
    T.panel(g, px, py, pw, ph, u);
    T.txt(g, 'CHANNEL LOAD', px + u * 0.8, py + u * 1.3, u * 0.7, T.C.dim);
    const load = new Array(14).fill(0);
    for (const n of nets) for (let c = 1; c <= 13; c++) { const d = Math.abs(n.c - c); if (d <= 2) load[c] += WF.sig(n.rs) * (1 - d / 3); }
    const mx = Math.max(1, ...load), bw = (pw - u * 1.6) / 13;
    for (let c = 1; c <= 13; c++) {
      const v = load[c] / mx, bh = (ph - u * 3) * v, bx = px + u * 0.8 + (c - 1) * bw, by = py + ph - u * 1.2;
      g.fillStyle = T.C.line; g.fillRect(bx + bw * 0.15, py + u * 1.8, bw * 0.7, ph - u * 3);
      g.fillStyle = c === best ? T.C.acc : T.heat(0.25 + 0.6 * v); g.fillRect(bx + bw * 0.15, by - bh, bw * 0.7, bh);
      if (c % 5 === 1) T.txt(g, c, bx + bw / 2, py + ph - u * 0.25, u * 0.6, T.C.dim, 'm', 'center');
    }
    // selected network - callout
    if (this.sel) {
      const f = tops.find(x => x.n.k === this.sel);
      if (f) this.callout(g, u, f.n, f.top, w, h);
    }
    T.footer(g, m, h - u * 2.2, w - 2 * m, u, 'drag: orbit  ·  tap peak: details', 'scan every ~20 s');
  },
  callout(g, u, n, p, w, h) {
    const bw = u * 15, bh = u * 7.6;
    let bx = p[0] + u * 3, by = p[1] - bh - u * 2;
    if (bx + bw > w - u) bx = p[0] - u * 3 - bw;
    by = T.clamp(by, 50 + u * 3, h - bh - u * 3);
    const open = WF.open(n), col = open ? T.C.alert : T.heat(0.4 + 0.6 * WF.sig(n.rs));
    g.strokeStyle = T.C.hot; g.lineWidth = 1;
    g.beginPath(); g.moveTo(p[0], p[1]); g.lineTo(bx < p[0] ? bx + bw : bx, by + bh * 0.5); g.stroke();
    T.panel(g, bx, by, bw, bh, u, `rgba(${T.RGB.bg},.78)`, T.C.acc);
    g.font = `500 ${u * 1.15}px ${T.FC}`;
    T.txt(g, T.fit(g, WF.ssid(n), bw - u * 2), bx + u, by + u * 1.7, u * 1.15, T.C.hot, 'c');
    T.txt(g, `${(n.v || 'unknown vendor')} · ${n.e}`.toUpperCase(), bx + u, by + u * 2.8, u * 0.7, open ? T.C.alert : T.C.label);
    T.bigNum(g, Math.round(n.rs), bx + u, by + u * 5.7, u * 2.7, col, 'DBM');
    T.spark(g, bx + u * 7.2, by + u * 3.6, bw - u * 8.2, u * 2.2, n.hist.slice(-30), -100, -35, open ? T.C.alert : T.C.acc);
    T.segBar(g, bx + u, by + u * 6.5, bw - u * 2, u * 0.4, WF.sig(n.rs), 16, false, open);
    T.hits.rect(bx, by, bw, bh, { k: 'close' });
  },

  // ---------- NETWORKS: table + card ----------
  networks(g, w, h, u, m, y0, dt, nets) {
    const now = performance.now();
    // sticky order: neighbors swap only if the difference is over 6 dB
    const have = new Set(nets.map(n => n.k));
    this.order = this.order.filter(k => have.has(k));
    for (const n of nets) if (!this.order.includes(n.k)) {
      let i = 0; while (i < this.order.length && (S.aps.get(this.order[i]).rs >= n.rs)) i++;
      this.order.splice(i, 0, n.k);
    }
    if (now - this.orderT > 3000) {
      this.orderT = now;
      for (let i = 0; i + 1 < this.order.length; i++) {
        const a = S.aps.get(this.order[i]), b = S.aps.get(this.order[i + 1]);
        if (b.rs - a.rs > 6) { this.order[i] = b.k; this.order[i + 1] = a.k; }
      }
    }
    const best = WF.bestCh(nets), open = nets.filter(WF.open).length;
    T.header(g, m, y0, w - 2 * m, u, 'Networks', `${nets.length} networks${open ? ' · ' + open + ' open' : ''} · best ch ${best}`, false);
    const top = y0 + u * 3.2, bot = h - u * 2.8, portrait = h > w * 1.05;
    const sel = this.sel && S.aps.get(this.sel);
    let tx = m, tw = w - 2 * m, th = bot - top, cx, cy, cw, ch;
    if (sel) {
      if (portrait) { th = (bot - top) * 0.52; cx = m; cy = top + th + u; cw = w - 2 * m; ch = bot - cy; }
      else { tw = (w - 2 * m) * 0.6; cx = m + tw + u; cy = top; cw = w - m - cx; ch = bot - top; }
    }
    this.table(g, tx, top, tw, th, u);
    if (sel) this.card(g, cx, cy, cw, ch, u, sel);
    T.footer(g, m, h - u * 2.2, w - 2 * m, u, sel ? 'tap row: switch  ·  ‹ all: back' : 'tap row: details', 'signal shown steady (±3 dB)');
  },
  table(g, x, y, w, h, u) {
    T.panel(g, x, y, w, h, u);
    const pad = u * 0.8, rowH = u * 2.6, wide = w > u * 44;
    const cols = { g: x + pad, n: x + pad + rowH * 1.0, ch: x + w * (wide ? 0.42 : 0.5), enc: x + w * (wide ? 0.49 : 0.6), bar: x + w * (wide ? 0.58 : 0.7), sp: x + w * 0.72, db: x + w - pad };
    const barW = wide ? w * 0.12 : w * 0.17;
    let cy = y + pad;
    T.txt(g, 'NETWORK', cols.n, cy + u * 0.8, u * 0.68, T.C.dim);
    T.txt(g, 'CH', cols.ch, cy + u * 0.8, u * 0.68, T.C.dim);
    T.txt(g, 'SEC', cols.enc, cy + u * 0.8, u * 0.68, T.C.dim);
    if (wide) T.txt(g, 'HISTORY', cols.sp, cy + u * 0.8, u * 0.68, T.C.dim);
    T.txt(g, 'DBM', cols.db, cy + u * 0.8, u * 0.68, T.C.dim, 'm', 'right');
    cy += u * 1.4;
    for (const k of this.order) {
      if (cy + rowH > y + h - pad * 0.5) break;
      const n = S.aps.get(k); if (!n) continue;
      const on = this.sel === k, open = WF.open(n), s = WF.sig(n.rs);
      g.globalAlpha = 0.35 + 0.65 * n.h;
      if (on) { g.fillStyle = `rgba(${T.RGB.acc},.08)`; g.fillRect(x + 2, cy, w - 4, rowH); T.brackets(g, x + 2, cy, w - 4, rowH, u * 0.45, T.C.acc, 1); }
      const gs = rowH * 0.6, gc = open ? T.C.alert : on ? T.C.hot : T.C.acc;
      T.glyph(g, cols.g, cy + (rowH - gs) / 2, gs, WF.encG(n), gc);
      g.font = `500 ${rowH * 0.36}px ${T.FC}`;
      T.txt(g, T.fit(g, WF.ssid(n), cols.ch - cols.n - u), cols.n, cy + rowH * 0.46, rowH * 0.36, n.s ? (on ? T.C.hot : T.C.label) : T.C.dim, 'c');
      g.font = `${rowH * 0.27}px ${T.FM}`;
      T.txt(g, T.fit(g, ((n.v || '—') + ' · ' + n.b).toUpperCase(), cols.ch - cols.n - u), cols.n, cy + rowH * 0.82, rowH * 0.27, T.C.dim);
      T.txt(g, n.c, cols.ch, cy + rowH * 0.62, rowH * 0.42, T.C.hot, 't');
      T.txt(g, String(n.e).toUpperCase(), cols.enc, cy + rowH * 0.6, rowH * 0.3, open ? T.C.alert : T.C.label);
      T.segBar(g, cols.bar, cy + rowH * 0.4, barW, rowH * 0.2, s, 10, false, open);
      if (wide) T.spark(g, cols.sp, cy + rowH * 0.18, cols.db - cols.sp - u * 3.6, rowH * 0.62, n.hist.slice(-30), -100, -35, open ? T.C.alert : T.C.acc);
      T.txt(g, Math.round(n.rs), cols.db, cy + rowH * 0.68, rowH * 0.52, open ? T.C.alert : T.heat(0.4 + 0.6 * s), 't', 'right');
      g.globalAlpha = 1;
      T.hits.rect(x, cy, w, rowH, { k: 'net', id: k });
      cy += rowH;
    }
  },
  card(g, x, y, w, h, u, n) {
    T.panel(g, x, y, w, h, u, 'rgba(10,20,28,.55)', T.C.acc);
    const p = u * 1.1, open = WF.open(n), s = WF.sig(n.rs), col = open ? T.C.alert : T.heat(0.4 + 0.6 * s);
    let cy = y + p;
    T.txt(g, '// NETWORK', x + p, cy + u * 0.9, u * 1.05, T.C.acc, 't');
    T.txt(g, '‹ ALL', x + w - p, cy + u * 0.9, u * 0.9, T.C.label, 'm', 'right');
    T.hits.rect(x + w - p - u * 4, cy - u * 0.3, u * 4.6, u * 1.8, { k: 'close' });
    cy += u * 2.6;
    g.font = `500 ${u * 1.7}px ${T.FC}`;
    T.txt(g, T.fit(g, WF.ssid(n), w - 2 * p), x + p, cy + u * 1.3, u * 1.7, n.s ? T.C.hot : T.C.dim, 'c'); cy += u * 2.1;
    T.txt(g, `${n.v || 'unknown vendor'} · ${n.e}${open ? ' · NO PASSWORD' : ''}`.toUpperCase(), x + p, cy + u * 0.6, u * 0.82, open ? T.C.alert : T.C.label); cy += u * 1.4;
    const big = Math.min(u * 4.6, h * 0.16);
    T.bigNum(g, Math.round(n.rs), x + p, cy + big * 0.8, big, col, 'DBM');
    const d = WF.dist(n.rs);
    T.txt(g, '≈ ' + (d < 10 ? d.toFixed(1) : Math.round(d)) + ' M', x + w - p, cy + big * 0.75, u * 1.2, T.C.label, 'm', 'right');
    cy += big + u * 0.6;
    T.segBar(g, x + p, cy, w - 2 * p, u * 0.55, s, 20, true, open); cy += u * 1.6;
    T.txt(g, 'SIGNAL · LAST SCANS', x + p, cy + u * 0.7, u * 0.7, T.C.dim); cy += u * 1.1;
    const kv = [['CHANNEL', n.c + (n.c === WF.bestCh(WF.list()) ? '  (best)' : '')], ['BSSID', n.b], ['SECURITY', n.e]];
    const sh = Math.max(u * 3, Math.min(u * 7, y + h - cy - kv.length * u * 1.45 - p - u));
    g.strokeStyle = T.C.line; g.lineWidth = 1; g.strokeRect(x + p, cy, w - 2 * p, sh);
    T.spark(g, x + p, cy, w - 2 * p, sh, n.hist, -100, -35, open ? T.C.alert : T.C.acc); cy += sh + u * 1.0;
    for (const [k, v] of kv) {
      if (cy + u * 1.4 > y + h - p * 0.5) break;
      T.txt(g, k, x + p, cy + u * 0.9, u * 0.8, T.C.dim); T.txt(g, String(v), x + w - p, cy + u * 0.9, u * 0.8, T.C.hot, 'm', 'right'); cy += u * 1.45;
    }
  },
});
})();
