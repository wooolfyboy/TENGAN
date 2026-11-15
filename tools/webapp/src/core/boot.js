// ---- заставка при открытии: та же, что на палке (core/boot.cpp), но в 2.8K ----
// прицел и уголки разлетаются, кольца расходятся, луч с хвостом засекает цели,
// название «расшифровывается», ставится печать 天眼, строка развёртки стирает экран.
T.boot = () => {
  const box = document.getElementById('boot'), cv = document.getElementById('bootCv');
  let skip = false;
  try { skip = sessionStorage.getItem('booted') === '1'; sessionStorage.setItem('booted', '1'); } catch (e) {}
  if (skip || T.reduce) { box.remove(); return; }
  const g = cv.getContext('2d'), t0 = performance.now(), MS = 2700;
  const seg = (t, a, b) => t <= a ? 0 : t >= b ? 1 : (t - a) / (b - a), eo = x => 1 - Math.pow(1 - T.clamp(x), 3);
  const BL = [[35, 72], [80, 38], [140, 88], [200, 55], [250, 80], [310, 30], [335, 64]];
  const POOL = 'ABCDEFGHKLMNPRSTVXZ0123456789', TITLE = 'TENGAN', SUB = '// ENVIRONMENT ANALYZER';
  const hash = x => { x ^= x >>> 16; x = Math.imul(x, 0x7feb352d); x ^= x >>> 15; x = Math.imul(x, 0x846ca68b); x ^= x >>> 16; return x >>> 0; };
  const end = () => { box.classList.add('done'); setTimeout(() => box.remove(), 400); };
  box.addEventListener('pointerdown', end);
  const frame = now => {
    if (!box.isConnected) return;
    const t = now - t0, d = Math.min(devicePixelRatio || 1, 3), W = innerWidth, H = innerHeight;
    if (cv.width !== Math.round(W * d)) { cv.width = Math.round(W * d); cv.height = Math.round(H * d); }
    g.setTransform(d, 0, 0, d, 0, 0);
    const cx = W / 2, cy = H / 2, R = Math.min(W, H) * 0.36, u = T.unit(W, H);
    g.fillStyle = T.C.bg; g.fillRect(0, 0, W, H);
    // прицел
    const gr = eo(seg(t, 0, 450));
    g.fillStyle = T.C.dim; g.fillRect(cx - gr * W / 2, cy, gr * W, 1); g.fillRect(cx, cy - gr * H / 2, 1, gr * H);
    // кольца
    for (let i = 0; i < 4; i++) { const e = eo(seg(t, 80 + i * 110, 420 + i * 110)); if (e > 0) { g.strokeStyle = i === 3 ? T.C.acc : T.C.dim; g.lineWidth = i === 3 ? 1.5 : 1; g.beginPath(); g.arc(cx, cy, R * (i + 1) / 4 * e, 0, T.TAU); g.stroke(); } }
    const tk = seg(t, 500, 800);
    g.lineWidth = 1;
    for (let a = 0; a < 360 * tk; a += 10) { const r = a * Math.PI / 180, l = a % 30 ? 0.025 : 0.05; g.strokeStyle = a % 30 ? T.C.dim : T.C.acc; g.beginPath(); g.moveTo(cx + Math.cos(r) * R, cy + Math.sin(r) * R); g.lineTo(cx + Math.cos(r) * R * (1 + l), cy + Math.sin(r) * R * (1 + l)); g.stroke(); }
    // луч с хвостом и цели
    if (t > 300 && t < 2400) {
      const sw = (t - 300) * 0.48 * Math.PI / 180, fade = 1 - seg(t, 2050, 2400);
      if (g.createConicGradient) { const c = g.createConicGradient(sw - 1.3, cx, cy), f = 1.3 / T.TAU; c.addColorStop(0, 'rgba(40,95,130,0)'); c.addColorStop(f * 0.985, `rgba(62,230,255,${0.4 * fade})`); c.addColorStop(f, 'rgba(62,230,255,0)'); c.addColorStop(1, 'rgba(62,230,255,0)'); g.fillStyle = c; g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, R, sw - 1.3, sw); g.closePath(); g.fill(); }
      g.strokeStyle = `rgba(235,250,255,${fade})`; g.lineWidth = 2; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + Math.cos(sw) * R, cy + Math.sin(sw) * R); g.stroke();
      const swd = (t - 300) * 0.48;
      for (const [deg, rr] of BL) {
        if (swd < deg) continue;
        const since = ((swd - deg) % 360 + 360) % 360, v = T.clamp(1 - since / 300) * fade; if (v < 0.02) continue;
        const a = deg * Math.PI / 180, x = cx + Math.cos(a) * R * rr / 100, y = cy + Math.sin(a) * R * rr / 100;
        T.diamond(g, x, y, u * 0.5, T.heat(0.3 + 0.7 * v), true);
        if (since < 40) { g.strokeStyle = T.heat(v, v); g.beginPath(); g.arc(x, y, u * (0.7 + since / 12), 0, T.TAU); g.stroke(); }
      }
    }
    // уголки к краям
    const c = eo(seg(t, 0, 500)), L = u * 2;
    for (const sx of [-1, 1]) for (const sy of [-1, 1]) {
      const x = cx + sx * (W / 2 - u) * c, y = cy + sy * (H / 2 - u) * c;
      g.fillStyle = T.C.acc; g.fillRect(sx < 0 ? x : x - L, y - (sy < 0 ? 0 : 1), L, 1.5); g.fillRect(x - (sx < 0 ? 0 : 1), sy < 0 ? y : y - L, 1.5, L);
    }
    // плашка, название, печать
    const bar = eo(seg(t, 1000, 1280)), bw = W * 0.6 * bar, bh = u * 9;
    if (bar > 0) { g.fillStyle = T.C.bg; g.fillRect(cx - bw / 2, cy - bh / 2, bw, bh); g.fillStyle = T.C.acc; g.fillRect(cx - bw / 2, cy - bh / 2, bw, 1.5); g.fillRect(cx - bw / 2, cy + bh / 2, bw, 1.5); }
    const px = u * 4.6, tw = T.tw(g, TITLE, px, 't'), sealH = px * 1.15, seal = T.sealImg(sealH, t < 1700 ? T.C.hot : T.C.acc), sw2 = seal ? seal.width * sealH / seal.height : 0;
    const bx = cx - (tw + u * 2 + sw2) / 2;
    if (t >= 1120) {
      let x = bx;
      for (let i = 0; i < TITLE.length; i++) {
        const ch = TITLE[i], w = T.tw(g, ch, px, 't'), lock = 1150 + i * 55;
        if (t >= 1120 + i * 25) T.txt(g, t < lock ? POOL[hash(i * 977 + (t / 45 | 0)) % POOL.length] : ch, x, cy + px * 0.3, px, t < lock + 90 ? T.C.acc : T.C.hot, 't');
        x += w;
      }
    }
    if (t >= 1640 && seal) {
      const k = seg(t, 1640, 1820), sx = bx + tw + u * 2, sy = cy - sealH / 2 - px * 0.06;
      g.drawImage(seal, sx, sy, sw2, sealH);
      if (k < 1) { const gg = u * (0.3 + k * 2); g.strokeStyle = `rgba(235,250,255,${0.6 * (1 - k)})`; g.strokeRect(sx - gg, sy - gg, sw2 + 2 * gg, sealH + 2 * gg); }
    }
    if (t >= 1700) { const k = Math.min(SUB.length, (t - 1700) / 14 | 0); T.txt(g, SUB.slice(0, k) + ((t / 120 | 0) & 1 && k < SUB.length ? '_' : ''), cx, cy + bh / 2 - u * 1.2, u * 1.05, T.C.label, 'm', 'center'); }
    // развёртка стирает
    const wp = seg(t, 2350, MS);
    if (wp > 0) { const y = eo(wp) * (H + 6); g.fillStyle = T.C.bg; g.fillRect(0, 0, W, Math.min(y, H)); g.fillStyle = T.C.hot; g.fillRect(0, y, W, 2); g.fillStyle = T.C.acc; g.fillRect(0, y - 2, W, 1); g.fillRect(0, y + 3, W, 1); }
    if (t > MS) { end(); return; }
    requestAnimationFrame(frame);
  };
  (document.fonts ? document.fonts.ready : Promise.resolve()).then(() => requestAnimationFrame(frame));
};
