// ---- BLUETOOTH (версия B): радар в глубину, 3D-город, плотная таблица ----
// Общие кусочки (T.bleUI) использует и «Обзор».
const BU = T.bleUI = {};
BU.G = { phone: 'P', audio: 'A', watch: 'W', tracker: 'T', computer: 'C', TV: 'V', tv: 'V', sensor: 'S', band: 'B', input: 'G', beacon: 'i', unknown: '?' };
BU.glyph = e => BU.G[e.type] || (e.type ? String(e.type)[0].toUpperCase() : '?');
BU.ang = id => (T.hash(id) % 360) * Math.PI / 180;
BU.rad = rs => 0.08 + 0.88 * (1 - T.pct(rs));          // сильный — ближе к центру
BU.list = () => [...S.dev.values()].filter(e => e.h > 0.02 && e.rs != null);
BU.label = e => e.name || (e.type && e.type !== 'unknown' ? e.type : 'device');
BU.tr = e => e.type === 'tracker';
BU.rgb = (e, v) => BU.tr(e) ? T.ALERT : T.heatRGB(v);
BU.crossed = (a, b, x) => b >= a ? (x > a && x <= b) : (x > a || x <= b);
BU.dist = r => { const d = Math.pow(10, (-59 - r) / 20); return d < 10 ? d.toFixed(1) : String(Math.round(d)); };
BU.apiBase = () => { const h = new URLSearchParams(location.search).get('host'); return h ? `http://${h}` : ''; };
// число, которое не мигает: новое значение принимается, только если продержалось 1.5 с
BU.hold = (ms = 1500) => ({ v: null, c: null, t: 0, set(x, now) { if (this.v == null) { this.v = x; return x; } if (x === this.v) { this.c = null; return x; } if (x !== this.c) { this.c = x; this.t = now; } else if (now - this.t > ms) { this.v = x; this.c = null; } return this.v; } });

// луч: шаг и засечки (ping — время, когда луч прошёл цель)
BU.sweeper = (period = 3.2) => ({
  a: 0, pings: new Map(),
  tick(dt, t) {
    const prev = this.a; this.a = (this.a + dt * T.TAU / (T.reduce ? 7 : period)) % T.TAU;
    for (const e of S.dev.values()) if (e.alive && BU.crossed(prev, this.a, BU.ang(e.id))) this.pings.set(e.id, t);
  },
  ping(id, t) { const p = this.pings.get(id); return p == null ? 9 : t - p; },
});

