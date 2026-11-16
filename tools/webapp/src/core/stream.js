// ---- канал с палкой: «вторая дверь» (SSE, порт 81), запасной — опрос /api/* ----
const Stream = T.Stream = { es: null, want: '', mode: 'off', lastMsg: 0, pollT: null };
// приложение для компьютера (tools/desktop) кладёт в страницу window.TENGAN_DESKTOP;
// в режиме USB данные идут по кабелю через мост window.TenganUSB (want / api / poll)
const USB = window.TENGAN_DESKTOP && window.TENGAN_DESKTOP.mode === 'usb' ? window.TenganUSB : null;
T.desktop = !!window.TENGAN_DESKTOP;
// события потока — одни и те же для Wi-Fi (SSE) и USB
const H = {
  st: d => { S.onSt(d); T.app && T.app.status(); },
  ble: d => S.onBle(d), wifi: d => S.onWifi(d), snd: d => S.onSnd(d), sub: d => S.onSub(d), pc: d => S.onPc(d),
  sat: d => S.onSat(d), wx: d => S.onWx(d), ir: d => S.onIr(d), nfc: d => S.onNfc(d), sens: d => S.onSens(d),
};
if (USB) {
  // запросы /api/… — тоже по кабелю (страница их не замечает: обычный fetch)
  const of = window.fetch.bind(window);
  window.fetch = (u, o) => {
    const url = typeof u === 'string' ? u : (u && u.url) || '';
    const i = url.indexOf('/api/');
    if (i < 0) return of(u, o);
    return USB.api(url.slice(i)).then(t => new Response(t, { status: 200, headers: { 'Content-Type': 'application/json' } }));
  };
}
const host = () => T.stickHost();
const ssePort = () => new URLSearchParams(location.search).get('sse') || '81';
const apiBase = () => T.apiBase();

Stream.setLink = (mode) => {
  Stream.mode = mode;
  const d = document.getElementById('link'), t = document.getElementById('linkTxt');
  d.className = 'dot ' + (mode === 'live' || mode === 'usb' ? 'live' : mode === 'poll' || mode === 'usbwait' ? 'poll' : 'off');
  t.textContent = mode === 'live' ? 'LIVE' : mode === 'poll' ? 'POLL' : mode === 'usb' ? 'USB' : mode === 'usbwait' ? 'USB · OPEN WEB ON STICK' : mode === 'usboff' ? 'USB · NO STICK' : 'NO LINK';
};
Stream.connect = (want) => {
  Stream.want = want;
  if (USB) { Stream.usb(want); return; }
  if (Stream.es) { Stream.es.close(); Stream.es = null; }
  clearInterval(Stream.pollT); Stream.pollT = null;
  let opened = false;
  const es = new EventSource(`http://${host()}:${ssePort()}/stream?want=${want}`);
  Stream.es = es;
  const on = (ev, fn) => es.addEventListener(ev, e => { Stream.lastMsg = performance.now(); try { fn(JSON.parse(e.data)); } catch (x) {} });
  es.onopen = () => { opened = true; Stream.setLink('live'); };
  for (const k in H) on(k, H[k]);
  es.onerror = () => { if (Stream.mode === 'live') Stream.setLink('off'); };
  setTimeout(() => { if (!opened && Stream.es === es) { es.close(); Stream.es = null; Stream.poll(want); } }, 5000);
};
// запасной путь: те же данные через старые /api/* (медленнее, но работает)
Stream.poll = (want) => {
  Stream.setLink('poll');
  // палка включает радио по вкладке: строго то, что смотрим, остальное молчит
  const has = k => want.split(/[,&]/).includes(k);
  const tab = has('snd') ? 'snd' : has('sub') ? 'sub' : has('ble') && has('wifi') ? 'ov' : has('ble') ? 'ble' : has('wifi') ? 'wifi' : 'sys';
  const J = u => fetch(apiBase() + u + (u.includes('?') ? '&' : '?') + 'tab=' + tab, { cache: 'no-store' }).then(r => r.json());
  let n = 0, busy = false;
  const step = async () => {
    if (busy) return;                                    // палка не успела ответить — не заваливаем её запросами
    busy = true;
    try {
      n++;
      if (has('snd')) {
        const j = await J('/api/snd');
        S.onSnd({ en: j.en ? 1 : 0, ok: j.ok ? 1 : 0, lv: j.lv, pk: j.pk, hz: j.hz, e: j.e,
          b: (j.b || []).map(v => Math.round(v * 2.55).toString(16).padStart(2, '0')).join(''), w: '' });
      }
      if (has('ble') && n % 8 === 1) {
        const j = await J('/api/ble');
        S.onBle({ on: j.on ? 1 : 0, n: j.count, m: j.macs, d: j.dev.map(d => [d.id, d.r, d.t, d.v, d.n, d.age, d.m]) });
      }
      if (has('wifi') && n % 40 === 1) S.onWifi(await J('/api/wifi'));
      if (has('sub') && n % 2 === 1) {
        const m = /sm=(\d+)/.exec(want), b = /sb=(\d+)/.exec(want);
        S.onSub(await J('/api/sub?m=' + (m ? m[1] : 0) + '&b=' + (b ? b[1] : 1)));
      }
      if (n % 16 === 1) { const i = await J('/api/info'); S.onSt({ name: 'TENGAN', up: i.up, bat: i.bat.ok ? i.bat.soc : -1, usb: i.bat.usb ? 1 : 0, cli: i.cli }); T.app.status(); }
      Stream.lastMsg = performance.now();
    } catch (e) { Stream.setLink('off'); }
    busy = false;
  };
  Stream.pollT = setInterval(step, 125); step();
  // запасной путь — временный: через 8 с снова пробуем нормальный поток
  clearTimeout(Stream.retryT);
  Stream.retryT = setTimeout(() => { if (Stream.mode === 'poll' && Stream.want === want) Stream.connect(want); }, 8000);
};

// ---- USB (приложение для компьютера): подписка + длинный опрос моста ----
// Мост раз в ≤0.25 с отдаёт пачку «[[событие, данные], …]»; «_x» — служебное от приложения/палки.
Stream.usb = (want) => {
  USB.want(want).catch(() => {});
  if (Stream.usbLoop) return;
  Stream.usbLoop = true;
  if (Stream.mode !== 'usb') Stream.setLink('usbwait');
  (async () => {
    for (;;) {
      let batch;
      try { batch = JSON.parse(await USB.poll()); } catch (e) { await new Promise(r => setTimeout(r, 500)); continue; }
      for (const [ev, d] of batch) {
        if (ev === '_x') { Stream.setLink(d === 'web' ? 'usbwait' : d === 'nostick' ? 'usboff' : Stream.mode); continue; }
        Stream.lastMsg = performance.now();
        if (Stream.mode !== 'usb') Stream.setLink('usb');
        const f = H[ev]; if (f) try { f(d); } catch (x) {}
      }
    }
  })();
};
