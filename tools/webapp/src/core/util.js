// ---- общие помощники и палитра Tactical HUD ----
const T = window.T = { views: [], C: {
  bg: '#02090b', panel: '#071419', line: '#12303a', acc: '#3fd9ff', hot: '#eafcff', steel: '#7f97a3', dim: '#46606b', alert: '#ff8a3d' } };
T.clamp = (v, a, b) => v < a ? a : v > b ? b : v;
T.lerp = (a, b, k) => a + (b - a) * k;
// «жар»: сталь -> бирюза -> белый (как heatV на палке)
T.heatRGB = x => {
  x = T.clamp(x, 0, 1);
  const A = [[70, 96, 107], [63, 217, 255], [234, 252, 255]];
  const i = x < 0.6 ? 0 : 1, t = x < 0.6 ? x / 0.6 : (x - 0.6) / 0.4;
  return A[i].map((v, k) => Math.round(v + (A[i + 1][k] - v) * t));
};
T.heat = (x, a = 1) => { const c = T.heatRGB(x); return `rgba(${c[0]},${c[1]},${c[2]},${a})`; };
T.rgba = (hex, a) => { const n = parseInt(hex.slice(1), 16); return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${a})`; };
T.hash = s => { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) h = Math.imul(h ^ s.charCodeAt(i), 16777619); return h >>> 0; };
T.esc = s => String(s == null ? '' : s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
T.ago = s => s < 60 ? s + 's' : s < 3600 ? Math.floor(s / 60) + 'm' : Math.floor(s / 3600) + 'h ' + Math.floor(s / 60) % 60 + 'm';
T.pct = r => T.clamp((r + 95) / 50, 0, 1);            // сила сигнала 0..1 (-95 .. -45 dBm)
T.hex8 = (h, signed) => {                              // "0a1b.." -> массив байт
  const n = (h || '').length >> 1, a = signed ? new Int8Array(n) : new Uint8Array(n);
  for (let i = 0; i < n; i++) { const v = parseInt(h.substr(i * 2, 2), 16); a[i] = signed && v > 127 ? v - 256 : v; }
  return a;
};
T.el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