// ---- радар в перспективе: диск, кольца, луч-клин со стенкой, столбы света ----
// opt: {sel, u, t, labels (сколько подписать), hits}
BU.scene = (g, cam, sw, opt) => {
  const { u, t } = opt;
  T.floorGrid(g, cam, 1.7, 0.2);
  for (let i = 1; i <= 4; i++) {
    g.beginPath();
    for (let k = 0; k <= 96; k++) { const a = k / 96 * T.TAU, p = cam.p(Math.cos(a) * i / 4, 0, Math.sin(a) * i / 4); k ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); }
    g.strokeStyle = i === 4 ? T.C.acc : 'rgba(90,112,128,.5)'; g.lineWidth = i === 4 ? 1.6 : 1; g.stroke();
  }
  g.beginPath();
  for (let a = 0; a < 360; a += 10) { const r = a * Math.PI / 180, l = a % 30 ? 1.03 : 1.07, p = cam.p(Math.cos(r), 0, Math.sin(r)), q = cam.p(Math.cos(r) * l, 0, Math.sin(r) * l); g.moveTo(p[0], p[1]); g.lineTo(q[0], q[1]); }
  g.strokeStyle = T.C.acc; g.lineWidth = 1; g.stroke();
  // клин луча на полу
  const tail = 1.3, N = 26, o = cam.p(0, 0, 0), s = sw.a;
  for (let i = 0; i < N; i++) {
    const a0 = s - tail * (1 - i / N), a1 = s - tail * (1 - (i + 1) / N), v = (i + 1) / N, p = cam.p(Math.cos(a0), 0, Math.sin(a0)), q = cam.p(Math.cos(a1), 0, Math.sin(a1));
    g.fillStyle = T.heat(0.2 + 0.65 * v, 0.3 * v * v); g.beginPath(); g.moveTo(o[0], o[1]); g.lineTo(p[0], p[1]); g.lineTo(q[0], q[1]); g.closePath(); g.fill();
  }
  { // стенка луча
    const rx = Math.cos(s), rz = Math.sin(s), a = cam.p(0, 0, 0), b = cam.p(rx, 0, rz), c = cam.p(rx, 0.2, rz), d = cam.p(0, 0.2, 0);
    const gr = g.createLinearGradient(a[0], a[1], d[0], d[1]); gr.addColorStop(0, 'rgba(62,230,255,.22)'); gr.addColorStop(1, 'rgba(62,230,255,0)');
    g.fillStyle = gr; g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.lineTo(c[0], c[1]); g.lineTo(d[0], d[1]); g.closePath(); g.fill();
    g.strokeStyle = T.C.hot; g.lineWidth = 2; g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.stroke();
  }
  // столбы: дальние раньше
  const list = BU.list(), rank = [...list].sort((a, b) => b.rs - a.rs);
  const items = list.map(e => { const r = BU.rad(e.rs), a = BU.ang(e.id), x = Math.cos(a) * r, z = Math.sin(a) * r; return { e, x, z, p0: cam.p(x, 0, z) }; }).sort((a, b) => b.p0[3] - a.p0[3]);
  let selTop = null;
  for (const it of items) {
    const e = it.e, sg = T.pct(e.rs), hh = (0.06 + 0.55 * sg * sg) * e.h, p0 = it.p0, p1 = cam.p(it.x, hh, it.z);
    const since = (((s - BU.ang(e.id)) % T.TAU) + T.TAU) % T.TAU / T.TAU, ph = 1 - since, fog = T.clamp(1.15 - (p0[3] + 1) * 0.28, 0.35, 1) * e.h;
    const pa = sw.ping(e.id, t);
    if (pa < 1.1) {
      g.strokeStyle = BU.tr(e) ? `rgba(255,122,26,${(1 - pa / 1.1) * 0.9 * e.h})` : `rgba(62,230,255,${(1 - pa / 1.1) * 0.9 * e.h})`; g.lineWidth = 1.2; g.beginPath();
      for (let i = 0; i <= 28; i++) { const a = i / 28 * T.TAU, rr = 0.03 + pa * 0.15, q = cam.p(it.x + Math.cos(a) * rr, 0, it.z + Math.sin(a) * rr); i ? g.lineTo(q[0], q[1]) : g.moveTo(q[0], q[1]); }
      g.stroke();
    }
    const col = BU.rgb(e, 0.35 + 0.65 * sg);
    const gr = g.createLinearGradient(p0[0], p0[1], p1[0], p1[1]); gr.addColorStop(0, T.rgb(col, 0.05 * fog)); gr.addColorStop(1, T.rgb(col, (0.5 + 0.5 * ph) * fog));
    g.strokeStyle = gr; g.lineWidth = Math.max(1.5, p0[2] * 0.016); g.lineCap = 'round'; g.beginPath(); g.moveTo(p0[0], p0[1]); g.lineTo(p1[0], p1[1]); g.stroke(); g.lineCap = 'butt';
    const rr = p1[2] * (0.014 + 0.02 * sg) * (1 + 0.6 * Math.max(0, 1 - pa * 2));
    g.fillStyle = T.rgb(BU.tr(e) ? col : T.heatRGB(0.55 + 0.45 * ph), fog); g.beginPath(); g.arc(p1[0], p1[1], rr, 0, T.TAU); g.fill();
    g.fillStyle = T.rgb(col, 0.5 * fog); g.beginPath(); g.ellipse(p0[0], p0[1], p0[2] * 0.02, p0[2] * 0.02 * Math.sin(cam.pitch), 0, 0, T.TAU); g.fill();
    if (opt.hits) { T.hits.circle(p1[0], p1[1], Math.max(u * 1.6, rr * 2.5), { k: 'dev', id: e.id }); T.hits.circle((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, u * 1.2, { k: 'dev', id: e.id }); }
    if (e.id === opt.sel) selTop = { p: p1, e };
    else if (opt.labels && rank.indexOf(e) < opt.labels && opt.sel == null) { g.font = `500 ${u * 0.72}px ${T.FC}`; T.txt(g, T.fit(g, BU.label(e).toUpperCase(), u * 9), p1[0] + rr + u * 0.5, p1[1] + u * 0.25, u * 0.72, BU.tr(e) ? T.C.alert : T.C.label, 'c'); }
  }
  return selTop;
};

