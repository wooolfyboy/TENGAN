// ---- OVERVIEW: радар-сводка во весь экран + плитки ----
T.views.push({
  id: 'ov', title: 'Overview', want: 'ble,wifi',
  mount(main) {
    const v = this.root = T.el('div', 'view');
    const st = T.el('div', 'stage'); v.appendChild(st);
    this.cv = T.canvas(st);
    const side = this.side = T.el('div', 'side');
    side.innerHTML = `<div class="tiles">
      <div class="card"><div class="lab">bluetooth</div><div class="big" id="o-b">—</div><div class="mut" id="o-b2"></div></div>
      <div class="card"><div class="lab">wi-fi</div><div class="big" id="o-w">—</div><div class="mut" id="o-w2"></div></div>
      <div class="card"><div class="lab">battery</div><div class="big" id="o-p">—</div><div class="mut" id="o-p2"></div></div>
      <div class="card"><div class="lab">uptime</div><div class="mid" id="o-u">—</div><div class="mut" id="o-u2"></div></div></div>
      <div class="card"><div class="lab">devices · 3 min</div><canvas id="o-sp" style="width:100%;height:110px;display:block"></canvas></div>`;
    v.appendChild(side); main.appendChild(v);
    this.sweep = 0; this.t1 = 0;
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  frame(dt, t) {
    const { g, w, h } = this.cv; this.cv.clear();
    this.sweep += dt * 1.6;
    T.radar(g, w / 2, h / 2, Math.min(w, h) * 0.44, this.sweep, true);
    T.corners(g, 8, 8, w - 16, h - 16);
    T.text(g, '// AROUND YOU', 18, 34, 15, T.C.acc, 'left', 'alphabetic', true);
    if ((this.t1 += dt) > 0.5) { this.t1 = 0; this.tiles(); }
  },
  tiles() {
    const $ = id => document.getElementById(id);
    if (S.ble) {
      $('o-b').textContent = S.ble.on ? S.ble.n : 'off';
      const tr = S.ble.d.filter(a => a[2] === 'tracker').length;
      $('o-b2').innerHTML = tr ? `<span class="alert">${tr} tracker${tr > 1 ? 's' : ''} nearby</span>` : S.ble.m + ' addresses';
    }
    if (S.wifi) {
      $('o-w').textContent = S.wifi.scanned ? S.wifi.nets.length : '…';
      $('o-w2').textContent = S.wifi.scanned ? 'best channel ' + T.bestCh(S.wifi.nets) : 'scanning';
    }
    if (S.st) {
      $('o-p').textContent = S.st.bat >= 0 ? S.st.bat + '%' : 'n/a';
      $('o-p2').textContent = S.st.usb ? 'USB power' : 'on battery';
      $('o-u').textContent = T.ago(S.st.up);
      $('o-u2').textContent = S.st.cli + ' connected';
    }
    const c = $('o-sp'); if (!c) return;
    const r = c.getBoundingClientRect(), d = window.devicePixelRatio || 1;
    c.width = r.width * d; c.height = r.height * d;
    const g = c.getContext('2d'); g.scale(d, d);
    const a = S.bleHist; if (a.length < 2) return;
    const mx = Math.max(...a, 1), W = r.width, H = r.height;
    g.strokeStyle = T.C.line; g.lineWidth = 1;
    for (let k = 1; k < 4; k++) { g.beginPath(); g.moveTo(0, H * k / 4); g.lineTo(W, H * k / 4); g.stroke(); }
    const grd = g.createLinearGradient(0, 0, 0, H); grd.addColorStop(0, T.rgba(T.C.acc, 0.35)); grd.addColorStop(1, T.rgba(T.C.acc, 0));
    g.beginPath(); a.forEach((v, i) => { const x = i / 179 * W, y = H - 4 - v / mx * (H - 14); i ? g.lineTo(x, y) : g.moveTo(x, y); });
    g.lineTo((a.length - 1) / 179 * W, H); g.lineTo(0, H); g.fillStyle = grd; g.fill();
    g.beginPath(); a.forEach((v, i) => { const x = i / 179 * W, y = H - 4 - v / mx * (H - 14); i ? g.lineTo(x, y) : g.moveTo(x, y); });
    g.strokeStyle = T.C.acc; g.lineWidth = 2; g.stroke();
    T.text(g, 'max ' + mx, 4, 12, 11, T.C.steel);
  },
});
// лучший из каналов 1/6/11 (меньше всего чужих сетей рядом)
T.bestCh = nets => {
  const cnt = Array(15).fill(0);
  for (const n of nets) for (let k = n.c - 2; k <= n.c + 2; k++) if (k > 0 && k < 15) cnt[k] += T.pct(n.r) + 0.3;
  return [1, 6, 11].sort((a, b) => cnt[a] - cnt[b])[0];
};
