// ---- BLUETOOTH: радар, 3D-город, список с подробностями ----
// радар (общий — его же рисует Overview)
T.radar = (g, cx, cy, R, sweep, labels) => {
  g.save();
  for (let k = 1; k <= 4; k++) { g.beginPath(); g.arc(cx, cy, R * k / 4, 0, Math.PI * 2); g.strokeStyle = k === 4 ? T.C.acc : T.C.line; g.lineWidth = k === 4 ? 1.5 : 1; g.stroke(); }
  g.strokeStyle = T.C.line; g.beginPath(); g.moveTo(cx - R, cy); g.lineTo(cx + R, cy); g.moveTo(cx, cy - R); g.lineTo(cx, cy + R); g.stroke();
  for (let a = 0; a < 360; a += 10) {                    // риски
    const r = a * Math.PI / 180, l = a % 30 ? 5 : 10;
    g.beginPath(); g.moveTo(cx + Math.cos(r) * R, cy + Math.sin(r) * R); g.lineTo(cx + Math.cos(r) * (R + l), cy + Math.sin(r) * (R + l));
    g.strokeStyle = a % 30 ? T.C.dim : T.C.acc; g.stroke();
  }
  // луч с послесвечением: конический градиент
  const a0 = sweep % (Math.PI * 2);
  if (g.createConicGradient) {
    const cg = g.createConicGradient(a0 - 1.2, cx, cy);
    cg.addColorStop(0, T.rgba(T.C.acc, 0)); cg.addColorStop(0.19, T.rgba(T.C.acc, 0.28)); cg.addColorStop(0.191, T.rgba(T.C.acc, 0)); cg.addColorStop(1, T.rgba(T.C.acc, 0));
    g.fillStyle = cg; g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, R, a0 - 1.2, a0 + 0.01); g.closePath(); g.fill();
  }
  g.strokeStyle = T.C.hot; g.lineWidth = 2; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + Math.cos(a0) * R, cy + Math.sin(a0) * R); g.stroke();
  T.text(g, '1 m', cx + 4, cy - R * 0.25 + 12, 11, T.C.dim); T.text(g, '5 m', cx + 4, cy - R * 0.5 + 12, 11, T.C.dim); T.text(g, '15 m', cx + 4, cy - R * 0.75 + 12, 11, T.C.dim);
  // цели
  const list = [...S.dev.values()].sort((a, b) => a.rs - b.rs);
  const near = [...list].sort((a, b) => b.rs - a.rs).slice(0, 6);
  for (const e of list) {
    const ang = (T.hash(e.id) % 360) * Math.PI / 180, p = T.pct(e.rs), r = R * (1 - p * 0.92);
    const x = cx + Math.cos(ang) * r, y = cy + Math.sin(ang) * r;
    let since = (a0 - ang) % (Math.PI * 2); if (since < 0) since += Math.PI * 2;
    const glow = Math.max(0, 1 - since / 2.5);           // только что «засветил» луч
    const tr = e.type === 'tracker', s = (3 + p * 5) * e.h;
    g.fillStyle = tr ? T.C.alert : T.heat(0.25 + 0.75 * p, 0.55 + 0.45 * glow);
    g.beginPath(); g.moveTo(x, y - s); g.lineTo(x + s, y); g.lineTo(x, y + s); g.lineTo(x - s, y); g.closePath(); g.fill();
    if (glow > 0.6) { g.strokeStyle = T.rgba(tr ? T.C.alert : T.C.hot, glow - 0.4); g.beginPath(); g.arc(x, y, s + 6 * (1 - glow) + 3, 0, Math.PI * 2); g.stroke(); }
    if (labels && near.includes(e)) T.text(g, (e.name || e.type) + ' ' + Math.round(e.rs), x + s + 4, y + 4, 12, tr ? T.C.alert : T.C.steel);
  }
  g.restore();
};

