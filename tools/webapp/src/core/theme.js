// ---- темы оформления: DENKO 電光 (бирюзовый неон, HUD) и KURENAI 紅 (манга-панк: чёрный / белый / красный) ----
// Тема — это только палитра и шрифты: T.C (цвета), T.RGB (те же цвета тройками «r,g,b» для rgba()),
// T.HEAT / T.COOL (шкалы силы сигнала), T.ALERT, T.FT. Рисование одно и то же — поэтому тема не тормозит.
// KURENAI — плоская: без свечения и сканлайнов (util.js: T.bloom / T.finish / T.backdrop это проверяют),
// шейдеры WebGL перекрашиваются одной функцией в конце (core/gl.js, T.GLK).
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
    ALERT: [255, 59, 48], FT: "Anton,Rajdhani,'Arial Narrow',sans-serif", FM: "'Space Mono','JetBrains Mono',ui-monospace,monospace", GLK: 1,
  };
  T.THEMES = { denko: DENKO, kurenai: KURENAI };
  T.theme = '';
  T.RGB = {};
  T.setTheme = name => {
    const th = T.THEMES[name] ? name : 'denko', D = T.THEMES[th];
    if (th === T.theme) return;
    T.theme = th;
    Object.assign(T.C, D.C); Object.assign(T.RGB, pack(D.C));
    T.HEAT = D.HEAT; T.COOL = D.COOL; T.ALERT = D.ALERT; T.FT = D.FT; T.FM = D.FM; T.GLK = D.GLK;
    document.documentElement.classList.toggle('kurenai', th === 'kurenai');
    const meta = document.querySelector('meta[name=theme-color]'); if (meta) meta.content = D.C.bg;
    T._bdCache = null; T._finKey = '';
    for (const o of T._cvs || []) o.glow = null;
  };
  // тема — как на палке (st.theme); ?theme=kurenai — для проверки
  const urlTheme = new URLSearchParams(location.search).get('theme');
  T.setTheme(urlTheme || 'denko');
  setInterval(() => { const st = T.S && T.S.st; if (!urlTheme && st && st.theme && st.theme !== T.theme) T.setTheme(st.theme); }, 400);
})();
