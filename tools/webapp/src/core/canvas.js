// ---- холст в полном разрешении планшета: учитывает devicePixelRatio и поворот ----
T.canvas = (parent, maxDpr = 3) => {
  const cv = document.createElement('canvas');
  parent.appendChild(cv);
  const g = cv.getContext('2d');
  const o = { cv, g, w: 0, h: 0, dpr: 1 };
  const fit = () => {
    const r = parent.getBoundingClientRect(), d = Math.min(window.devicePixelRatio || 1, maxDpr);
    const W = Math.max(1, Math.round(r.width * d)), H = Math.max(1, Math.round(r.height * d));
    if (W !== cv.width || H !== cv.height) { cv.width = W; cv.height = H; }
    o.w = r.width; o.h = r.height; o.dpr = d;
    g.setTransform(d, 0, 0, d, 0, 0);                  // рисуем в CSS-пикселях, чётко на любом экране
  };
  o.fit = fit;
  o.ro = new ResizeObserver(fit); o.ro.observe(parent); fit();
  o.clear = (c) => { g.setTransform(1, 0, 0, 1, 0, 0); g.fillStyle = c || T.C.bg; g.fillRect(0, 0, cv.width, cv.height); g.setTransform(o.dpr, 0, 0, o.dpr, 0, 0); };
  o.dispose = () => { o.ro.disconnect(); cv.remove(); };
  return o;
};