T.views.push({
  id: 'ble', title: 'Bluetooth', want: 'ble', subs: ['Radar', 'City'], sub: 1,
  mount(main) {
    const v = this.root = T.el('div', 'view');
    const st = this.stage = T.el('div', 'stage'); v.appendChild(st);
    this.cv = T.canvas(st);
    this.side = T.el('div', 'side'); v.appendChild(this.side);
    this.side.innerHTML = `<div class="card"><div class="lab">devices around</div><div class="big" id="b-n">—</div><div class="mut" id="b-m"></div></div>
      <div class="card list" id="b-list"></div>`;
    main.appendChild(v);
    this.yaw = 0.6; this.yawV = 0.12; this.pitch = 0.62; this.sweep = 0; this.t1 = 9;
    // пальцем — крутить город
    let drag = null;
    this.cv.cv.addEventListener('pointerdown', e => { drag = { x: e.clientX, y: e.clientY }; this.cv.cv.setPointerCapture(e.pointerId); });
    this.cv.cv.addEventListener('pointermove', e => {
      if (!drag) return;
      this.yaw += (e.clientX - drag.x) * 0.008; this.pitch = T.clamp(this.pitch + (e.clientY - drag.y) * 0.005, 0.2, 1.3);
      this.yawV = (e.clientX - drag.x) * 0.3; drag = { x: e.clientX, y: e.clientY };
    });
    this.cv.cv.addEventListener('pointerup', () => { drag = null; });
    this.side.addEventListener('click', e => { const r = e.target.closest('.row'); if (r) T.devModal(+r.dataset.id); });
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  frame(dt, t) {
    const { g, w, h } = this.cv; this.cv.clear();
    if (this.sub === 0) { this.sweep += dt * 1.6; T.radar(g, w / 2, h / 2, Math.min(w, h) * 0.43, this.sweep, true); }
    else this.city(g, w, h, dt);
    T.corners(g, 8, 8, w - 16, h - 16);
    T.text(g, this.sub === 0 ? '// SCOPE' : '// CITY', 18, 34, 15, T.C.acc, 'left', 'alphabetic', true);
    if (!S.ble) T.text(g, 'waiting for data…', w / 2, h / 2, 18, T.C.steel, 'center');
    if ((this.t1 += dt) > 1) { this.t1 = 0; this.list(); }
  },
  city(g, w, h, dt) {
    this.yawV *= Math.pow(0.05, dt); this.yaw += dt * (0.12 + this.yawV * 0.05);
    const R = Math.min(w, h), P = T.cam(this.yaw, this.pitch, 3.3, R * 1.5, w / 2, h * 0.6);
    // площадка: кольца, лучи, волна сканирования
    const ring = (r, col, lw) => { g.beginPath(); for (let i = 0; i <= 96; i++) { const a = i / 96 * Math.PI * 2, [x, y] = P(Math.cos(a) * r, 0, Math.sin(a) * r); i ? g.lineTo(x, y) : g.moveTo(x, y); } g.strokeStyle = col; g.lineWidth = lw; g.stroke(); };
    ring(1 / 3, T.C.line, 1); ring(2 / 3, T.C.line, 1); ring(1, T.C.acc, 1.5);
    const wv = (performance.now() / 2200) % 1; ring(wv, T.rgba(T.C.acc, 0.6 * (1 - wv)), 2);
    g.strokeStyle = T.C.line; g.lineWidth = 1;
    for (let i = 0; i < 12; i++) { const a = i * Math.PI / 6, [x0, y0] = P(0, 0, 0), [x1, y1] = P(Math.cos(a), 0, Math.sin(a)); g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke(); }
    // башни: сглаженная высота и место; дальние рисуем раньше
    const tw = [];
    for (const e of S.dev.values()) {
      const p = T.pct(e.rs), ang = (T.hash(e.id) % 360) * Math.PI / 180, r = 0.95 - 0.8 * p;
      const x = Math.cos(ang) * r, z = Math.sin(ang) * r, hgt = (0.05 + 0.7 * p * p) * e.h;
      tw.push({ e, x, z, hgt, p, d: P(x, 0, z)[2] });
    }
    tw.sort((a, b) => b.d - a.d);
    const S0 = 0.04;
    for (const q of tw) {
      const tr = q.e.type === 'tracker', base = tr ? [255, 138, 61] : T.heatRGB(0.25 + 0.75 * q.p);
      const C = [[-S0, -S0], [S0, -S0], [S0, S0], [-S0, S0]].map(([dx, dz]) => [P(q.x + dx, 0, q.z + dz), P(q.x + dx, q.hgt, q.z + dz)]);
      const faces = [0, 1, 2, 3].map(i => ({ i, d: C[i][0][2] + C[(i + 1) % 4][0][2] })).sort((a, b) => b.d - a.d);
      for (const f of faces) {
        const a = C[f.i], b = C[(f.i + 1) % 4], k = f.i % 2 ? 0.38 : 0.6;
        g.fillStyle = `rgb(${base.map(v => Math.round(v * k)).join(',')})`;
        g.beginPath(); g.moveTo(a[0][0], a[0][1]); g.lineTo(b[0][0], b[0][1]); g.lineTo(b[1][0], b[1][1]); g.lineTo(a[1][0], a[1][1]); g.closePath(); g.fill();
      }
      g.fillStyle = `rgb(${base.join(',')})`; g.beginPath(); C.forEach((c, i) => i ? g.lineTo(c[1][0], c[1][1]) : g.moveTo(c[1][0], c[1][1])); g.closePath(); g.fill();
      g.strokeStyle = 'rgba(234,252,255,.8)'; g.lineWidth = 1; g.stroke();
    }
    // подписи ближайших
    const top = [...tw].sort((a, b) => b.p - a.p).slice(0, 5), used = [];
    for (const q of top) {
      const [x, y] = P(q.x, q.hgt, q.z); let ly = y - 8;
      for (const u of used) if (Math.abs(u - ly) < 16) ly = u - 16;
      used.push(ly);
      const tr = q.e.type === 'tracker';
      g.strokeStyle = T.C.dim; g.beginPath(); g.moveTo(x, y); g.lineTo(x, ly + 3); g.stroke();
      T.text(g, `${q.e.name || q.e.type} ${Math.round(q.e.rs)}`, x, ly, 13, tr ? T.C.alert : T.C.hot, 'center');
    }
  },
  list() {
    if (!S.ble) return;
    document.getElementById('b-n').textContent = S.ble.on ? S.ble.n : 'off';
    const tr = S.ble.d.filter(a => a[2] === 'tracker').length;
    document.getElementById('b-m').innerHTML = (tr ? `<span class="alert">${tr} tracker(s) · </span>` : '') + S.ble.m + ' addresses';
    document.getElementById('b-list').innerHTML = S.ble.d.slice(0, 60).map(([id, r, type, vend, name, age, macs]) => {
      const al = type === 'tracker';
      return `<div class="row" data-id="${id}"><div><div class="bar"><i style="width:${T.pct(r) * 100}%;${al ? 'background:#ff8a3d' : ''}"></i></div><div class="s">${r} dBm</div></div>
        <div class="n${al ? ' alert' : ''}">${T.esc(name || '(no name)')}<div class="s">${T.esc(vend)}${macs > 1 ? ' · ' + macs + ' addr' : ''}</div></div>
        <div class="${al ? 'alert' : 'mut'}">${T.esc(type)}<div class="s" style="text-align:right">${T.ago(age)}</div></div></div>`;
    }).join('');
  },
});
// подробности устройства — старым API (порт 80), в окне поверх
T.devModal = async id => {
  const m = document.getElementById('modal'), c = document.getElementById('modalCard');
  c.innerHTML = '<div class="mut">loading…</div>'; m.classList.remove('hide');
  m.onclick = e => { if (e.target === m) m.classList.add('hide'); };
  try {
    const h = new URLSearchParams(location.search).get('host'), base = h ? `http://${h}` : '';
    const d = await (await fetch(`${base}/api/bledev?id=${id}`, { cache: 'no-store' })).json();
    const kv = [['type', d.t], ['vendor', d.v], ['signal', `${d.r} dBm (min ${d.rmin}, max ${d.rmax})`], ['near', T.ago(d.age || 0)], ['packets', d.pk], d.cid && ['company id', d.cid], d.mfr && ['mfr data', d.mfr]].filter(Boolean);
    c.innerHTML = `<div class="lab">device</div><div class="mid">${T.esc(d.n || '(no name)')}</div><div class="kv" style="margin-top:12px">${kv.map(([k, v]) => `<span>${k}</span><span>${T.esc(v)}</span>`).join('')}</div>
      ${(d.uu || []).length ? '<div class="lab" style="margin-top:14px">services</div>' + d.uu.map(u => `<div>${u.u} <span class="dim">${T.esc(u.o)}</span></div>`).join('') : ''}
      <div class="lab" style="margin-top:14px">addresses</div>${(d.addr || []).map(a => `<div>${a.mac} <span class="dim">${a.kind} · ${a.r} dBm</span></div>`).join('')}
      <button class="btn" style="margin-top:16px" onclick="document.getElementById('modal').classList.add('hide')">CLOSE</button>`;
  } catch (e) { c.innerHTML = '<div class="alert">no answer from the device</div>'; }
};