// выноска-карточка у цели (Scope и City)
BU.card = (g, p, e, u, W, H, top) => {
  const sg = T.pct(e.rs), bw = u * 15, bh = u * 8.2;
  let bx = p[0] + u * 4, by = p[1] - u * 7;
  if (bx + bw > W - u) bx = p[0] - u * 4 - bw;
  by = T.clamp(by, top, H - bh - u * 3);
  T.diamond(g, p[0], p[1], u * 0.9, T.C.hot, false);
  const ax = bx > p[0] ? bx : bx + bw;
  g.strokeStyle = T.C.hot; g.lineWidth = 1; g.beginPath(); g.moveTo(p[0] + (bx > p[0] ? 1 : -1) * u * 0.7, p[1] - u * 0.7); g.lineTo(ax + (bx > p[0] ? -1 : 1) * u * 1.2, by + bh * 0.55); g.lineTo(ax, by + bh * 0.55); g.stroke();
  T.panel(g, bx, by, bw, bh, u, 'rgba(5,8,13,.78)', T.C.acc);
  g.font = `500 ${u * 1.2}px ${T.FC}`; T.txt(g, T.fit(g, BU.label(e).toUpperCase(), bw - u * 2), bx + u, by + u * 1.7, u * 1.2, T.C.hot, 'c');
  g.font = `${u * 0.75}px ${T.FM}`; T.txt(g, T.fit(g, ((e.vend || '—') + ' · ' + (e.type || '')).toUpperCase(), bw - u * 2), bx + u, by + u * 2.8, u * 0.75, BU.tr(e) ? T.C.alert : T.C.label);
  T.bigNum(g, Math.round(e.rs), bx + u, by + u * 5.6, u * 2.8, BU.tr(e) ? T.C.alert : T.heat(0.4 + 0.6 * sg), 'DBM');
  T.spark(g, bx + u * 7.5, by + u * 3.6, bw - u * 8.5, u * 2.1, e.hist.slice(-40), -100, -35, BU.tr(e) ? T.C.alert : T.C.acc);
  T.segBar(g, bx + u, by + u * 6.6, bw - u * 2, u * 0.4, sg, 16, false, BU.tr(e));
  T.hits.rect(bx, by, bw, bh, { k: 'card' });
};

// липкий порядок: соседи меняются, только если разница > 6 dB (как ORDER_HYST_DB на палке)
BU.sticky = () => ({
  ids: [], t: 0,
  get(list, now, frozen) {
    const byId = new Map(list.map(e => [e.id, e]));
    if (!this.ids.length || (!frozen && now - this.t > 1000)) {
      this.t = now;
      const o = this.ids.filter(id => byId.has(id));
      for (const e of [...list].sort((a, b) => b.rs - a.rs)) if (!o.includes(e.id)) { let k = 0; while (k < o.length && byId.get(o[k]).rs >= e.rs) k++; o.splice(k, 0, e.id); }
      for (let pass = 0; pass < o.length; pass++) { let sw = false; for (let i = 0; i + 1 < o.length; i++) if (byId.get(o[i + 1]).rs > byId.get(o[i]).rs + 6) { [o[i], o[i + 1]] = [o[i + 1], o[i]]; sw = true; } if (!sw) break; }
      this.ids = o;
    }
    return this.ids.filter(id => byId.has(id)).map(id => byId.get(id));
  },
});

