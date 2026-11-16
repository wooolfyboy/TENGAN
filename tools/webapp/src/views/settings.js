// ---- НАСТРОЙКИ ПАЛКИ С ПЛАНШЕТА: пульт управления ----
// Всё, что на палке крутится колесом в Settings, — здесь пальцем. Палка сама отдаёт список
// (/api/settings: страницы, пункты, тип, варианты, значение), планшет только рисует и шлёт
// изменения (/api/set). Значение меняется сразу (оптимистично), при ошибке — откатывается.
// Обычный DOM, а не холст: кнопки, ползунки, переключатели — без нагрузки на кадры.
(() => {
  T.KANJI.set = '設';
  const PAGE_KANJI = { general: '設', ble: '藍', bluetooth: '藍', wifi: '網', 'wi-fi': '網', sub: '波', subghz: '波', 'sub-ghz': '波', sound: '音', snd: '音', alert: '警', alerts: '警' };
  const kanjiOf = p => PAGE_KANJI[String(p.id || p.name || '').toLowerCase()] || PAGE_KANJI[String(p.name || '').toLowerCase()] || '設';
  const esc = s => String(s == null ? '' : s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const api = (path) => fetch(T.apiBase() + path, { cache: 'no-store' }).then(r => { if (!r.ok) throw new Error('http ' + r.status); return r.json(); });

  T.views.push({
    id: 'set', title: 'Settings', want: 'st',
    mount(main) {
      this.root = T.el('div', 'view setv');
      this.root.innerHTML = `<div class="set-wrap"><aside class="set-tabs"></aside><section class="set-body"></section></div><div class="set-toast"></div>`;
      main.appendChild(this.root);
      this.tabs = this.root.querySelector('.set-tabs'); this.body = this.root.querySelector('.set-body'); this.toastEl = this.root.querySelector('.set-toast');
      this.page = 0; this.data = null; this.busy = 0; this.screenOn = true; this.err = null;
      try { this.page = +(localStorage.getItem('set_page') || 0); } catch (e) {}
      this.load();
      this.timer = setInterval(() => { if (!this.root.hidden && !this.busy && !this.dragging) this.load(true); }, 5000);
    },
    unmount() { clearInterval(this.timer); this.root.remove(); },
    frame() {},
    load(quiet) {
      api('/api/settings').then(d => { this.data = d; this.err = null; S.cfgFromSettings(d); quiet ? this.refresh() : this.render(); })
        .catch(e => { this.err = e; if (!this.data || !quiet) { this.data = null; this.render(); } });
    },
    toast(msg, bad) {
      const t = this.toastEl; t.textContent = msg; t.className = 'set-toast on' + (bad ? ' bad' : '');
      clearTimeout(this.toastT); this.toastT = setTimeout(() => { t.className = 'set-toast'; }, 1800);
    },
    // ---- отрисовка ----
    render() {
      const d = this.data;
      if (!d || !d.pages || !d.pages.length) {
        this.tabs.innerHTML = '';
        this.body.innerHTML = `<div class="set-empty"><b>NO LINK</b><span>settings live on the stick · open Web on the stick and connect</span></div>`;
        return;
      }
      if (this.page >= d.pages.length) this.page = 0;
      this.tabs.innerHTML = d.pages.map((p, i) => `<button data-i="${i}" class="${i === this.page ? 'on' : ''}"><b class="kj">${kanjiOf(p)}</b>${esc(p.name)}</button>`).join('')
        + `<button data-i="act" class="act ${this.page === 'act' ? 'on' : ''}"><b class="kj">動</b>Actions</button>`;
      this.tabs.querySelectorAll('button').forEach(b => b.onclick = () => {
        this.page = b.dataset.i === 'act' ? 'act' : +b.dataset.i;
        try { localStorage.setItem('set_page', b.dataset.i === 'act' ? 0 : this.page); } catch (e) {}
        this.render();
      });
      if (this.page === 'act') { this.renderActions(); return; }
      const p = d.pages[this.page];
      this.body.innerHTML = `<h2 class="set-h"><span class="kj">${kanjiOf(p)}</span>// ${esc(p.name)}</h2>` +
        p.items.map((it, j) => `<div class="set-row" data-j="${j}">${this.rowHtml(it)}</div>`).join('');
      this.body.querySelectorAll('.set-row').forEach(row => this.bind(row, p.items[+row.dataset.j]));
    },
    refresh() {                                            // тихое обновление значений без перерисовки (не сбивает касание)
      const d = this.data; if (!d || this.page === 'act' || !d.pages[this.page]) { if (!d) this.render(); return; }
      const p = d.pages[this.page], rows = this.body.querySelectorAll('.set-row');
      if (rows.length !== p.items.length) { this.render(); return; }
      rows.forEach(row => { const it = p.items[+row.dataset.j]; const html = this.rowHtml(it); if (row._html !== html) { row.innerHTML = html; row._html = html; this.bind(row, it); } });
    },
    rowHtml(it) {
      const head = `<div class="set-name"><b>${esc(it.name)}</b><span>${esc(it.hint || '')}</span></div>`;
      let ctl = '';
      if (it.type === 'enum') {
        const opts = it.opts || [];
        ctl = `<div class="chips">${opts.map((o, i) => `<button data-o="${i}" class="${this.isSel(it, o, i) ? 'on' : ''}">${esc(o)}</button>`).join('')}</div>`;
      } else if (it.type === 'range') {
        ctl = `<div class="rng"><input type="range" min="${it.min}" max="${it.max}" step="${it.step || 1}" value="${it.v}"><output>${esc(it.v)}${esc(it.unit || '')}</output></div>`;
      } else if (it.type === 'bool') {
        const on = it.v === true || it.v === 1 || it.v === '1' || it.v === 'on';
        ctl = `<button class="tgl ${on ? 'on' : ''}" aria-pressed="${on}"><i></i><span>${on ? 'ON' : 'OFF'}</span></button>`;
      } else {
        ctl = `<div class="info">${esc(it.v)}${esc(it.unit || '')}</div>`;
      }
      return head + ctl;
    },
    isSel(it, o, i) { return typeof it.v === 'number' ? it.v === i : String(it.v) === String(o); },
    bind(row, it) {
      row._html = row._html || row.innerHTML;
      if (it.type === 'enum') row.querySelectorAll('.chips button').forEach(b => b.onclick = () => {
        const i = +b.dataset.o, o = it.opts[i];
        this.set(it, typeof it.v === 'number' ? i : o);
      });
      else if (it.type === 'range') {
        const r = row.querySelector('input'), out = row.querySelector('output');
        r.oninput = () => { this.dragging = true; out.textContent = r.value + (it.unit || ''); };
        r.onchange = () => { this.dragging = false; this.set(it, +r.value); };
      } else if (it.type === 'bool') {
        row.querySelector('.tgl').onclick = () => { const on = it.v === true || it.v === 1 || it.v === '1' || it.v === 'on'; this.set(it, typeof it.v === 'boolean' ? !on : on ? 0 : 1); };
      }
    },
    set(it, v) {
      const old = it.v; it.v = v; this.busy++;
      if (this.page !== 'act') this.refresh();
      api(`/api/set?k=${encodeURIComponent(it.k)}&v=${encodeURIComponent(v)}`).then(r => {
        if (!r || !r.ok) throw new Error((r && r.err) || 'refused');
        if (r.item) Object.assign(it, r.item);
        if (String(it.k).startsWith('al_')) { S.cfgFromItem(r.item || it); if (it.k === 'al_warn' || it.k === 'al_alarm') this.load(true); }   // штаб сразу видит новые пороги
        if (it.k === 'theme') {                              // тема — сразу и на планшете
          const th = String(typeof it.v === 'number' ? (it.opts || [])[it.v] : it.v).toLowerCase();
          if (S.st) S.st.theme = th;
          T.setTheme(th);
        }
        this.toast('saved on the stick');
      }).catch(e => { it.v = old; this.toast('not saved · ' + (e.message || 'no link'), true); })
        .finally(() => { this.busy--; if (this.page !== 'act') this.refresh(); });
    },
    // ---- действия ----
    renderActions() {
      this.body.innerHTML = `<h2 class="set-h"><span class="kj">動</span>// Actions</h2>
        <div class="set-row"><div class="set-name"><b>Stick screen</b><span>turn the stick's display off to save battery</span></div>
          <button class="tgl ${this.screenOn ? 'on' : ''}" data-a="screen"><i></i><span>${this.screenOn ? 'ON' : 'OFF'}</span></button></div>
        <div class="set-row"><div class="set-name"><b>LEDs</b><span>turn the led strip off now</span></div>
          <div class="chips"><button data-a="leds">OFF</button></div></div>`;
      this.body.querySelector('[data-a=screen]').onclick = () => {
        const v = this.screenOn ? 0 : 1;
        api(`/api/act?a=screen&v=${v}`).then(r => { if (!r.ok) throw new Error(r.err || 'refused'); this.screenOn = !!v; this.renderActions(); this.toast(v ? 'stick screen on' : 'stick screen off'); })
          .catch(e => this.toast('failed · ' + (e.message || 'no link'), true));
      };
      this.body.querySelector('[data-a=leds]').onclick = () => {
        api('/api/act?a=leds&v=0').then(r => { if (!r.ok) throw new Error(r.err || 'refused'); this.toast('leds off'); })
          .catch(e => this.toast('failed · ' + (e.message || 'no link'), true));
      };
    },
  });

  // ---- подписи устройств: окно «назвать» (вызывает Bluetooth › Devices) ----
  T.nameDevice = (e, cur) => {
    const modal = document.getElementById('modal'), card = document.getElementById('modalCard');
    card.innerHTML = `<div class="lbl-dlg"><h3>// NAME THIS DEVICE</h3>
      <p class="lbl-sub">${esc((e.vend || '') + ' · ' + (e.type || 'device') + (e.name ? ' · ' + e.name : ''))}</p>
      <input id="lblIn" maxlength="20" autocomplete="off" spellcheck="false" placeholder="my AirPods" value="${esc(cur || '')}">
      <p class="lbl-note" hidden></p>
      <div class="lbl-btns"><button data-b="save" class="pri">SAVE</button><button data-b="del">REMOVE</button><button data-b="cancel">CANCEL</button></div></div>`;
    modal.classList.remove('hide');
    const inp = card.querySelector('#lblIn'), note = card.querySelector('.lbl-note');
    setTimeout(() => { inp.focus(); inp.select(); }, 30);
    const close = () => { modal.classList.add('hide'); card.innerHTML = ''; };
    const send = n => {
      S.setLabel(e.id, n);                                  // сразу видно на планшете
      api(`/api/label?id=${e.id}&n=${encodeURIComponent(n)}`).then(r => {
        if (!r.ok) throw new Error(r.err || 'refused');
        if (r.stable === false && n) { note.hidden = false; note.textContent = 'this device changes its address — the name may be lost'; setTimeout(close, 2600); }
        else close();
      }).catch(err => { note.hidden = false; note.textContent = 'not saved on the stick · ' + (err.message || 'no link'); });
    };
    card.querySelector('[data-b=save]').onclick = () => send(inp.value.trim().slice(0, 20));
    card.querySelector('[data-b=del]').onclick = () => send('');
    card.querySelector('[data-b=cancel]').onclick = close;
    inp.onkeydown = ev => { if (ev.key === 'Enter') send(inp.value.trim().slice(0, 20)); if (ev.key === 'Escape') close(); };
    modal.onclick = ev => { if (ev.target === modal) close(); };
  };
})();
