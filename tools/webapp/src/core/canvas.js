// ---- full-resolution tablet canvas (devicePixelRatio and rotation aware) ----
// Capped at T.RES (1920) on the long side: stays smooth on a 2.8K tablet.
T._cvs = new Set();
T.canvas = (parent, maxDpr = 3) => {
  const cv = document.createElement('canvas');
  parent.appendChild(cv);
  // opaque canvas (alpha:false): the browser need not blend it with the page, which is cheaper. Every view fills its whole background.
  const g = cv.getContext('2d', { alpha: false });
  const o = { cv, g, w: 0, h: 0, dpr: 1 };
  const fit = () => {
    const r = parent.getBoundingClientRect();
    if (r.width < 2 || r.height < 2) return;           // section hidden: keep the canvas (no redraw or realloc)
    const d = Math.min(window.devicePixelRatio || 1, maxDpr, T.RES / Math.max(1, r.width, r.height)) * (T.RS || 1);   // T.RS: adaptive resolution (app.js)
    const W = Math.max(1, Math.round(r.width * d)), H = Math.max(1, Math.round(r.height * d));
    if (W !== cv.width || H !== cv.height) { cv.width = W; cv.height = H; }
    o.w = r.width; o.h = r.height; o.dpr = d;
    g.setTransform(d, 0, 0, d, 0, 0);                  // draw in CSS pixels, sharp on any screen
  };
  o.fit = fit;
  o.ro = new ResizeObserver(fit); o.ro.observe(parent); fit();
  o.clear = (c) => { g.setTransform(1, 0, 0, 1, 0, 0); g.fillStyle = c || T.C.bg; g.fillRect(0, 0, cv.width, cv.height); g.setTransform(o.dpr, 0, 0, o.dpr, 0, 0); };
  T._cvs.add(o);
  o.dispose = () => { o.ro.disconnect(); cv.remove(); T._cvs.delete(o); };
  return o;
};