// строки списка; ry плавно едет к своему месту
BU.rows = (g, x, y, w, h, u, rowH, list, sel, dt, cols = {}) => {
  const fs = rowH * 0.42;
  T.txt(g, 'T', x + u * 0.35, y + u * 0.8, u * 0.7, T.C.dim);
  T.txt(g, 'NAME', x + rowH * 1.05, y + u * 0.8, u * 0.7, T.C.dim);
  if (cols.more) { T.txt(g, 'MACS', x + w * 0.5, y + u * 0.8, u * 0.7, T.C.dim); T.txt(g, 'NEAR', x + w * 0.58, y + u * 0.8, u * 0.7, T.C.dim); }
  T.txt(g, 'DBM', x + w - u * 0.4, y + u * 0.8, u * 0.7, T.C.dim, 'm', 'right');
  const y0 = y + u * 1.3;
  g.save(); g.beginPath(); g.rect(x - 2, y0, w + 4, h - u * 1.3); g.clip();
  list.forEach((e, i) => {
    if (e._ry == null) e._ry = i;
    e._ry += (i - e._ry) * T.ease(dt, 0.25);
    const cy = y0 + e._ry * rowH;
    if (cy > y + h || cy + rowH < y0) return;
    const on = e.id === sel, tr = BU.tr(e), sg = T.pct(e.rs), gs = rowH * 0.62, col = tr ? T.C.alert : (on ? T.C.hot : T.C.acc);
    g.globalAlpha = T.clamp(e.h);
    if (on) { g.fillStyle = 'rgba(62,230,255,.08)'; g.fillRect(x, cy, w, rowH); T.brackets(g, x, cy, w, rowH, u * 0.45, T.C.acc, 1); }
    T.glyph(g, x + u * 0.3, cy + (rowH - gs) / 2, gs, BU.glyph(e), col);
    const nameW = (cols.more ? w * 0.5 : w * 0.62) - rowH * 1.05 - u * 0.3;
    g.font = `500 ${fs}px ${T.FC}`;
    T.txt(g, T.fit(g, BU.label(e).toUpperCase(), nameW), x + rowH * 1.05, cy + rowH * 0.45, fs, on ? T.C.hot : (tr ? T.C.alert : T.C.label), 'c');
    g.font = `${fs * 0.75}px ${T.FM}`;
    T.txt(g, T.fit(g, ((e.vend || '—') + (tr ? ' · TRACKER' : '')).toUpperCase(), nameW), x + rowH * 1.05, cy + rowH * 0.82, fs * 0.75, tr ? T.C.alert : T.C.dim);
    if (cols.more) { T.txt(g, String(e.macs || 1), x + w * 0.5, cy + rowH * 0.6, fs * 0.85, T.C.label); T.txt(g, T.ago(e.age || 0), x + w * 0.58, cy + rowH * 0.6, fs * 0.85, T.C.dim); }
    const bw = cols.more ? w * 0.16 : w * 0.22;
    T.segBar(g, x + w - u * 0.4 - u * 3.2 - bw, cy + rowH * 0.36, bw, rowH * 0.2, sg, 10, false, tr);
    T.txt(g, Math.round(e.rs), x + w - u * 0.4, cy + rowH * 0.66, rowH * 0.55, tr ? T.C.alert : T.heat(0.4 + 0.6 * sg), 't', 'right');
    T.hits.rect(x, cy, w, rowH, { k: 'dev', id: e.id });
    g.globalAlpha = 1;
  });
  g.restore();
};

