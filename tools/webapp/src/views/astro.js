// ---- astronomy and satellites for Sky and Globe (library T.astro, not a view) ----
// Same math as the firmware (gallery/astro.cpp, gallery/sat.cpp): JPL mean elements for planets,
// truncated Meeus series for the Moon, Kepler + J2 secular terms for satellites. Time = tablet clock.
T.astro = (() => {
  const D2R = Math.PI / 180, R2D = 180 / Math.PI;
  const n360 = a => { a %= 360; return a < 0 ? a + 360 : a; };
  const A = {};
  A.jd = unix => unix / 86400 + 2440587.5;
  A.now = () => Date.now() / 1000;
  A.gmst = jd => { const d = jd - 2451545, Tc = d / 36525; return n360(280.46061837 + 360.98564736629 * d + 0.000387933 * Tc * Tc); };
  // equatorial -> horizontal: {alt, az} in degrees (azimuth from north via east)
  A.altAz = (ra, dec, jd, lat, lon, gm) => {
    const H = n360((gm != null ? gm : A.gmst(jd)) + lon - ra) * D2R, d = dec * D2R, p = lat * D2R;
    const sa = Math.sin(p) * Math.sin(d) + Math.cos(p) * Math.cos(d) * Math.cos(H);
    const alt = Math.asin(T.clamp(sa, -1, 1));
    const az = Math.atan2(-Math.cos(d) * Math.sin(H), Math.sin(d) * Math.cos(p) - Math.cos(d) * Math.sin(p) * Math.cos(H));
    return { alt: alt * R2D, az: n360(az * R2D) };
  };
  const eclToEq = (lonDeg, latDeg) => {
    const eps = 23.43928 * D2R, l = lonDeg * D2R, b = latDeg * D2R;
    const x = Math.cos(b) * Math.cos(l), y = Math.cos(eps) * Math.cos(b) * Math.sin(l) - Math.sin(eps) * Math.sin(b);
    const z = Math.sin(eps) * Math.cos(b) * Math.sin(l) + Math.cos(eps) * Math.sin(b);
    return { ra: n360(Math.atan2(y, x) * R2D), dec: Math.asin(z) * R2D };
  };
  // a, da, e, de, I, dI, L, dL, w, dw, N, dN (J2000 + per century)
  const EL = [
    [0.38709927, 0.00000037, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.25032350, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081],
    [0.72333566, 0.00000390, 0.00677672, -0.00004107, 3.39467605, -0.00078890, 181.97909950, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418],
    [1.52371034, 0.00001847, 0.09339410, 0.00007882, 1.84969142, -0.00813131, -4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343],
    [5.20288700, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106],
    [9.53667594, -0.00125060, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794],
  ];
  const EARTH = [1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0, 0];
  A.PLANET_RU = ['Меркурий', 'Венера', 'Марс', 'Юпитер', 'Сатурн'];
  A.PLANET_EN = ['MERCURY', 'VENUS', 'MARS', 'JUPITER', 'SATURN'];
  A.planetName = i => T.L(['Mercury', 'Venus', 'Mars', 'Jupiter', 'Saturn'][i], A.PLANET_RU[i]);
  const helio = (el, Tc) => {
    const a = el[0] + el[1] * Tc, e = el[2] + el[3] * Tc, I = (el[4] + el[5] * Tc) * D2R;
    const L = el[6] + el[7] * Tc, w = el[8] + el[9] * Tc, N = (el[10] + el[11] * Tc) * D2R;
    const M = n360(L - w) * D2R, om = w * D2R - N;
    let E = M;
    for (let k = 0; k < 8; k++) E -= (E - e * Math.sin(E) - M) / (1 - e * Math.cos(E));
    const xp = a * (Math.cos(E) - e), yp = a * Math.sqrt(1 - e * e) * Math.sin(E);
    const co = Math.cos(om), so = Math.sin(om), cN = Math.cos(N), sN = Math.sin(N), cI = Math.cos(I), sI = Math.sin(I);
    return [(co * cN - so * sN * cI) * xp + (-so * cN - co * sN * cI) * yp,
      (co * sN + so * cN * cI) * xp + (-so * sN + co * cN * cI) * yp,
      (so * sI) * xp + (co * sI) * yp];
  };
  A.planet = (p, jd) => {
    const Tc = (jd - 2451545) / 36525, P = helio(EL[p], Tc), E = helio(EARTH, Tc);
    const x = P[0] - E[0], y = P[1] - E[1], z = P[2] - E[2];
    return eclToEq(Math.atan2(y, x) * R2D, Math.atan2(z, Math.hypot(x, y)) * R2D);
  };
  A.sun = jd => { const E = helio(EARTH, (jd - 2451545) / 36525); return eclToEq(Math.atan2(-E[1], -E[0]) * R2D, 0); };
  // {ra, dec, phase 0..1, waxing}
  A.moon = jd => {
    const d = jd - 2451545;
    const L = n360(218.316 + 13.176396 * d), M = n360(134.963 + 13.064993 * d) * D2R;
    const F = n360(93.272 + 13.229350 * d) * D2R, D = n360(297.850 + 12.190749 * d) * D2R, Ms = n360(357.529 + 0.98560028 * d) * D2R;
    const lon = L + 6.289 * Math.sin(M) + 1.274 * Math.sin(2 * D - M) + 0.658 * Math.sin(2 * D) - 0.186 * Math.sin(Ms) + 0.214 * Math.sin(2 * M);
    const r = eclToEq(lon, 5.128 * Math.sin(F));
    r.phase = (1 - Math.cos(D)) / 2; r.waxing = D < Math.PI;
    return r;
  };

  // ---- satellites: elements as in firmware [name, epoch, n0, ndot2, inc, raan0, ecc, argp0, m0, a] ----
  const MU = 398600.4418, RE = 6378.137, J2 = 1.08262668e-3;
  A.RE = RE;
  A.sat = arr => ({ name: arr[0], epoch: arr[1], n0: arr[2], ndot2: arr[3], inc: arr[4], raan0: arr[5], ecc: arr[6], argp0: arr[7], m0: arr[8], a: arr[9] });
  A.satPos = (s, t) => {
    const tm = (t - s.epoch) / 60, cosi = Math.cos(s.inc), sini = Math.sin(s.inc), p = s.a * (1 - s.ecc * s.ecc);
    const k = 1.5 * J2 * (RE / p) * (RE / p) * s.n0;
    const raan = s.raan0 - k * cosi * tm, argp = s.argp0 + k * (2 - 2.5 * sini * sini) * tm;
    const M = (s.m0 + s.n0 * tm + s.ndot2 * tm * tm) % (2 * Math.PI);
    let E = M;
    for (let i = 0; i < 10; i++) E -= (E - s.ecc * Math.sin(E) - M) / (1 - s.ecc * Math.cos(E));
    const nu = 2 * Math.atan2(Math.sqrt(1 + s.ecc) * Math.sin(E / 2), Math.sqrt(1 - s.ecc) * Math.cos(E / 2));
    const r = s.a * (1 - s.ecc * Math.cos(E)), u = argp + nu;
    const cu = Math.cos(u), su = Math.sin(u), cO = Math.cos(raan), sO = Math.sin(raan);
    const x = r * (cO * cu - sO * su * cosi), y = r * (sO * cu + cO * su * cosi), z = r * (su * sini);
    const g = A.gmst(A.jd(t)) * D2R, xe = x * Math.cos(g) + y * Math.sin(g), ye = -x * Math.sin(g) + y * Math.cos(g);
    const rr = Math.hypot(xe, ye, z);
    return { x: xe, y: ye, z, lat: Math.asin(z / rr) * R2D, lon: Math.atan2(ye, xe) * R2D, alt: rr - RE, speed: Math.sqrt(MU * (2 / r - 1 / s.a)) };
  };
  // as seen from (lat, lon): {el, az}
  A.satLook = (p, lat, lon) => {
    const la = lat * D2R, lo = lon * D2R;
    const rx = p.x - RE * Math.cos(la) * Math.cos(lo), ry = p.y - RE * Math.cos(la) * Math.sin(lo), rz = p.z - RE * Math.sin(la);
    const e = -Math.sin(lo) * rx + Math.cos(lo) * ry;
    const n = -Math.sin(la) * Math.cos(lo) * rx - Math.sin(la) * Math.sin(lo) * ry + Math.cos(la) * rz;
    const u = Math.cos(la) * Math.cos(lo) * rx + Math.cos(la) * Math.sin(lo) * ry + Math.sin(la) * rz;
    return { el: Math.asin(u / Math.hypot(rx, ry, rz)) * R2D, az: n360(Math.atan2(e, n) * R2D) };
  };
  // next pass above minEl: {found, rise, set, maxEl, azRise}
  A.nextPass = (s, from, lat, lon, minEl = 10, hours = 24) => {
    const ps = { found: false, rise: 0, set: 0, maxEl: 0, azRise: 0 }, end = from + hours * 3600;
    let up = false;
    for (let t = from; t < end; t += 30) {
      const l = A.satLook(A.satPos(s, t), lat, lon);
      if (l.el >= minEl) {
        if (!up) {
          let tr = t; ps.azRise = l.az;
          if (t > from) for (let q = t - 30; q <= t; q += 2) { const l2 = A.satLook(A.satPos(s, q), lat, lon); if (l2.el >= minEl) { tr = q; ps.azRise = l2.az; break; } }
          ps.rise = tr; up = true; ps.found = true;
        }
        if (l.el > ps.maxEl) ps.maxEl = l.el;
      } else if (up) { ps.set = t; return ps; }
    }
    if (up) ps.set = end;
    return ps;
  };
  // city and satellites come from the stick stream; without a stick - Moscow
  A.place = () => {                                   // where we are: tablet GPS (app) > stick city > Moscow
    if (T.gps) return { city: 'GPS', lat: T.gps.lat, lon: T.gps.lon, gps: true };
    const k = T.S && T.S.sky; return k && k.lat != null ? { city: k.city || '', lat: k.lat, lon: k.lon } : { city: 'Moscow', lat: 55.756, lon: 37.617 }; };
  A.sats = () => {
    const k = T.S && T.S.sky;
    if (!k || !k.sats) return [];
    if (A._satsSrc !== k.sats) { A._satsSrc = k.sats; A._sats = k.sats.map(A.sat).filter(s => s.n0 > 0 && s.a > 0); }
    return A._sats;
  };
  // unpack star data from firmware (once)
  A.data = () => {
    if (A._d) return A._d;
    const D = T.DATA || {}, st = D.stars || [], s16 = v => v > 32767 ? v - 65536 : v;
    const stars = [];
    for (let i = 0; i + 2 < st.length; i += 3) stars.push({ ra: st[i] / 100, dec: st[i + 1] / 100, mag: st[i + 2] / 10 - 2 });
    const names = {}; (D.starNames || []).forEach(([i, n]) => names[i] = n);
    const lines = [], cl = D.cl || [], cs = D.clStart || [];
    for (let k = 0; k + 1 < cs.length; k++) { const L = []; for (let i = cs[k]; i < cs[k + 1]; i++) L.push([cl[i * 2] / 100, s16(cl[i * 2 + 1]) / 100]); lines.push(L); }
    const land = [], lp = D.land || [], ls = D.landStart || [];
    for (let k = 0; k + 1 < ls.length; k++) { const L = []; for (let i = ls[k]; i < ls[k + 1]; i++) L.push([lp[i * 2 + 1] / 100, lp[i * 2] / 100]); land.push(L); }   // [lat, lon]
    const con = (D.con || []).map(c => ({ ra: c[0] / 100, dec: c[1] / 100, ab: c[2], ru: c[3] }));
    return (A._d = { stars, names, lines, land, con });
  };
  return A;
})();
