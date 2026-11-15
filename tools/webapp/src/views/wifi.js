// ---- WI-FI: каналы «холмиками» во весь экран + список сетей ----
T.views.push({
  id: 'wifi', title: 'Wi-Fi', want: 'wifi',
  mount(main) {
    const v = this.root = T.el('div', 'view');
    const st = T.el('div', 'stage'); v.appendChild(st); this.cv = T.canvas(st);
    this.side = T.el('div', 'side'); v.appendChild(this.side);
    this.side.innerHTML = `<div class="card"><div class="lab">networks</div><div class="big" id="w-n">—</div><div class="mut" id="w-b"></div></div><div class="card list" id="w-list"></div>`;
    main.appendChild(v); this.t1 = 9; this.h = {};
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  frame(dt) {
    const { g, w, h } = this.cv; this.cv.clear();
    const nets = (S.wifi && S.wifi.nets) || [];
    const L = 56, Rr = 24, top = 60, bot = h - 46, cw = (w - L - Rr) / 15;
    const X = c => L + (c + 0.5) * cw, Y = dbm => bot - (T.clamp((dbm + 100) / 70, 0, 1)) * (bot - top);
    g.strokeStyle = T.C.line; g.lineWidth = 1;
    for (let d = -90; d <= -40; d += 10) { g.beginPath(); g.moveTo(L, Y(d)); g.lineTo(w - Rr, Y(d)); g.stroke(); T.text(g, d, L - 8, Y(d) + 4, 12, T.C.dim, 'right'); }
    for (let c = 1; c <= 14; c++) T.text(g, c, X(c), bot + 20, 13, c === 1 || c === 6 || c === 11 ? T.C.acc : T.C.steel, 'center');
    const best = nets.length ? T.bestCh(nets) : 0;
    if (best) { g.fillStyle = T.C.acc; g.beginPath(); g.moveTo(X(best), bot + 26); g.lineTo(X(best) - 7, bot + 38); g.lineTo(X(best) + 7, bot + 38); g.fill(); }
    // холмики: плавно вырастают; слабые сзади
    const k = 1 - Math.pow(0.02, dt);
    const sorted = [...nets].sort((a, b) => a.r - b.r);
    for (const n of sorted) {
      const key = n.b, tgt = n.r; this.h[key] = this.h[key] == null ? -100 : T.lerp(this.h[key], tgt, k);
      const r = this.h[key], x = X(n.c), half = 2 * cw, yt = Y(r), open = n.e === 'open', p = T.pct(r);
      const col = open ? [255, 138, 61] : T.heatRGB(0.2 + 0.8 * p);
      const grd = g.createLinearGradient(0, yt, 0, bot); grd.addColorStop(0, `rgba(${col},.30)`); grd.addColorStop(1, `rgba(${col},0)`);
      g.beginPath(); g.moveTo(x - half, bot); g.bezierCurveTo(x - half * 0.45, bot, x - half * 0.55, yt, x, yt); g.bezierCurveTo(x + half * 0.55, yt, x + half * 0.45, bot, x + half, bot);
      g.fillStyle = grd; g.fill(); g.strokeStyle = `rgb(${col})`; g.lineWidth = 1.6; g.stroke();
      if (p > 0.45) T.text(g, n.s || '(hidden)', x, yt - 8, 12, open ? T.C.alert : T.C.hot, 'center');
    }
    T.corners(g, 8, 8, w - 16, h - 16);
    T.text(g, '// CHANNELS 2.4 GHZ', 18, 34, 15, T.C.acc, 'left', 'alphabetic', true);
    if (best) T.text(g, 'BEST ' + best, w - 22, 34, 15, T.C.hot, 'right', 'alphabetic', true);
    if (!S.wifi) T.text(g, 'scanning…', w / 2, h / 2, 18, T.C.steel, 'center');
    if ((this.t1 += dt) > 2) { this.t1 = 0; this.list(nets, best); }
  },
  list(nets, best) {
    document.getElementById('w-n').textContent = S.wifi && S.wifi.scanned ? nets.length : '…';
    document.getElementById('w-b').textContent = best ? 'best channel ' + best : '';
    document.getElementById('w-list').innerHTML = [...nets].sort((a, b) => b.r - a.r).map(n => `<div class="row"><div><div class="bar"><i style="width:${T.pct(n.r) * 100}%"></i></div><div class="s">${n.r} dBm</div></div>
      <div class="n">${T.esc(n.s || '(hidden)')} <span class="${n.e === 'open' ? 'alert' : 'dim'}">${T.esc(n.e)}</span><div class="s">${n.v ? T.esc(n.v) + ' · ' : ''}${T.esc(n.b)}</div></div><div class="mut">ch ${n.c}</div></div>`).join('');
  },
});