T.views.push({
  id: 'ble', title: 'Bluetooth', want: 'ble', subs: ['Scope', 'City', 'Devices'], sub: 0,
  mount(main) {
    this.root = T.el('div', 'view'); const st = T.el('div', 'stage'); this.root.appendChild(st); this.cv = T.canvas(st); main.appendChild(this.root);
    this.cam = T.orbit(0.5, 0.98, 3.2); this.camT = { yaw: 0.5, D: 3.2 };
    this.drag = T.drag(this.cv.cv, this.cam, { minPitch: 0.5, maxPitch: 1.35 });
    this.drag.onTap = (x, y) => this.tap(x, y);
    this.sw = BU.sweeper(); this.dust = T.dust(220); this.sel = null; this.sticky = BU.sticky(); this.slots = new Map(); this.info = null; this.infoT = 0;
    this.onSub(this.sub);
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  onSub(i) {
    if (i === 1) { this.cam.pitch = 0.62; this.cam.D = 3.6; this.camT.D = 3.6; } else { this.cam.pitch = 0.98; this.camT.D = 3.2; }
    this.camT.yaw = this.cam.yaw;
  },
  tap(x, y) {
    const h = T.hits.at(x, y);
    if (!h) { if (this.sub !== 2) this.sel = null; return; }
    if (h.k === 'dev') { this.sel = (this.sel === h.id && this.sub !== 2) ? null : h.id; this.info = null; this.infoT = 0; }
    else if (h.k === 'back') this.sel = null;
  },
  // подробности выбранного с палки: адреса, Company ID, сервисы
  fetchInfo(now) {
    if (this.sel == null || now - this.infoT < 3000) return;
    this.infoT = now; const id = this.sel;
    fetch(`${BU.apiBase()}/api/bledev?id=${id}`, { cache: 'no-store' }).then(r => r.json()).then(d => { if (this.sel === id) this.info = d; }).catch(() => {});
  },
  frame(dt, t) {
    const { g, w, h } = this.cv; T.hits.clear();
    const u = T.unit(w, h), m = u * 1.6, y0 = 50, portrait = h > w * 1.05;
    this.sw.tick(dt, t); this.drag.tick(dt); this.dust.tick(dt);
    if (this.sel != null && !S.dev.has(this.sel)) this.sel = null;
    if (this.drag.down && this.drag.moved && this.sub !== 2) this.sel = null;
    T.backdrop(g, w, h, portrait ? 0.5 : 0.4, 0.55);
    const n = BU.list().length;
    if (this.sub === 0) this.scope(g, w, h, u, m, y0, portrait, dt, t);
    else if (this.sub === 1) this.city(g, w, h, u, m, y0, portrait, dt, t);
    else this.devices(g, w, h, u, m, y0, portrait, dt, t);
    if (!S.ble) T.emptyState(g, w, h, u, 'WAITING FOR TENGAN', 'connect the tablet to the TENGAN Wi-Fi');
    else if (!n) T.emptyState(g, w, h, u, 'NO DEVICES', S.ble.on ? 'listening…' : 'bluetooth is off');
    T.finish(g, w, h);
  },
  camStep(dt) {
    const c = this.cam;
    if (this.sel != null) { const e = S.dev.get(this.sel); if (e && this.sub === 0) this.camT.yaw = -Math.PI / 2 - BU.ang(e.id) - 0.32; this.camT.D = this.sub === 1 ? 3.0 : 2.85; }
    else { if (this.drag.idle()) this.camT.yaw = c.yaw + dt * (this.sub === 1 ? 0.05 : 0.07); else this.camT.yaw = c.yaw; this.camT.D = this.sub === 1 ? 3.6 : 3.2; }
    const dy = ((this.camT.yaw - c.yaw + Math.PI) % T.TAU + T.TAU) % T.TAU - Math.PI;
    if (!this.drag.down) c.yaw += dy * Math.min(1, dt * 2.4);
    c.D = T.lerp(c.D, this.camT.D, Math.min(1, dt * 2.4));
  },
  hud(g, w, h, u, m, y0, title, status) {
    T.header(g, m, y0, w - 2 * m, u, title, status, false);
    T.dots(g, m, y0 + u * 2.75, u, 3, this.sub);
  },
  // ---------- SCOPE ----------
  scope(g, w, h, u, m, y0, portrait, dt, t) {
    this.camStep(dt);
    const c = this.cam;
    c.frame(portrait ? w * 0.5 : w * 0.37, portrait ? y0 + (h - y0) * 0.27 : y0 + (h - y0) * 0.55, portrait ? w * 0.9 : Math.min((h - y0) * 1.42, w * 0.95));
    const selTop = BU.scene(g, c, this.sw, { u, t, sel: this.sel, labels: 3, hits: true });
    this.dust.draw(g, c);
    T.bloom(this.cv);
    const list = BU.list(), trk = list.some(BU.tr);
    this.hud(g, w, h, u, m, y0, 'Scope · depth', `${list.length} devices · cam ${Math.round(((c.yaw * 180 / Math.PI) % 360 + 360) % 360)}°${trk ? ' · tracker' : ''}`);
    const lst = this.sticky.get(list, performance.now(), false);
    if (!portrait) {
      const lw = w * 0.27, lx = w - m - lw, ly = y0 + u * 4.2, lh = Math.min(h - ly - u * 4, u * 2.25 * 12 + u * 2);
      T.panel(g, lx, ly, lw, lh, u, 'rgba(14,42,52,.25)', 'rgba(62,230,255,.25)');
      BU.rows(g, lx + u * 0.5, ly + u * 0.6, lw - u, lh - u * 1.2, u, u * 2.25, lst, this.sel, dt);
    } else {
      const ly = y0 + (h - y0) * 0.6, lh = h - ly - u * 3;
      T.panel(g, m, ly, w - 2 * m, lh, u, 'rgba(14,42,52,.25)', 'rgba(62,230,255,.25)');
      BU.rows(g, m + u * 0.5, ly + u * 0.6, w - 2 * m - u, lh - u * 1.2, u, u * 2.4, lst, this.sel, dt, { more: true });
    }
    if (selTop) BU.card(g, selTop.p, selTop.e, u, portrait ? w : w * 0.7, h, y0 + u * 4);
    T.footer(g, m, h - u * 2.4, w - 2 * m, u, 'drag: orbit  ·  tap pillar: focus', 'height = signal · ring = sweep hit');
  },
  // ---------- CITY ----------
  slot(id) {
    let s = this.slots.get(id); if (s != null) return s;
    const used = new Set(this.slots.values()), N = 9;
    // спираль от центра; место выбирается по хэшу, чтобы город был «тот же» после переподключения
    const sp = []; for (let r = 0; r <= 4; r++) for (let x = -r; x <= r; x++) for (let z = -r; z <= r; z++) if (Math.max(Math.abs(x), Math.abs(z)) === r) sp.push([x, z]);
    const start = T.hash(id) % 9;
    for (let k = 0; k < sp.length; k++) { const i = (k < 9 ? (start + k) % 9 : k); const key = sp[i][0] + ',' + sp[i][1]; if (!used.has(key)) { this.slots.set(id, key); return key; } }
    this.slots.set(id, '0,0'); return '0,0';
  },
  city(g, w, h, u, m, y0, portrait, dt, t) {
    this.camStep(dt);
    const c = this.cam;
    c.frame(w * 0.5, y0 + (h - y0) * (portrait ? 0.5 : 0.6), portrait ? w * 1.0 : Math.min((h - y0) * 1.5, w * 0.9));
    for (const id of [...this.slots.keys()]) if (!S.dev.has(id)) this.slots.delete(id);
    T.floorGrid(g, c, 1.6, 0.32 / 2);
    const list = BU.list(), rank = [...list].sort((a, b) => b.rs - a.rs);
    const SZ = { computer: 0.09, TV: 0.1, tv: 0.1, phone: 0.065, audio: 0.06, watch: 0.05, band: 0.045, tracker: 0.045, sensor: 0.05 };
    const towers = list.map(e => {
      const [gx, gz] = this.slot(e.id).split(',').map(Number), x = gx * 0.3, z = gz * 0.3;
      const sg = T.pct(e.rs); e._ch = e._ch == null ? 0 : e._ch + ((0.05 + 0.7 * Math.pow(sg, 1.3)) * e.h - e._ch) * T.ease(dt, 0.6);
      return { e, x, z, s: SZ[e.type] || 0.055, hh: e._ch, sg, d: c.p(x, 0, z)[3] };
    }).sort((a, b) => b.d - a.d);
    let selP = null; const labels = [];
    for (const tw of towers) {
      const { e, x, z, s, hh, sg } = tw, tr = BU.tr(e), fog = T.clamp(1.2 - (tw.d + 1) * 0.3, 0.3, 1) * T.clamp(e.h);
      const col = BU.rgb(e, 0.35 + 0.65 * sg);
      const P = [[-s, -s], [s, -s], [s, s], [-s, s]].map(([a, b]) => [c.p(x + a, 0, z + b), c.p(x + a, hh, z + b)]);
      // боковые грани: видимые (обход по часовой на экране), дальние раньше
      const faces = [0, 1, 2, 3].map(i => { const j = (i + 1) % 4; return { i, j, d: (P[i][0][3] + P[j][0][3]) / 2 }; }).sort((a, b) => b.d - a.d);
      for (const f of faces) {
        const a = P[f.i], b = P[f.j];
        const cross = (b[0][0] - a[0][0]) * (a[1][1] - a[0][1]) - (b[0][1] - a[0][1]) * (a[1][0] - a[0][0]);
        if (cross > 0) continue;
        const lit = 0.18 + 0.22 * Math.abs(Math.sin(f.i + 0.5));
        g.fillStyle = T.rgb(col, lit * 0.4 * fog); g.beginPath(); g.moveTo(a[0][0], a[0][1]); g.lineTo(b[0][0], b[0][1]); g.lineTo(b[1][0], b[1][1]); g.lineTo(a[1][0], a[1][1]); g.closePath(); g.fill();
        g.strokeStyle = T.rgb(col, 0.45 * fog); g.lineWidth = 1; g.stroke();
        // окна: полосы этажей
        g.strokeStyle = T.rgb(col, 0.18 * fog); g.beginPath();
        for (let y = 0.05; y < hh - 0.02; y += 0.05) { const k = y / Math.max(hh, 1e-3), ax = T.lerp(a[0][0], a[1][0], k), ay = T.lerp(a[0][1], a[1][1], k), bx = T.lerp(b[0][0], b[1][0], k), by = T.lerp(b[0][1], b[1][1], k); g.moveTo(ax, ay); g.lineTo(bx, by); }
        g.stroke();
      }
      // крыша светится; у трекеров — оранжевая
      g.fillStyle = T.rgb(tr ? T.ALERT : T.heatRGB(0.55 + 0.45 * sg), (0.45 + 0.35 * sg) * fog);
      g.beginPath(); P.forEach((q, k) => k ? g.lineTo(q[1][0], q[1][1]) : g.moveTo(q[1][0], q[1][1])); g.closePath(); g.fill();
      const top = c.p(x, hh, z);
      T.hits.circle(top[0], top[1], Math.max(u * 1.6, top[2] * s * 1.2), { k: 'dev', id: e.id });
      const bot = c.p(x, 0, z); T.hits.rect(Math.min(top[0], bot[0]) - top[2] * s, top[1], top[2] * s * 2, Math.max(4, bot[1] - top[1]), { k: 'dev', id: e.id });
      if (e.id === this.sel) selP = { p: top, e };
      else if (this.sel == null && rank.indexOf(e) < 5) labels.push([top, e]);
    }
    for (const [top, e] of labels) { const tr = BU.tr(e); g.strokeStyle = 'rgba(235,250,255,.35)'; g.lineWidth = 1; g.beginPath(); g.moveTo(top[0], top[1] - u * 0.2); g.lineTo(top[0], top[1] - u * 1.4); g.stroke(); g.font = `500 ${u * 0.72}px ${T.FC}`; const s = T.fit(g, BU.label(e).toUpperCase() + '  ' + Math.round(e.rs), u * 11), tw = T.tw(g, s, u * 0.72, 'c'); g.fillStyle = 'rgba(5,8,13,.7)'; g.fillRect(top[0] - tw / 2 - u * 0.3, top[1] - u * 2.4, tw + u * 0.6, u * 1.1); T.txt(g, s, top[0], top[1] - u * 1.55, u * 0.72, tr ? T.C.alert : T.C.hot, 'c', 'center'); }
    this.dust.draw(g, c);
    T.bloom(this.cv);
    this.hud(g, w, h, u, m, y0, 'City', `${list.length} towers · height = signal`);
    if (selP) BU.card(g, selP.p, selP.e, u, w, h, y0 + u * 4);
    T.footer(g, m, h - u * 2.4, w - 2 * m, u, 'drag: orbit  ·  tap tower: focus', 'roof orange = tracker');
  },
  // ---------- DEVICES ----------
  devices(g, w, h, u, m, y0, portrait, dt, t) {
    const now = performance.now(), list = BU.list(), lst = this.sticky.get(list, now, this.sel != null);
    this.hud(g, w, h, u, m, y0, 'Devices', `${S.ble ? S.ble.n : 0} devices · ${S.ble ? S.ble.m : 0} addresses`);
    const top = y0 + u * 4.2, bot = h - u * 3;
    let tx = m, tw = portrait ? w - 2 * m : w * 0.58 - m, th = portrait ? (bot - top) * 0.55 : bot - top;
    T.panel(g, tx, top, tw, th, u, 'rgba(14,42,52,.22)', T.C.line);
    BU.rows(g, tx + u * 0.6, top + u * 0.8, tw - u * 1.2, th - u * 1.6, u, u * 2.5, lst, this.sel, dt, { more: tw > u * 34 });
    const dx = portrait ? m : w * 0.58 + u, dy = portrait ? top + th + u : top, dw = portrait ? w - 2 * m : w - m - dx, dh = portrait ? bot - dy : bot - top;
    T.panel(g, dx, dy, dw, dh, u, 'rgba(14,42,52,.22)', T.C.line);
    const e = this.sel != null && S.dev.get(this.sel);
    if (e) { this.fetchInfo(now); this.detail(g, dx, dy, dw, dh, u, e); }
    else this.summary(g, dx, dy, dw, dh, u, list);
    T.footer(g, m, h - u * 2.4, w - 2 * m, u, e ? 'tap row: switch  ·  ‹ all: back' : 'tap row: details', 'order holds until 6 dB difference');
  },
  summary(g, x, y, w, h, u, list) {
    const p = u * 1.1; let cy = y + p;
    T.txt(g, '// AROUND', x + p, cy + u * 0.9, u * 1.05, T.C.acc, 't'); cy += u * 2.4;
    T.bigNum(g, list.length, x + p, cy + u * 3.4, u * 4.4, T.C.hot, 'DEVICES'); cy += u * 4.6;
    T.txt(g, 'COUNT · 3 MIN', x + p, cy + u * 0.7, u * 0.72, T.C.dim); cy += u * 1.1;
    const sh = Math.min(u * 5, h * 0.22);
    g.strokeStyle = T.C.line; g.lineWidth = 1; g.strokeRect(x + p, cy, w - 2 * p, sh);
    const hi = Math.max(5, ...S.bleHist); T.spark(g, x + p, cy, w - 2 * p, sh, S.bleHist, 0, hi * 1.15); cy += sh + u * 1.4;
    const cnt = {}; for (const e of list) cnt[e.type || 'unknown'] = (cnt[e.type || 'unknown'] || 0) + 1;
    const types = Object.entries(cnt).sort((a, b) => b[1] - a[1]), mx = Math.max(1, ...types.map(q => q[1]));
    for (const [tp, n] of types) {
      if (cy + u * 1.6 > y + h - p) break;
      const tr = tp === 'tracker';
      T.glyph(g, x + p, cy, u * 1.3, BU.G[tp] || tp[0].toUpperCase(), tr ? T.C.alert : T.C.acc);
      T.txt(g, tp.toUpperCase(), x + p + u * 1.9, cy + u * 1, u * 0.8, tr ? T.C.alert : T.C.label);
      T.segBar(g, x + w * 0.45, cy + u * 0.45, w * 0.4, u * 0.4, n / mx, 12, false, tr);
      T.txt(g, n, x + w - p, cy + u * 1.05, u * 1.1, T.C.hot, 't', 'right');
      cy += u * 1.75;
    }
  },
  detail(g, x, y, w, h, u, e) {
    const p = u * 1.1, tr = BU.tr(e), sg = T.pct(e.rs), d = this.info; let cy = y + p;
    T.txt(g, '// DEVICE', x + p, cy + u * 0.9, u * 1.05, T.C.acc, 't');
    T.txt(g, '‹ ALL', x + w - p, cy + u * 0.9, u * 0.9, T.C.label, 'm', 'right');
    T.hits.rect(x + w - p - u * 4.5, cy - u * 0.4, u * 5, u * 2, { k: 'back' });
    cy += u * 2.6;
    g.font = `500 ${u * 1.7}px ${T.FC}`; T.txt(g, T.fit(g, BU.label(e).toUpperCase(), w - 2 * p), x + p, cy + u * 1.3, u * 1.7, T.C.hot, 'c'); cy += u * 2.1;
    g.font = `${u * 0.85}px ${T.FM}`; T.txt(g, T.fit(g, ((e.vend || '—') + ' · ' + (e.type || '') + (tr ? ' · TRACKER' : '')).toUpperCase(), w - 2 * p), x + p, cy + u * 0.6, u * 0.85, tr ? T.C.alert : T.C.label); cy += u * 1.5;
    T.bigNum(g, Math.round(e.rs), x + p, cy + u * 3.6, u * 4.6, tr ? T.C.alert : T.heat(0.4 + 0.6 * sg), 'DBM');
    T.txt(g, '≈ ' + BU.dist(e.rs) + ' M', x + w - p, cy + u * 3.5, u * 1.2, T.C.label, 'm', 'right'); cy += u * 4.6;
    T.segBar(g, x + p, cy, w - 2 * p, u * 0.55, sg, 20, true, tr); cy += u * 1.6;
    T.txt(g, 'RSSI · 2 MIN', x + p, cy + u * 0.7, u * 0.72, T.C.dim); cy += u * 1.1;
    const sh = T.clamp(h * 0.18, u * 3, u * 6);
    g.strokeStyle = T.C.line; g.lineWidth = 1; g.strokeRect(x + p, cy, w - 2 * p, sh);
    T.spark(g, x + p, cy, w - 2 * p, sh, e.hist, -100, -35, tr ? T.C.alert : T.C.acc); cy += sh + u * 1.1;
    const kv = [['ADDRESSES', String(e.macs || 1)], ['NEAR FOR', T.ago(e.age || 0)]];
    if (d) {
      if (d.rmin != null) kv.push(['RANGE', `${d.rmin} … ${d.rmax} dBm`]);
      if (d.pk != null) kv.push(['PACKETS', String(d.pk)]);
      if (d.cid) kv.push(['COMPANY ID', d.cid]);
      for (const a of (d.addr || []).slice(0, 3)) kv.push([a.mac, (a.kind || '') + ' · ' + a.r]);
      for (const s of (d.uu || []).slice(0, 3)) kv.push(['SERVICE ' + s.u, s.o || '']);
    }
    for (const [k, v] of kv) {
      if (cy + u * 1.4 > y + h - p) break;
      T.txt(g, k, x + p, cy + u * 0.9, u * 0.8, T.C.dim);
      g.font = `500 ${u * 0.8}px ${T.FC}`; T.txt(g, T.fit(g, String(v).toUpperCase(), w * 0.55), x + w - p, cy + u * 0.9, u * 0.8, T.C.hot, 'c', 'right');
      cy += u * 1.4;
    }
  },
});
