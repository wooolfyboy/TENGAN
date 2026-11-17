// ---- themes: DENKO 電光 (teal neon HUD) and KURENAI 紅 (manga-punk: black / white / red) ----
// A theme is only palette and fonts: T.C (colors), T.RGB (same as "r,g,b" for rgba()), T.HEAT / T.COOL (signal scales), T.ALERT, T.FT.
// Drawing code is shared, so themes cost nothing. KURENAI is flat: no glow or scanlines (T.bloom / T.finish / T.backdrop check it);
// WebGL shaders are recolored by one function at the end (core/gl.js, T.GLK).
(() => {
  const rgb = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16)).join(',');
  const pack = C => { const o = {}; for (const k in C) o[k] = rgb(C[k]); return o; };
  const DENKO = {
    C: Object.assign({}, T.C, { seal: T.C.acc, mid: '#285f82', bg2: '#050a10' }),
    HEAT: T.HEAT, COOL: T.COOL, ALERT: T.ALERT, FT: T.FT, FM: T.FM, GLK: 0,
  };
  const KURENAI = {
    C: { bg: '#0b0b0b', acc: '#e5262f', hot: '#f4f1ea', alert: '#ff3b30', label: '#bdb9b0', dim: '#8a8a8a', line: '#3a3a3a',
      panel: '#1e1e1e', steel: '#bdb9b0', seal: '#e5262f', mid: '#6e6e6c', bg2: '#101010' },
    HEAT: [[0, [38, 38, 38]], [0.4, [110, 110, 108]], [0.75, [244, 241, 234]], [1, [229, 38, 47]]],
    COOL: [[0, [11, 11, 11]], [0.25, [42, 42, 42]], [0.5, [95, 95, 95]], [0.72, [150, 150, 148]], [0.86, [229, 38, 47]], [1, [244, 241, 234]]],
    ALERT: [255, 59, 48], FT: "Oswald,'Arial Narrow',sans-serif", FM: "'JetBrains Mono',ui-monospace,monospace", GLK: 1,
  };
  // MONO: strict black-and-white start; the boot screen stays neutral (same in browser, tablet app and PC app) until the stick's theme is known
  const MONO = {
    C: { bg: '#000000', acc: '#ffffff', hot: '#ffffff', alert: '#d9d9d9', label: '#b4b4b4', dim: '#6c6c6c', line: '#2e2e2e',
      panel: '#141414', steel: '#b4b4b4', seal: '#ffffff', mid: '#5a5a5a', bg2: '#080808' },
    HEAT: [[0, [40, 40, 40]], [0.5, [130, 130, 130]], [1, [255, 255, 255]]],
    COOL: [[0, [0, 0, 0]], [0.5, [100, 100, 100]], [1, [255, 255, 255]]],
    ALERT: [217, 217, 217], FT: T.FT, FM: T.FM, GLK: 1,
  };
  DENKO.FLAT = 0; KURENAI.FLAT = 1; MONO.FLAT = 1;
  T.THEMES = { denko: DENKO, kurenai: KURENAI, mono: MONO };
  T.theme = '';
  T.RGB = {};
  T.setTheme = name => {
    const th = T.THEMES[name] ? name : 'denko', D = T.THEMES[th];
    if (th === T.theme) return;
    T.theme = th;
    Object.assign(T.C, D.C); Object.assign(T.RGB, pack(D.C));
    T.HEAT = D.HEAT; T.COOL = D.COOL; T.ALERT = D.ALERT; T.FT = D.FT; T.FM = D.FM; T.GLK = D.GLK; T.FLAT = D.FLAT;
    document.documentElement.classList.toggle('kurenai', th === 'kurenai'); document.documentElement.classList.toggle('mono', th === 'mono');
    const meta = document.querySelector('meta[name=theme-color]'); if (meta) meta.content = D.C.bg;
    T._bdCache = null; T._finKey = '';
    for (const o of T._cvs || []) o.glow = null;
  };
  // theme follows the stick (st.theme); ?theme=kurenai for testing
  const urlTheme = new URLSearchParams(location.search).get('theme');
  T.setTheme(urlTheme || 'mono');                       // neutral start; the theme arrives from the stick
  T.themeFallback = () => { if (T.theme === 'mono') T.setTheme('denko'); };   // no stick: default to DENKO
  setTimeout(() => { if (!T.themeHold) T.themeFallback(); }, 9000);
  // during boot (boot.js holds T.themeHold) stay black and white; the theme switches on at once before the splash
  T.themeHold = !new URLSearchParams(location.search).has('noboot');
  const urlLang = new URLSearchParams(location.search).get('lang');
  setInterval(() => {
    const st = T.S && T.S.st; if (T.themeHold || !st) return;
    if (!urlTheme && st.theme && st.theme !== T.theme) T.setTheme(st.theme);
    if (!urlLang && st.lang && st.lang !== T.LANG) T.setLang(st.lang);      // language as on the device (changed only there)
  }, 400);
})();
