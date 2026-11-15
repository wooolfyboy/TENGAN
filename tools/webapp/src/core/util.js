// ---- TENGAN HUD для планшета: токены прошивки (core/theme.cpp) и кисти ----
// Версия «B · обогащённая»: тот же HUD, что на палке, плюс глубина, свечение, частицы.
const T = window.T = { views: [] };
// цвета — ровно как на палке
T.C = { bg: '#05080d', acc: '#3ee6ff', hot: '#ebfaff', alert: '#ff7a1a', label: '#96afc0', dim: '#5a7080', line: '#203240', panel: '#0e2a34',
  steel: '#96afc0' };
T.FT = "Rajdhani,'Arial Narrow',sans-serif";                      // заголовки, крупные числа
T.FM = "'Share Tech Mono','JetBrains Mono',ui-monospace,monospace"; // текст HUD
T.FC = "'JetBrains Mono','Share Tech Mono',ui-monospace,monospace"; // где может быть кириллица
T.TAU = Math.PI * 2;
T.clamp = (v, a = 0, b = 1) => v < a ? a : v > b ? b : v;
T.lerp = (a, b, k) => a + (b - a) * k;
T.ease = (dt, tau) => 1 - Math.exp(-dt / tau);                    // доля пути за кадр при постоянной времени tau
// шкалы как на палке: HEAT (сигнал, жар) и COOL (фон, рельеф)
T.HEAT = [[0, [14, 30, 46]], [0.35, [40, 95, 130]], [0.75, [62, 230, 255]], [1, [235, 250, 255]]];
T.COOL = [[0, [5, 8, 13]], [0.25, [24, 40, 54]], [0.5, [95, 125, 145]], [0.72, [150, 185, 205]], [0.86, [62, 230, 255]], [1, [235, 250, 255]]];
T.ramp = (R, v) => {
  v = T.clamp(v);
  for (let i = 1; i < R.length; i++) if (v <= R[i][0]) {
    const a = R[i - 1], b = R[i], t = (v - a[0]) / (b[0] - a[0]);
    return [a[1][0] + (b[1][0] - a[1][0]) * t, a[1][1] + (b[1][1] - a[1][1]) * t, a[1][2] + (b[1][2] - a[1][2]) * t];
  }
  return R[R.length - 1][1];
};
T.rgb = (c, a = 1) => `rgba(${c[0] | 0},${c[1] | 0},${c[2] | 0},${a})`;
T.heatRGB = v => T.ramp(T.HEAT, v);
T.heat = (v, a = 1) => T.rgb(T.ramp(T.HEAT, v), a);
T.cool = (v, a = 1) => T.rgb(T.ramp(T.COOL, v), a);
T.ALERT = [255, 122, 26];
T.rgba = (hex, a) => { const n = parseInt(hex.slice(1), 16); return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${a})`; };
T.hash = s => { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) h = Math.imul(h ^ s.charCodeAt(i), 16777619); return h >>> 0; };
T.esc = s => String(s == null ? '' : s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
T.ago = s => s < 60 ? (s | 0) + 's' : s < 3600 ? Math.floor(s / 60) + 'm' : Math.floor(s / 3600) + 'h ' + Math.floor(s / 60) % 60 + 'm';
T.pct = r => T.clamp((r + 95) / 50, 0, 1);            // сила сигнала 0..1 (-95 .. -45 dBm), как на палке
T.hex8 = (h, signed) => {
  const n = (h || '').length >> 1, a = signed ? new Int8Array(n) : new Uint8Array(n);
  for (let i = 0; i < n; i++) { const v = parseInt(h.substr(i * 2, 2), 16); a[i] = signed && v > 127 ? v - 256 : v; }
  return a;
};
T.el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
T.reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
// ---- уровень качества: подбирается сам по реальным кадрам (app.js) ----
// 0 — родное разрешение; дальше по шагу дешевле. Красота важнее точности, но плавность важнее красоты.
T.QL = [
  { dpr: 3, bloom: 1, dust: 1, scan: 1 },
  { dpr: 2, bloom: 1, dust: 1, scan: 1 },
  { dpr: 1.5, bloom: 1, dust: 1, scan: 1 },
  { dpr: 1.5, bloom: 0, dust: 1, scan: 0 },
  { dpr: 1.25, bloom: 0, dust: 0, scan: 0 },
  { dpr: 1, bloom: 0, dust: 0, scan: 0 },
];
T.qLevel = 2;                                           // старт: 1.5× + свечение; дальше app.js подстроит
try { const v = localStorage.getItem('qLevel'); if (v != null && +v >= 0 && +v < T.QL.length) T.qLevel = +v; } catch (e) {}
T.Q = Object.assign({}, T.QL[T.qLevel]);
T.setQuality = lv => {
  lv = T.clamp(lv, 0, T.QL.length - 1) | 0;
  if (lv === T.qLevel) return;
  T.qLevel = lv; Object.assign(T.Q, T.QL[lv]);
  try { localStorage.setItem('qLevel', lv); } catch (e) {}
  for (const o of T._cvs || []) o.fit();
};

// ---- «спокойное» значение: дрожь меньше порога не показываем, большие изменения перетекают ----
// const s = T.steady(4, 0.7); s.set(v); каждый кадр s.tick(dt); s.v — что рисовать
T.steady = (db = 4, tau = 0.7) => ({
  v: null, target: null,
  set(x) { if (x == null || isNaN(x)) return; if (this.target == null) { this.v = this.target = x; return; } if (Math.abs(x - this.target) >= db) this.target = x; },
  tick(dt) { if (this.target != null) this.v += (this.target - this.v) * T.ease(dt, tau); return this.v; },
});

// ---- текст ----
T.txt = (g, s, x, y, px, col, f = 'm', al = 'left', bl = 'alphabetic') => {
  g.font = f === 't' ? `600 ${px}px ${T.FT}` : f === 'c' ? `500 ${px}px ${T.FC}` : `${px}px ${T.FM}`;
  g.fillStyle = col; g.textAlign = al; g.textBaseline = bl; g.fillText(s, x, y);
};
T.tw = (g, s, px, f = 'm') => { g.font = f === 't' ? `600 ${px}px ${T.FT}` : f === 'c' ? `500 ${px}px ${T.FC}` : `${px}px ${T.FM}`; return g.measureText(s).width; };
T.fit = (g, s, max) => { s = String(s); if (g.measureText(s).width <= max) return s; while (s.length > 1 && g.measureText(s + '…').width > max) s = s.slice(0, -1); return s + '…'; };
// совместимость со старыми видами
T.font = (px, bold) => `${bold ? '600 ' : ''}${px}px ${bold ? T.FT : T.FM}`;
T.text = (g, s, x, y, px, col, align = 'left', base = 'alphabetic', bold) => T.txt(g, s, x, y, px, col, bold ? 't' : 'm', align, base);

// ---- единица разметки: 16 px на планшете 1280×800, пропорционально на других ----
T.unit = (W, H) => Math.min(W / 80, H / 50);

// ---- кисти HUD (как ui.cpp на палке) ----
// шапка «// TITLE» + статус справа + угасающая линия
T.header = (g, x, y, w, u, title, status, alert) => {
  T.txt(g, '// ' + String(title).toUpperCase(), x, y + u * 1.55, u * 1.8, T.C.acc, 't');
  if (status) {
    g.font = `${u * 0.92}px ${T.FM}`;
    const room = w - T.tw(g, '// ' + String(title).toUpperCase(), u * 1.8, 't') - u * 2;
    g.font = `${u * 0.92}px ${T.FM}`;
    T.txt(g, T.fit(g, String(status).toUpperCase(), Math.max(u * 4, room)), x + w, y + u * 1.45, u * 0.92, alert ? T.C.alert : T.C.label, 'm', 'right');
  }
  const gr = g.createLinearGradient(x, 0, x + w, 0);
  gr.addColorStop(0, 'rgba(62,230,255,1)'); gr.addColorStop(1, 'rgba(62,230,255,.15)');
  g.fillStyle = gr; g.fillRect(x, y + u * 2.1, w, Math.max(1, u * 0.08));
};
T.dots = (g, x, y, u, n, i) => { for (let k = 0; k < n; k++) { const on = k === i; g.fillStyle = on ? T.C.acc : T.C.line; g.fillRect(x + k * u * 0.85, y, on ? u * 0.55 : u * 0.3, u * 0.3); } };
T.footer = (g, x, y, w, u, l, r) => {
  g.fillStyle = T.C.line; g.fillRect(x, y, w, 1);
  if (l) T.txt(g, String(l).toUpperCase(), x, y + u * 1.25, u * 0.8, T.C.dim);
  if (r) T.txt(g, String(r).toUpperCase(), x + w, y + u * 1.25, u * 0.8, T.C.dim, 'm', 'right');
};
T.brackets = (g, x, y, w, h, L, col = T.C.acc, lw = 1.5) => {
  g.strokeStyle = col; g.lineWidth = lw; g.beginPath();
  g.moveTo(x, y + L); g.lineTo(x, y); g.lineTo(x + L, y);
  g.moveTo(x + w - L, y); g.lineTo(x + w, y); g.lineTo(x + w, y + L);
  g.moveTo(x + w, y + h - L); g.lineTo(x + w, y + h); g.lineTo(x + w - L, y + h);
  g.moveTo(x + L, y + h); g.lineTo(x, y + h); g.lineTo(x, y + h - L);
  g.stroke();
};
T.corners = (g, x, y, w, h, L = 16, col = T.C.acc) => T.brackets(g, x, y, w, h, L, col, 2);
T.cutPath = (g, x, y, w, h, c) => { g.beginPath(); g.moveTo(x + c, y); g.lineTo(x + w, y); g.lineTo(x + w, y + h - c); g.lineTo(x + w - c, y + h); g.lineTo(x, y + h); g.lineTo(x, y + c); g.closePath(); };
// панель со срезанными углами; по умолчанию «стекло» поверх 3D-сцены
T.panel = (g, x, y, w, h, u, fill = 'rgba(14,42,52,.28)', stroke = 'rgba(62,230,255,.22)') => {
  T.cutPath(g, x, y, w, h, u * 0.8); g.fillStyle = fill; g.fill(); g.strokeStyle = stroke; g.lineWidth = 1; g.stroke();
};
// сегментная полоса с отражением
T.segBar = (g, x, y, w, h, v, n, refl = true, alert = false) => {
  const gap = Math.max(1, w / n * 0.24), sw = (w - gap * (n - 1)) / n, lit = Math.round(T.clamp(v) * n);
  for (let i = 0; i < n; i++) {
    g.fillStyle = i < lit ? (alert ? T.C.alert : T.heat(0.3 + 0.7 * i / Math.max(1, n - 1))) : T.C.line;
    g.fillRect(x + i * (sw + gap), y, sw, h);
    if (refl && i < lit) { g.globalAlpha = 0.15; g.fillRect(x + i * (sw + gap), y + h * 1.3, sw, h * 0.45); g.globalAlpha = 1; }
  }
};
// буква в рамке (тип устройства)
T.glyph = (g, x, y, s, ch, col) => { g.strokeStyle = col; g.lineWidth = 1; g.strokeRect(x + 0.5, y + 0.5, s - 1, s - 1); T.txt(g, ch, x + s / 2, y + s * 0.56, s * 0.66, col, 't', 'center', 'middle'); };
T.diamond = (g, x, y, r, col, fill) => {
  g.beginPath(); g.moveTo(x, y - r); g.lineTo(x + r, y); g.lineTo(x, y + r); g.lineTo(x - r, y); g.closePath();
  if (fill) { g.fillStyle = col; g.fill(); } else { g.strokeStyle = col; g.lineWidth = 1.5; g.stroke(); }
};
// график-линия с заливкой
T.spark = (g, x, y, w, h, arr, lo, hi, col = T.C.acc) => {
  if (!arr || arr.length < 2) return;
  g.beginPath();
  arr.forEach((v, i) => { const px = x + i / (arr.length - 1) * w, py = y + h - T.clamp((v - lo) / (hi - lo)) * h; i ? g.lineTo(px, py) : g.moveTo(px, py); });
  g.strokeStyle = col; g.lineWidth = 1.5; g.stroke();
  g.lineTo(x + w, y + h); g.lineTo(x, y + h); g.closePath();
  const gr = g.createLinearGradient(0, y, 0, y + h); gr.addColorStop(0, 'rgba(62,230,255,.22)'); gr.addColorStop(1, 'rgba(62,230,255,0)');
  g.fillStyle = gr; g.fill();
};
// крупное число + подпись единиц
T.bigNum = (g, v, x, y, px, col, unit, al = 'left') => {
  const s = String(v), w = T.tw(g, s, px, 't');
  const x0 = al === 'right' ? x - w - (unit ? T.tw(g, unit, px * 0.22) + px * 0.12 : 0) : x;
  T.txt(g, s, x0, y, px, col, 't');
  if (unit) T.txt(g, unit, x0 + w + px * 0.1, y - px * 0.04, px * 0.22, T.C.dim);
};
// печать 天眼 из прошивки (core/seal.h) — кэшируется по размеру и цвету
T.sealImg = (() => {
  const cache = {};
  return (h, col = T.C.acc) => {
    const k = (h | 0) + col; if (cache[k]) return cache[k];
    const D = T.DATA && T.DATA.seal; if (!D) return null;
    const src = document.createElement('canvas'); src.width = D.w; src.height = D.h;
    const sx = src.getContext('2d'), im = sx.createImageData(D.w, D.h), n = parseInt(col.slice(1), 16);
    for (let y = 0; y < D.h; y++) for (let x = 0; x < D.w; x++) {
      const byte = parseInt(D.bits.substr((y * (D.w >> 3) + (x >> 3)) * 2, 2), 16);
      if ((byte >> (7 - (x & 7))) & 1) { const o = (y * D.w + x) * 4; im.data[o] = n >> 16; im.data[o + 1] = (n >> 8) & 255; im.data[o + 2] = n & 255; im.data[o + 3] = 255; }
    }
    sx.putImageData(im, 0, 0);
    const c = document.createElement('canvas'), s = Math.max(1, Math.round(h / D.h));
    c.width = D.w * s; c.height = D.h * s;
    const cx = c.getContext('2d'); cx.imageSmoothingEnabled = false; cx.drawImage(src, 0, 0, c.width, c.height);
    return (cache[k] = c);
  };
})();

// ---- 3D: камера на орбите ----
// const cam = T.orbit(); cam.frame(cx, cy, F); const p = cam.p(x, y, z) -> [sx, sy, scale, depth]
T.orbit = (yaw = 0.6, pitch = 0.95, D = 3.2) => ({
  yaw, pitch, D, cx: 0, cy: 0, F: 1, cs: 1, sn: 0, cp: 1, sp: 0,
  frame(cx, cy, F) { this.cx = cx; this.cy = cy; this.F = F; this.cs = Math.cos(this.yaw); this.sn = Math.sin(this.yaw); this.cp = Math.cos(this.pitch); this.sp = Math.sin(this.pitch); },
  p(x, y, z) {
    const x1 = x * this.cs - z * this.sn, z1 = x * this.sn + z * this.cs;
    const y2 = y * this.cp + z1 * this.sp, z2 = -y * this.sp + z1 * this.cp, s = this.F / (this.D + z2);
    return [this.cx + x1 * s, this.cy - y2 * s, s, z2];
  },
});
// старая форма (совместимость)
T.cam = (yaw, pitch, dist, F, cx, cy) => { const o = T.orbit(yaw, pitch, dist); o.frame(cx, cy, F); return (x, y, z) => { const p = o.p(x, y, z); return [p[0], p[1], dist + p[3]]; }; };
// пальцем: тянуть — облёт, отпустить — инерция. Возвращает {down, tap(cb)}
T.drag = (cv, cam, opt = {}) => {
  const st = { down: null, vy: 0, last: 0, onTap: null, moved: false };
  const minP = opt.minPitch ?? 0.45, maxP = opt.maxPitch ?? 1.4;
  cv.addEventListener('pointerdown', e => { st.down = { x: e.offsetX, y: e.offsetY, yaw: cam.yaw, pitch: cam.pitch, t: performance.now() }; st.moved = false; cv.setPointerCapture(e.pointerId); });
  cv.addEventListener('pointermove', e => {
    if (!st.down) return;
    const dx = e.offsetX - st.down.x, dy = e.offsetY - st.down.y;
    if (Math.abs(dx) + Math.abs(dy) > 7) st.moved = true;
    if (!st.moved) return;
    const ny = st.down.yaw - dx * 0.006;
    st.vy = (ny - cam.yaw) * 60; cam.yaw = ny;
    cam.pitch = T.clamp(st.down.pitch - dy * 0.004, minP, maxP);
    st.last = performance.now();
  });
  const up = e => { if (!st.down) return; const moved = st.moved; st.down = null; if (!moved && st.onTap && e.type === 'pointerup') st.onTap(e.offsetX, e.offsetY); };
  cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
  st.tick = dt => { if (!st.down && Math.abs(st.vy) > 0.01) { cam.yaw += st.vy * dt; st.vy *= Math.exp(-dt * 3); } };
  st.idle = () => !st.down && performance.now() - st.last > 4000;   // давно не трогали — можно облетать самим
  return st;
};

// ---- атмосфера: свечение, пыль, сканлайны, виньетка ----
// свечение (bloom): кадр уменьшается цепочкой ½ → ¼ → ⅛ (каждый шаг честно усредняет,
// поэтому тонкие линии не мерцают) и ложится поверх «светом». Только drawImage — дёшево на GPU,
// без ctx.filter (на Android он считается процессором и роняет кадры).
T.bloom = (o, strength = 0.85) => {
  if (!T.Q.bloom) return;
  const cv = o.cv, g = o.g;
  if (!o.bl) o.bl = [0, 1, 2].map(() => { const c = document.createElement('canvas'); return { c, g: c.getContext('2d') }; });
  let src = cv;
  for (let i = 0; i < 3; i++) {
    const b = o.bl[i], w = Math.max(1, cv.width >> (i + 1)), h = Math.max(1, cv.height >> (i + 1));
    if (b.c.width !== w || b.c.height !== h) { b.c.width = w; b.c.height = h; }
    b.g.globalCompositeOperation = 'copy'; b.g.imageSmoothingEnabled = true; b.g.imageSmoothingQuality = 'low';
    b.g.drawImage(src, 0, 0, w, h);
    src = b.c;
  }
  g.save(); g.setTransform(1, 0, 0, 1, 0, 0); g.globalCompositeOperation = 'lighter'; g.imageSmoothingEnabled = true;
  g.globalAlpha = strength * 0.45; g.drawImage(o.bl[1].c, 0, 0, cv.width, cv.height);
  g.globalAlpha = strength * 0.6; g.drawImage(o.bl[2].c, 0, 0, cv.width, cv.height);
  g.restore();
};
T.backdrop = (g, W, H, fx = 0.45, fy = 0.55) => {
  const r = g.createRadialGradient(W * fx, H * fy, 0, W * fx, H * fy, Math.max(W, H) * 0.75);
  r.addColorStop(0, '#0a1722'); r.addColorStop(1, T.C.bg); g.fillStyle = r; g.fillRect(0, 0, W, H);
};
T.finish = (g, W, H) => {                               // сканлайны + виньетка: последним слоем
  if (!T.Q.scan) return;
  if (!T._scan) { const c = document.createElement('canvas'); c.width = 4; c.height = 4; const x = c.getContext('2d'); x.fillStyle = 'rgba(0,0,0,.16)'; x.fillRect(0, 0, 4, 1); T._scanC = c; }
  if (!T._scan || T._scanG !== g) { T._scan = g.createPattern(T._scanC, 'repeat'); T._scanG = g; }
  g.fillStyle = T._scan; g.fillRect(0, 0, W, H);
  const v = g.createRadialGradient(W / 2, H / 2, Math.min(W, H) * 0.45, W / 2, H / 2, Math.max(W, H) * 0.75);
  v.addColorStop(0, 'rgba(5,8,13,0)'); v.addColorStop(1, 'rgba(5,8,13,.7)'); g.fillStyle = v; g.fillRect(0, 0, W, H);
};
// пыль в объёме: T.dust(n) -> {tick(dt), draw(g, cam)}
T.dust = (n = 220, ext = 1.7, top = 1.1) => {
  if (T.reduce) n = 0;
  const P = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) { P[i * 3] = (Math.random() - 0.5) * ext * 2; P[i * 3 + 1] = Math.random() * top; P[i * 3 + 2] = (Math.random() - 0.5) * ext * 2; }
  return {
    tick(dt) { for (let i = 0; i < n; i++) { P[i * 3 + 1] += dt * 0.035; if (P[i * 3 + 1] > top) P[i * 3 + 1] = 0; } },
    draw(g, cam) {
      if (!T.Q.dust) return;
      const b = [[], [], []];
      for (let i = 0; i < n; i++) { const p = cam.p(P[i * 3], P[i * 3 + 1], P[i * 3 + 2]); if (cam.D + p[3] < 0.3) continue; b[p[3] < -0.4 ? 2 : p[3] < 0.6 ? 1 : 0].push(p); }
      [0.18, 0.32, 0.55].forEach((a, k) => { g.fillStyle = `rgba(62,230,255,${a})`; g.beginPath(); for (const p of b[k]) { const s = Math.max(0.8, p[2] * 0.0045); g.rect(p[0] - s / 2, p[1] - s / 2, s, s); } g.fill(); });
    },
  };
};
// пол-сетка с туманом (3D-сцены): клетки step, до ext
T.floorGrid = (g, cam, ext = 1.7, step = 0.2, y = 0) => {
  g.lineWidth = 1;
  const n = Math.round(ext / step);
  for (let k = -n; k <= n; k++) {
    const c = k * step, a = 0.48 * (1 - Math.abs(c) / (ext + 0.05));
    g.strokeStyle = `rgba(32,50,64,${a})`;
    let p = cam.p(c, y, -ext), q = cam.p(c, y, ext); g.beginPath(); g.moveTo(p[0], p[1]); g.lineTo(q[0], q[1]); g.stroke();
    p = cam.p(-ext, y, c); q = cam.p(ext, y, c); g.beginPath(); g.moveTo(p[0], p[1]); g.lineTo(q[0], q[1]); g.stroke();
  }
};
// попадания пальцем: виды кладут сюда зоны каждый кадр
T.hits = {
  list: [],
  clear() { this.list.length = 0; },
  circle(x, y, r, data) { this.list.push({ x, y, r, data }); },
  rect(x, y, w, h, data) { this.list.push({ x, y, w, h, data }); },
  at(x, y) { for (let i = this.list.length - 1; i >= 0; i--) { const h = this.list[i]; if (h.r != null) { if ((x - h.x) ** 2 + (y - h.y) ** 2 <= h.r * h.r) return h.data; } else if (x >= h.x && x <= h.x + h.w && y >= h.y && y <= h.y + h.h) return h.data; } return null; },
};
// пусто / нет связи — одинаково во всех разделах
T.emptyState = (g, W, H, u, title, line, alert) => {
  T.txt(g, title, W / 2, H / 2 - u * 0.4, u * 1.9, alert ? T.C.alert : T.C.hot, 't', 'center');
  if (line) T.txt(g, String(line).toUpperCase(), W / 2, H / 2 + u * 1.3, u * 0.85, T.C.label, 'm', 'center');
};
/*@DATA*/
