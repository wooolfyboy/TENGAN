// ---- последние данные с палки + сглаживание для плавной картинки ----
// Красота важнее точности: мелкая дрожь сигнала (±2–3 dB) не показывается,
// большие изменения «перетекают» (T.steady). Палка шлёт уже спокойные значения,
// здесь — второй слой, чтобы движение между пакетами было мягким.
const S = T.S = {
  st: null, ble: null, bleT: 0, wifi: null, wifiT: 0, snd: null, sndT: 0, sub: null, subT: 0, pc: null, pcT: 0, sky: null,
  bleHist: [],                                         // число устройств, точка в секунду
  dev: new Map(),                                      // id -> устройство (rs — что рисовать)
  aps: new Map(),                                      // bssid -> сеть (rs — что рисовать)
  bands: new Float32Array(32), caps: new Float32Array(32), level: -90, peak: -90, hz: 0, energy: 0,
  wave: new Float32Array(64), lvHist: [], target: new Float32Array(32),
  subWf: [], subHist: [],                              // водопад Sub-GHz (строки спектра), до 400 строк
  pcHist: [], cores: [],                               // CPU по секундам; ядра (сглаженные)
  events: [],                                          // лента событий: {t, tag, text, alert}
};
S.event = (tag, text, alert) => { S.events.unshift({ t: Date.now(), tag, text, alert: !!alert }); if (S.events.length > 40) S.events.pop(); };

S.onBle = d => {
  S.ble = d; S.bleT = performance.now();
  S.bleHist.push(d.n); if (S.bleHist.length > 180) S.bleHist.shift();
  const seen = new Set(), now = performance.now();
  for (const a of d.d) {
    const [id, r, type, vend, name, age, macs] = a; seen.add(id);
    let e = S.dev.get(id);
    if (!e) {
      e = { id, st: T.steady(4, 0.8), h: 0, born: now, hist: [], first: !S.devInit };
      S.dev.set(id, e);
      if (S.devInit) S.event('BLE', '+ ' + (name || type || 'device') + '  ' + r + ' dBm', type === 'tracker');
    }
    e.st.set(r);
    Object.assign(e, { r, type, vend, name, age, macs, alive: true, seen: now });
    e.hist.push(r); if (e.hist.length > 120) e.hist.shift();
  }
  for (const [id, e] of S.dev) if (!seen.has(id) && e.alive) { e.alive = false; if (now - e.born > 5000) S.event('BLE', '- ' + (e.name || e.type || 'device') + '  lost'); }
  S.devInit = true;
};
S.onWifi = d => {
  S.wifi = d; S.wifiT = performance.now();
  const seen = new Set();
  for (const a of (d.aps || d.nets || [])) {
    const k = a.b || a.bssid || a.s + a.c; seen.add(k);
    let e = S.aps.get(k);
    if (!e) { e = { k, st: T.steady(3, 1.2), h: 0, hist: [] }; S.aps.set(k, e); }
    e.st.set(a.r);
    e.hist.push(a.r); if (e.hist.length > 60) e.hist.shift();
    Object.assign(e, a, { alive: true });
  }
  for (const [k, e] of S.aps) if (!seen.has(k)) e.alive = false;
};
S.onSnd = d => {
  S.snd = d; S.sndT = performance.now();
  if (!d.ok) return;
  const b = T.hex8(d.b), w = T.hex8(d.w, true);
  for (let k = 0; k < 32; k++) S.target[k] = (b[k] || 0) / 255;
  for (let i = 0; i < 64; i++) S.wave[i] = (w[i] || 0) / 127;
  S.level = d.lv; S.peak = d.pk; S.hz = d.hz; S.energy = d.e;
  S.lvHist.push(d.lv); if (S.lvHist.length > 25 * 120) S.lvHist.shift();
};
S.onSub = d => {
  const prevBurst = S.sub && S.sub.burst;
  S.sub = d; S.subT = performance.now();
  if (d.cur) { S.subWf.unshift(Int8Array.from(d.cur)); if (S.subWf.length > 400) S.subWf.pop(); }
  if (d.rssi) S.subHist = d.rssi;
  if (d.burst && !prevBurst) S.event('SUB', 'burst ' + (d.mon != null ? d.mon.toFixed(2) + ' MHz' : ''), true);
};
S.onPc = d => {
  const was = S.pc && S.pc.live;
  S.pc = d; S.pcT = performance.now();
  if (d.live && d.cpu != null) { S.pcHist.push(d.cpu); if (S.pcHist.length > 120) S.pcHist.shift(); }
  if (d.c) d.c.forEach((v, i) => { if (!S.cores[i]) S.cores[i] = T.steady(3, 0.5); S.cores[i].set(v); });
  if (was !== undefined && was !== !!d.live) S.event('PC', d.live ? 'laptop online' : 'laptop stopped', !d.live);
};
S.onSat = d => { S.sky = d; };
// радиопогода (событие wx, раз в секунду): rx — что пришло, wxs — плавные значения для сцены
S.wxs = { clouds: 0, wind: 0, rain: 0, storm: 0, trend: 0 }; S.wxBolt = 0;
S.onWx = d => {
  const prev = S.wx; S.wx = d; S.wxT = performance.now();
  if (d.thunder && !(prev && prev.thunder)) S.wxBolt++;          // новый раскат — сцена рисует молнию
};
S.onSt = d => {
  const prev = S.st; S.st = d;
  if (prev && prev.bat >= 0 && d.bat >= 0 && prev.bat > 15 && d.bat <= 15) S.event('PWR', 'battery ' + d.bat + '%', true);
};
// каждый кадр: всё, что приходит рывками, перетекает плавно
S.tick = dt => {
  if (S.wx) { const k = T.ease(dt, 1.4); for (const key in S.wxs) { const v = +S.wx[key] || 0; S.wxs[key] += (v - S.wxs[key]) * k; } }
  const k = 1 - Math.pow(0.001, dt);
  for (const e of S.dev.values()) {
    e.rs = e.st.tick(dt);
    e.h = T.lerp(e.h, e.alive ? 1 : 0, T.ease(dt, 0.6));
    if (!e.alive && e.h < 0.01) S.dev.delete(e.id);
  }
  for (const e of S.aps.values()) {
    e.rs = e.st.tick(dt);
    e.h = T.lerp(e.h, e.alive ? 1 : 0, T.ease(dt, 0.8));
    if (!e.alive && e.h < 0.01) S.aps.delete(e.k);
  }
  for (const c of S.cores) if (c) c.tick(dt);
  for (let i = 0; i < 32; i++) {
    const t = S.target[i];
    S.bands[i] = t > S.bands[i] ? T.lerp(S.bands[i], t, Math.min(1, k * 3)) : T.lerp(S.bands[i], t, k * 0.8);
    S.caps[i] = Math.max(S.bands[i], S.caps[i] - dt * 0.35);
  }
};
