// ---- темы: DENKO 電光 (электрический свет, HUD) и SUMI 墨 (тушь на рисовой бумаге) ----
// TENGAN — имя прошивки, не темы.
// Тему выбирают на палке («Основные настройки» → Theme), она приходит в событии st.
// Для проверки: ?theme=sumi в адресе. Последняя тема помнится — запуск сразу в ней.
//
// Как SUMI перекрашивает всё сразу: холсту подменены сеттеры fillStyle/strokeStyle/
// shadowColor и addColorStop у градиентов. Цвета палитры DENKO (бирюза, белый, фон,
// панели, линии) на лету становятся тушью, бумагой и киноварью; «свет» (режим lighter)
// становится «тушью» (multiply). Отдельные сцены (радар, город, рельеф, запуск) в SUMI
// рисуются по-своему — кистью (T.ink).
(() => {
  const DENKO = { C: Object.assign({}, T.C, { seal: T.C.acc }), HEAT: T.HEAT, COOL: T.COOL, ALERT: T.ALERT, FT: T.FT };
  const SUMI = {
    C: { bg: '#ece4d4', acc: '#1b1814', hot: '#1b1814', alert: '#c23a2b', label: '#4a433b', dim: '#8a8174', line: '#c9bfae', panel: '#e2d4c2',
      steel: '#4a433b', seal: '#c23a2b' },
    HEAT: [[0, [222, 212, 194]], [0.35, [160, 150, 135]], [0.75, [74, 67, 59]], [1, [27, 24, 20]]],
    COOL: [[0, [236, 228, 212]], [0.4, [201, 191, 174]], [0.75, [110, 101, 90]], [1, [27, 24, 20]]],
    ALERT: [194, 58, 43],
    FT: "'Noto Serif','Liberation Serif',Georgia,serif",
  };
  T.THEMES = { denko: DENKO, sumi: SUMI };
  T.theme = 'denko';
  T.KANJI = { ov: '観', ble: '藍', wifi: '網', sub: '波', snd: '音', pc: '機', gal: '画' };

  // ---- перекраска палитры DENKO -> SUMI ----
  const INK = [27, 24, 20], PAPER = [236, 228, 212];
  const MAP = {
    '62,230,255': INK, '235,250,255': INK, '255,255,255': INK, '234,252,255': INK,
    '5,8,13': PAPER, '0,0,0': PAPER, '7,9,13': PAPER, '10,23,34': [230, 221, 204],
    '14,42,52': [226, 212, 194], '32,50,64': [201, 191, 174], '90,112,128': [138, 129, 116], '150,175,192': [74, 67, 59],
    '255,122,26': [194, 58, 43], '40,95,130': [160, 150, 135], '14,30,46': [222, 212, 194], '18,58,72': [214, 202, 182],
    '8,24,32': [228, 218, 200], '8,16,22': [230, 221, 204], '5,9,14': [232, 224, 208], '5,10,16': [232, 224, 208], '10,20,28': [228, 219, 202],
  };
  const parse = s => {
    let m;
    if (s.charCodeAt(0) === 35) {                          // #rgb / #rrggbb
      let h = s.slice(1); if (h.length === 3) h = h.replace(/./g, c => c + c);
      if (h.length !== 6) return null; const n = parseInt(h, 16); return [n >> 16, (n >> 8) & 255, n & 255, 1];
    }
    if ((m = /^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$/.exec(s))) return [+m[1] | 0, +m[2] | 0, +m[3] | 0, m[4] == null ? 1 : +m[4]];
    return null;
  };
  const cache = new Map();
  const remap = s => {
    let r = cache.get(s); if (r !== undefined) return r;
    const c = parse(s); r = s;
    if (c) {
      const k = c[0] + ',' + c[1] + ',' + c[2]; let t = MAP[k];
      if (!t) {
        const L = (77 * c[0] + 150 * c[1] + 29 * c[2]) >> 8;
        if (Math.max(c[0], c[1], c[2]) <= 60 && c[2] >= c[0]) t = [PAPER[0] - L * 0.4, PAPER[1] - L * 0.4, PAPER[2] - L * 0.4];        // тёмные подложки -> бумага
        else if (c[2] - c[0] > 60) t = T.ramp(SUMI.HEAT, L / 255);
        else if (c[2] - c[0] > 20 && L < 130) { const k = L / 130; t = [236 - 50 * k, 228 - 48 * k, 212 - 44 * k]; }                   // ночное небо, тёмная сталь -> размывка                                                     // бирюза/сталь -> тушь
        else if (c[0] > 200 && c[1] < 170 && c[2] < 90) t = SUMI.ALERT;                                                // оранжевый -> киноварь
      }
      if (t) r = `rgba(${t[0] | 0},${t[1] | 0},${t[2] | 0},${c[3]})`;
    }
    if (cache.size > 6000) cache.clear();
    cache.set(s, r); return r;
  };
  T.remap = remap;
  // перехват цветов холста ставится только в SUMI; в DENKO — родные сеттеры браузера, без накладных расходов
  const P = CanvasRenderingContext2D.prototype, G = CanvasGradient.prototype;
  const orig = {}; for (const k of ['fillStyle', 'strokeStyle', 'shadowColor', 'globalCompositeOperation']) orig[k] = Object.getOwnPropertyDescriptor(P, k);
  const origAdd = G.addColorStop;
  let hooked = false;
  T.hookCanvas = on => {
    if (on === hooked) return; hooked = on;
    if (!on) { for (const k in orig) Object.defineProperty(P, k, orig[k]); G.addColorStop = origAdd; return; }
    for (const prop of ['fillStyle', 'strokeStyle', 'shadowColor']) {
      const d = orig[prop];
      Object.defineProperty(P, prop, { configurable: true, get() { return d.get.call(this); }, set(v) { d.set.call(this, typeof v === 'string' && T.theme !== 'raw' ? remap(v) : v); } });
    }
    { const d = orig.globalCompositeOperation;
      Object.defineProperty(P, 'globalCompositeOperation', { configurable: true, get() { return d.get.call(this); },
        set(v) { d.set.call(this, (v === 'lighter' || v === 'screen') && T.theme !== 'raw' && !this.canvas.__raw ? 'multiply' : v); } }); }
    G.addColorStop = function (o, c) { return origAdd.call(this, o, typeof c === 'string' && T.theme !== 'raw' ? remap(c) : c); };
  };

  // ---- бумага (одна текстура на всё) ----
  const rng = seed => () => { seed |= 0; seed = seed + 0x6D2B79F5 | 0; let t = Math.imul(seed ^ seed >>> 15, 1 | seed); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
  let paperC = null;
  const paperTex = () => {
    if (paperC) return paperC;
    const c = document.createElement('canvas'); c.width = c.height = 512; c.__raw = 1;
    const g = c.getContext('2d'), r = rng(7);
    const fill = s => { const t = T.theme; T.theme = 'raw'; g.fillStyle = s; T.theme = t; };
    fill('#ece4d4'); g.fillRect(0, 0, 512, 512);
    for (let i = 0; i < 60; i++) {
      const x0 = r() * 512, y0 = r() * 512, rr = 40 + r() * 120, light = r() < 0.5, a = 0.04 + r() * 0.05;
      for (const ox of [-512, 0, 512]) for (const oy of [-512, 0, 512]) {
        const x = x0 + ox, y = y0 + oy; if (x + rr < 0 || x - rr > 512 || y + rr < 0 || y - rr > 512) continue;
        const gr = g.createRadialGradient(x, y, 0, x, y, rr);
        gr.addColorStop(0, light ? `rgba(255,252,244,${a})` : `rgba(150,130,100,${a})`); gr.addColorStop(1, 'rgba(150,130,100,0)');
        const t = T.theme; T.theme = 'raw'; g.fillStyle = gr; T.theme = t; g.fillRect(x - rr, y - rr, rr * 2, rr * 2);
      }
    }
    g.lineCap = 'round';
    for (let i = 0; i < 900; i++) {
      const x = r() * 512, y = r() * 512, a = r() * T.TAU, l = 4 + r() * 22, dark = r() < 0.6;
      const t = T.theme; T.theme = 'raw'; g.strokeStyle = dark ? `rgba(120,100,70,${0.05 + r() * 0.07})` : `rgba(255,252,244,${0.12 + r() * 0.1})`; T.theme = t;
      g.lineWidth = 0.4 + r() * 0.8; g.beginPath(); g.moveTo(x, y); g.quadraticCurveTo(x + Math.cos(a + 0.5) * l * 0.5, y + Math.sin(a + 0.5) * l * 0.5, x + Math.cos(a) * l, y + Math.sin(a) * l); g.stroke();
    }
    return (paperC = c);
  };
  const raw = (g, prop, v) => { const t = T.theme; T.theme = 'raw'; g[prop] = v; T.theme = t; };   // без перекраски

  // ---- кисть: T.ink ----
  const lerp = T.lerp;
  const I = T.ink = { rng, raw };
  I.ink = a => `rgba(27,24,20,${a})`; I.shu = a => `rgba(194,58,43,${a})`;
  // мазок: путь точек, нажим, сужение концов, сухая кисть (просветы волокон)
  I.stroke = (g, pts, w, { col = '#1b1814', alpha = 0.92, seed = 1, taper = [0.35, 0.15], dry = 0.5, wob = 0.18 } = {}) => {
    if (pts.length < 2 || w <= 0) return;
    { const out = []; let len = 0; for (let i = 1; i < pts.length; i++) len += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
      const per = Math.max(1, Math.ceil(Math.min(60, Math.max(14, len / 5)) / (pts.length - 1)));
      for (let i = 0; i < pts.length - 1; i++) for (let k = 0; k < per; k++) { const t = k / per; out.push([lerp(pts[i][0], pts[i + 1][0], t), lerp(pts[i][1], pts[i + 1][1], t)]); }
      out.push(pts[pts.length - 1]); pts = out; }
    const r = rng(seed), n = pts.length, L = [], R = [];
    for (let i = 0; i < n; i++) {
      const a = pts[Math.max(0, i - 1)], b = pts[Math.min(n - 1, i + 1)], dx = b[0] - a[0], dy = b[1] - a[1], l = Math.hypot(dx, dy) || 1, nx = -dy / l, ny = dx / l, t = i / (n - 1);
      let k = 1; if (t < taper[0]) k = Math.sin(t / taper[0] * Math.PI / 2) ** 0.7; if (t > 1 - taper[1]) k = Math.min(k, Math.sin((1 - t) / taper[1] * Math.PI / 2) ** 0.6);
      const ww = w * k * (1 + (r() - 0.5) * wob) / 2; L.push([pts[i][0] + nx * ww, pts[i][1] + ny * ww]); R.push([pts[i][0] - nx * ww, pts[i][1] - ny * ww]);
    }
    g.save(); g.globalAlpha *= alpha; raw(g, 'fillStyle', col);
    g.beginPath(); g.moveTo(L[0][0], L[0][1]); for (const p of L) g.lineTo(p[0], p[1]); for (let i = n - 1; i >= 0; i--) g.lineTo(R[i][0], R[i][1]); g.closePath(); g.fill();
    if (dry > 0 && w > 2.5) {
      g.globalAlpha = 1; raw(g, 'strokeStyle', 'rgba(236,228,212,.55)'); g.lineCap = 'round';
      const br = Math.max(2, Math.round(w / 3 * dry));
      for (let b = 0; b < br; b++) {
        const off = (r() - 0.5) * 0.8, s0 = Math.floor(n * (0.45 + r() * 0.4)), s1 = Math.min(n - 1, s0 + Math.floor(n * (0.1 + r() * 0.35)));
        g.lineWidth = 0.6 + r() * Math.max(0.6, w * 0.06); g.beginPath();
        for (let i = s0; i <= s1; i++) { const x = lerp(R[i][0], L[i][0], 0.5 + off), y = lerp(R[i][1], L[i][1], 0.5 + off); i === s0 ? g.moveTo(x, y) : g.lineTo(x, y); }
        g.stroke();
      }
    }
    g.restore();
  };
  I.pen = (g, pts, w, col) => { raw(g, 'strokeStyle', col); g.lineWidth = w; g.lineCap = 'round'; g.lineJoin = 'round'; g.beginPath(); pts.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1])); g.stroke(); };
  // энсо: круг одним движением с разрывом (и эллипс — для пола в перспективе: ry)
  I.enso = (g, cx, cy, R, w, a0, sweep, seed, alpha = 0.92, ry = null) => {
    const r = rng(seed), pts = [], N = 120;
    for (let i = 0; i <= N; i++) { const t = i / N, a = a0 + sweep * t, k = 1 + 0.012 * Math.sin(t * 9 + seed) + (r() - 0.5) * 0.004; pts.push([cx + Math.cos(a) * R * k, cy + Math.sin(a) * (ry ?? R) * k]); }
    I.stroke(g, pts, w, { seed, taper: [0.08, 0.3], dry: 0.9, alpha });
  };
  // капля туши: неровное пятно + ореол
  I.blot = (g, x, y, rad, a, seed, col = [27, 24, 20]) => {
    if (!(rad > 0.05) || !isFinite(rad)) return;             // нулевой/отрицательный размер — нечего рисовать
    const r = rng(seed), h = g.createRadialGradient(x, y, rad * 0.4, x, y, rad * 2.6);
    const t = T.theme; T.theme = 'raw';
    h.addColorStop(0, `rgba(${col},${a * 0.22})`); h.addColorStop(1, `rgba(${col},0)`); g.fillStyle = h;
    g.beginPath(); g.arc(x, y, rad * 2.6, 0, T.TAU); g.fill();
    g.fillStyle = `rgba(${col},${a})`; g.beginPath();
    for (let i = 0; i <= 14; i++) { const an = i / 14 * T.TAU, rr = rad * (0.82 + r() * 0.36); i ? g.lineTo(x + Math.cos(an) * rr, y + Math.sin(an) * rr) : g.moveTo(x + Math.cos(an) * rr, y + Math.sin(an) * rr); }
    g.closePath(); g.fill(); T.theme = t;
  };
  // печать ханко: киноварь, знаки вырезаны, края со щербинками
  I.hanko = (g, x, y, s, chars, { col = '#c23a2b', filled = true, seed = 3, rot = 0 } = {}) => {
    const r = rng(seed), h = s * chars.length, t = T.theme; T.theme = 'raw';
    g.save(); g.translate(x + s / 2, y + h / 2); g.rotate(rot); g.translate(-s / 2, -h / 2);
    g.textAlign = 'center'; g.textBaseline = 'middle'; g.font = `${s * 0.66}px ${T.SEAL_FONT}`;
    if (filled) {
      g.fillStyle = col; g.fillRect(0, 0, s, h);
      g.strokeStyle = '#ece4d4'; g.lineWidth = s * 0.05; g.strokeRect(s * 0.09, s * 0.09, s * 0.82, h - s * 0.18);
      g.fillStyle = '#ece4d4'; g.strokeStyle = '#ece4d4'; g.lineWidth = s * 0.025; g.lineJoin = 'round';
      chars.forEach((c, i) => { g.fillText(c, s / 2, s * (i + 0.54)); g.strokeText(c, s / 2, s * (i + 0.54)); });
      for (let i = 0; i < s * 0.9; i++) { g.fillStyle = `rgba(236,228,212,${0.4 + r() * 0.5})`; const ex = r() < 0.5 ? (r() < 0.5 ? 0 : s) : r() * s, ey = r() < 0.5 ? r() * h : (r() < 0.5 ? 0 : h); g.beginPath(); g.arc(ex, ey, 0.6 + r() * s * 0.035, 0, T.TAU); g.fill(); }
      for (let i = 0; i < s * 0.6; i++) { g.fillStyle = `rgba(236,228,212,${0.25 + r() * 0.4})`; g.fillRect(r() * s, r() * h, 0.8 + r() * 1.4, 0.8 + r() * 1.4); }
    } else {
      g.strokeStyle = col; g.lineWidth = s * 0.06; g.strokeRect(0, 0, s, h);
      g.fillStyle = col; chars.forEach((c, i) => g.fillText(c, s / 2, s * (i + 0.54)));
    }
    g.restore(); T.theme = t;
  };
  // туман: горизонтальная полоса бумаги
  I.mist = (g, y, h, W, a) => { const gr = g.createLinearGradient(0, y - h, 0, y + h); const t = T.theme; T.theme = 'raw';
    gr.addColorStop(0, 'rgba(236,228,212,0)'); gr.addColorStop(0.5, `rgba(236,228,212,${a})`); gr.addColorStop(1, 'rgba(236,228,212,0)'); g.fillStyle = gr; T.theme = t; g.fillRect(0, y - h, W, h * 2); };
  // далёкие горы-силуэты (кэш по размеру)
  I.mountains = (g, W, horizon, u, seed = 5) => {
    const r = rng(seed);
    for (let layer = 0; layer < 3; layer++) {
      raw(g, 'fillStyle', `rgba(27,24,20,${0.05 + layer * 0.035})`); g.beginPath(); g.moveTo(0, horizon + u * 1.5);
      let y = horizon - u * (6 - layer * 1.6);
      for (let x = 0; x <= W + u * 2; x += u * 2) { y += (r() - 0.5) * u * 1.6; y = T.clamp(y, horizon - u * (10 - layer * 2), horizon - u * (1 + layer)); g.lineTo(x, y); }
      g.lineTo(W, horizon + u * 1.5); g.closePath(); g.fill();
      I.mist(g, horizon - u * (3 - layer), u * 3, W, 0.7);
    }
  };

  // ---- эффекты сцены в SUMI ----
  const oBackdrop = T.backdrop, oFinish = T.finish, oBloom = T.bloom, oDust = T.dust, oHeader = T.header;
  const paperPats = new WeakMap();
  T.backdrop = (g, W, H, fx, fy) => {
    if (T.theme !== 'sumi') return oBackdrop(g, W, H, fx, fy);
    let pat = paperPats.get(g);                          // узор бумаги — один на холст, не каждый кадр (иначе копятся текстуры)
    if (!pat) { pat = g.createPattern(paperTex(), 'repeat'); paperPats.set(g, pat); }
    const t = T.theme; T.theme = 'raw'; g.fillStyle = pat; T.theme = t; g.fillRect(0, 0, W, H);
  };
  T.finish = (g, W, H) => {
    if (T.theme !== 'sumi') return oFinish(g, W, H);
    if (!T.Q.scan) return;
    const v = g.createRadialGradient(W / 2, H / 2, Math.min(W, H) * 0.5, W / 2, H / 2, Math.max(W, H) * 0.8);
    const t = T.theme; T.theme = 'raw'; v.addColorStop(0, 'rgba(150,120,80,0)'); v.addColorStop(1, 'rgba(150,120,80,.18)'); g.fillStyle = v; T.theme = t; g.fillRect(0, 0, W, H);
  };
  // «растекание туши» вместо свечения: размытая копия кадра ложится через multiply, слабо
  T.bloom = (o, strength = 0.85) => {
    if (T.theme !== 'sumi') return oBloom(o, strength);
    if (!T.Q.bloom) return;
    const cv = o.cv, g = o.g;
    if (!o.bl) o.bl = [0, 1, 2].map(() => { const c = document.createElement('canvas'); c.__raw = 1; return { c, g: c.getContext('2d') }; });
    o.blN = (o.blN || 0) + 1;
    const redo = (o.blN & 1) || o.bl[0].c.width !== Math.max(1, cv.width >> 1);
    let src = cv;
    for (let i = 0; redo && i < 3; i++) {
      const b = o.bl[i], w = Math.max(1, cv.width >> (i + 1)), h = Math.max(1, cv.height >> (i + 1));
      if (b.c.width !== w || b.c.height !== h) { b.c.width = w; b.c.height = h; }
      b.g.globalCompositeOperation = 'copy'; b.g.imageSmoothingEnabled = true; b.g.drawImage(src, 0, 0, w, h); src = b.c;
    }
    g.save(); g.setTransform(1, 0, 0, 1, 0, 0); g.globalCompositeOperation = 'multiply'; g.globalAlpha = 0.22 * strength;
    g.drawImage(o.bl[2].c, 0, 0, cv.width, cv.height); g.restore();
  };
  T.dust = (n, ext, top) => {
    const d = oDust(n, ext, top), draw = d.draw;
    d.draw = (g, cam) => {
      if (T.theme !== 'sumi') return draw(g, cam);
      const a = g.globalAlpha; g.globalAlpha = a * 0.35; draw(g, cam); g.globalAlpha = a;           // редкие пылинки туши
    };
    return d;
  };
  // шапка раздела в SUMI: знак раздела + название засечками, линия — мазок кистью
  T.header = (g, x, y, w, u, title, status, alert) => {
    if (T.theme !== 'sumi') return oHeader(g, x, y, w, u, title, status, alert);
    const cur = T.app && T.app.cur, k = cur && T.KANJI[cur.id];
    let tx = x;
    if (k) { T.txt(g, k, x, y + u * 1.7, u * 2.1, T.C.alert, 'k'); tx += T.tw(g, k, u * 2.1, 'k') + u * 0.5; }
    T.txt(g, String(title), tx, y + u * 1.55, u * 1.75, T.C.acc, 't');
    if (status) {
      g.font = `${u * 0.9}px ${T.FM}`;
      T.txt(g, T.fit(g, String(status).toUpperCase(), Math.max(u * 4, w * 0.55)), x + w, y + u * 1.45, u * 0.9, alert ? T.C.alert : T.C.label, 'm', 'right');
    }
    I.stroke(g, [[x, y + u * 2.3], [x + w * 0.55, y + u * 2.25], [x + w, y + u * 2.35]], u * 0.2, { seed: 9, taper: [0.04, 0.6], dry: 0.8, alpha: 0.75 });
  };

  // ---- переключение ----
  const css = document.documentElement.style;
  T.setTheme = name => {
    if (name === 'tengan') name = 'denko';                // старое имя (до v1.4)
    const th = T.THEMES[name] ? name : 'denko', D = T.THEMES[th];
    if (th === T.theme && T._themeSet) return;
    T._themeSet = 1; T.theme = th; T.hookCanvas(th === 'sumi');
    Object.assign(T.C, D.C); T.HEAT = D.HEAT; T.COOL = D.COOL; T.ALERT = D.ALERT; T.FT = D.FT;
    document.documentElement.classList.toggle('sumi', th === 'sumi');
    const meta = document.querySelector('meta[name=theme-color]'); if (meta) meta.content = D.C.bg;
    try { localStorage.setItem('theme', th); } catch (e) {}
    T.labelNav && T.labelNav();
    T._scan = null;                                            // узор сканлайнов пересоздастся
    for (const o of T._cvs || []) { o.glow = null; }
    css.setProperty('--theme', th);
  };
  T.labelNav = () => {
    const nav = document.getElementById('nav'); if (!nav) return;
    for (const b of nav.children) { const v = T.views.find(x => x.id === b.dataset.id); if (v) b.innerHTML = T.theme === 'sumi' && T.KANJI[v.id] ? `<b class="kj">${T.KANJI[v.id]}</b>${v.title}` : v.title; }
  };
  const urlTheme = new URLSearchParams(location.search).get('theme');
  let start = urlTheme; if (!start) { try { start = localStorage.getItem('theme'); } catch (e) {} }
  T.setTheme(start || 'denko');
  // тема с палки: применяем, если в адресе не задана своя
  setInterval(() => { const st = T.S && T.S.st; if (!urlTheme && st && st.theme && st.theme !== T.theme) T.setTheme(st.theme); }, 400);
})();
