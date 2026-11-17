// ---- tablet GPS (app only) ----
// Sky and globe use these coordinates, and the stick gets them too (/api/loc). Browsers block geolocation on plain http://192.168.4.1, so it stays silent there.
T.gps = null;
(() => {
  const G = window.Capacitor && window.Capacitor.Plugins && window.Capacitor.Plugins.Geolocation;
  if (!T.native || !G) return;
  let sent = null, sentT = 0;
  const toStick = () => {                                // to the stick: on first fix, a move > 1 km, or every 10 minutes
    const p = T.gps; if (!p) return;
    const moved = !sent || Math.hypot(p.lat - sent.lat, (p.lon - sent.lon) * Math.cos(p.lat * Math.PI / 180)) > 0.009;
    if (!moved && Date.now() - sentT < 600000) return;
    fetch(`${T.apiBase()}/api/loc?lat=${p.lat.toFixed(5)}&lon=${p.lon.toFixed(5)}`, { cache: 'no-store' })
      .then(r => { if (r.ok) { sent = { lat: p.lat, lon: p.lon }; sentT = Date.now(); } }).catch(() => {});
  };
  const show = () => {
    const el = document.getElementById('gps'); if (!el) return;
    const p = T.gps;
    el.textContent = p ? 'GPS ±' + Math.round(p.acc) + 'm' : 'GPS …';
    el.className = p && Date.now() - p.t < 30000 ? 'on' : '';
  };
  const start = async () => {
    try { const s = await G.checkPermissions(); if (s.location !== 'granted') await G.requestPermissions({ permissions: ['location'] }); } catch (e) {}
    try {
      await G.watchPosition({ enableHighAccuracy: true, timeout: 30000, maximumAge: 15000 }, (pos, err) => {
        if (!pos || err) return;
        const c = pos.coords;
        T.gps = { lat: c.latitude, lon: c.longitude, acc: c.accuracy || 0, alt: c.altitude, t: Date.now() };
        show(); toStick();
      });
    } catch (e) { const el = document.getElementById('gps'); if (el) el.textContent = 'GPS OFF'; }
  };
  setInterval(() => { show(); toStick(); }, 5000);
  start(); show();
})();
