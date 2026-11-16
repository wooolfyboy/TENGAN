// ---- тема оформления: пока одна — DENKO 電光 (электрический свет, HUD); TENGAN — имя прошивки ----
// Палитра лежит в T.C / T.HEAT / T.COOL (util.js). T.setTheme оставлен как точка входа для будущих тем:
// новая тема — это ещё одна запись в T.THEMES с теми же полями.
(() => {
  const DENKO = { C: Object.assign({}, T.C, { seal: T.C.acc }), HEAT: T.HEAT, COOL: T.COOL, ALERT: T.ALERT, FT: T.FT };
  T.THEMES = { denko: DENKO };
  T.theme = '';
  T.setTheme = name => {
    const th = T.THEMES[name] ? name : 'denko', D = T.THEMES[th];
    if (th === T.theme) return;
    T.theme = th;
    Object.assign(T.C, D.C); T.HEAT = D.HEAT; T.COOL = D.COOL; T.ALERT = D.ALERT; T.FT = D.FT;
    const meta = document.querySelector('meta[name=theme-color]'); if (meta) meta.content = D.C.bg;
    for (const o of T._cvs || []) o.glow = null;
  };
  T.setTheme('denko');
})();
