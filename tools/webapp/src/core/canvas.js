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
// уголки-скобки, как на экране палки
T.corners = (g, x, y, w, h, L = 16, col = T.C.acc) => {
  g.strokeStyle = col; g.lineWidth = 2; g.beginPath();
  g.moveTo(x, y + L); g.lineTo(x, y); g.lineTo(x + L, y);
  g.moveTo(x + w - L, y); g.lineTo(x + w, y); g.lineTo(x + w, y + L);
  g.moveTo(x + w, y + h - L); g.lineTo(x + w, y + h); g.lineTo(x + w - L, y + h);
  g.moveTo(x + L, y + h); g.lineTo(x, y + h); g.lineTo(x, y + h - L);
  g.stroke();
};
T.font = (px, bold) => `${bold ? '700 ' : ''}${px}px ui-monospace,"Roboto Mono","Droid Sans Mono",monospace`;
T.text = (g, s, x, y, px, col, align = 'left', base = 'alphabetic', bold) => {
  g.font = T.font(px, bold); g.fillStyle = col; g.textAlign = align; g.textBaseline = base; g.fillText(s, x, y);
};
// мини-3D: камера на орбите
T.cam = (yaw, pitch, dist, F, cx, cy) => {
  const cyw = Math.cos(yaw), syw = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch);
  return (x, y, z) => {
    const xr = x * cyw - z * syw, zr = x * syw + z * cyw;
    const vy = y * cp + zr * sp, vz = -y * sp + zr * cp, d = dist + vz;
    return [cx + xr * F / d, cy - vy * F / d, d];
  };
};
