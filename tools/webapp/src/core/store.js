// ---- последние данные с палки + плавное сглаживание для 60 fps ----
const S = T.S = {
  st: null, ble: null, bleT: 0, wifi: null, snd: null, sndT: 0,
  bleHist: [],                                         // число устройств, точка в секунду
  dev: new Map(),                                      // id -> {r (сглаженный rssi), ...}
  bands: new Float32Array(32), caps: new Float32Array(32), level: -90, peak: -90, hz: 0, energy: 0,
  wave: new Float32Array(64), lvHist: [], target: new Float32Array(32),
};
S.onBle = d => {
  S.ble = d; S.bleT = performance.now();
  S.bleHist.push(d.n); if (S.bleHist.length > 180) S.bleHist.shift();
  const seen = new Set();
  for (const a of d.d) {
    const [id, r, type, vend, name, age, macs] = a; seen.add(id);
    let e = S.dev.get(id);
    if (!e) { e = { id, r, rs: r, h: 0, born: performance.now() }; S.dev.set(id, e); }
    Object.assign(e, { r, type, vend, name, age, macs, alive: true });
  }
  for (const [id, e] of S.dev) if (!seen.has(id)) e.alive = false;
};
S.onWifi = d => { S.wifi = d; };
S.onSnd = d => {
  S.snd = d; S.sndT = performance.now();
  if (!d.ok) return;
  const b = T.hex8(d.b), w = T.hex8(d.w, true);
  for (let k = 0; k < 32; k++) S.target[k] = (b[k] || 0) / 255;
  for (let i = 0; i < 64; i++) S.wave[i] = (w[i] || 0) / 127;
  S.level = d.lv; S.peak = d.pk; S.hz = d.hz; S.energy = d.e;
  S.lvHist.push(d.lv); if (S.lvHist.length > 25 * 120) S.lvHist.shift();
};
// вызывается каждый кадр: всё, что приходит рывками, «перетекает» плавно
S.tick = dt => {
  const k = 1 - Math.pow(0.001, dt);                   // ~ одинаково быстро при любом fps
  for (const e of S.dev.values()) {
    e.rs = T.lerp(e.rs, e.r, k * 0.6);
    e.h = T.lerp(e.h, e.alive ? 1 : 0, k * 0.5);
    if (!e.alive && e.h < 0.01) S.dev.delete(e.id);
  }
  for (let i = 0; i < 32; i++) {
    const t = S.target[i];
    S.bands[i] = t > S.bands[i] ? T.lerp(S.bands[i], t, Math.min(1, k * 3)) : T.lerp(S.bands[i], t, k * 0.8);
    S.caps[i] = Math.max(S.bands[i], S.caps[i] - dt * 0.35);
  }
};
