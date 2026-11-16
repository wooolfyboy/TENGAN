// ---- холст в полном разрешении планшета: учитывает devicePixelRatio и поворот ----
// Разрешение: не больше T.RES (1920) по длинной стороне — стабильно на планшете 2.8K.
T._cvs = new Set();
T.canvas = (parent, maxDpr = 3) => {
  const cv = document.createElement('canvas');
  parent.appendChild(cv);
  // непрозрачный холст (alpha:false): браузеру не нужно смешивать его со страницей — дешевле (MDN «Optimizing canvas»).
  // Каждый вид сам заливает фон целиком, прозрачных мест нет.
  const g = cv.getContext('2d', { alpha: false });
  const o = { cv, g, w: 0, h: 0, dpr: 1 };
  const fit = () => {
    const r = parent.getBoundingClientRect();
    if (r.width < 2 || r.height < 2) return;           // раздел спрятан — холст не трогаем (не терять картинку и память на пересоздание)
    const d = Math.min(window.devicePixelRatio || 1, maxDpr, T.RES / Math.max(1, r.width, r.height)) * (T.RS || 1);   // T.RS — адаптивное разрешение (app.js)
    const W = Math.max(1, Math.round(r.width * d)), H = Math.max(1, Math.round(r.height * d));
    if (W !== cv.width || H !== cv.height) { cv.width = W; cv.height = H; }
    o.w = r.width; o.h = r.height; o.dpr = d;
    g.setTransform(d, 0, 0, d, 0, 0);                  // рисуем в CSS-пикселях, чётко на любом экране
  };
  o.fit = fit;
  o.ro = new ResizeObserver(fit); o.ro.observe(parent); fit();
  o.clear = (c) => { g.setTransform(1, 0, 0, 1, 0, 0); g.fillStyle = c || T.C.bg; g.fillRect(0, 0, cv.width, cv.height); g.setTransform(o.dpr, 0, 0, o.dpr, 0, 0); };
  T._cvs.add(o);
  o.dispose = () => { o.ro.disconnect(); cv.remove(); T._cvs.delete(o); };
  return o;
};
