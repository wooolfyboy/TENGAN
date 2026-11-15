// ---- канал с палкой: «вторая дверь» (SSE, порт 81), запасной — опрос /api/* ----
const Stream = T.Stream = { es: null, want: '', mode: 'off', lastMsg: 0, pollT: null };
const host = () => new URLSearchParams(location.search).get('host') || location.hostname || '192.168.4.1';
const ssePort = () => new URLSearchParams(location.search).get('sse') || '81';
const apiBase = () => { const h = new URLSearchParams(location.search).get('host'); return h ? `http://${h}` : ''; };

Stream.setLink = (mode) => {
  Stream.mode = mode;
  const d = document.getElementById('link'), t = document.getElementById('linkTxt');
  d.className = 'dot ' + (mode === 'live' ? 'live' : mode === 'poll' ? 'poll' : 'off');
  t.textContent = mode === 'live' ? 'LIVE' : mode === 'poll' ? 'POLL' : 'NO LINK';
};
Stream.connect = (want) => {
  Stream.want = want;
  if (Stream.es) { Stream.es.close(); Stream.es = null; }
  clearInterval(Stream.pollT); Stream.pollT = null;
  let opened = false;
  const es = new EventSource(`http://${host()}:${ssePort()}/stream?want=${want}`);
  Stream.es = es;
  const on = (ev, fn) => es.addEventListener(ev, e => { Stream.lastMsg = performance.now(); try { fn(JSON.parse(e.data)); } catch (x) {} });
  es.onopen = () => { opened = true; Stream.setLink('live'); };
  on('st', d => { S.onSt(d); T.app && T.app.status(); });
  on('ble', S.onBle); on('wifi', S.onWifi); on('snd', S.onSnd); on('sub', S.onSub); on('pc', S.onPc); on('sat', S.onSat);
  es.onerror = () => { if (Stream.mode === 'live') Stream.setLink('off'); };
  setTimeout(() => { if (!opened && Stream.es === es) { es.close(); Stream.es = null; Stream.poll(want); } }, 3500);
};
// запасной путь: те же данные через старые /api/* (медленнее, но работает)
Stream.poll = (want) => {
  Stream.setLink('poll');
  const tab = want.includes('snd') ? 'snd' : want.includes('wifi') && !want.includes('ble') ? 'wifi' : 'ble';
  const J = u => fetch(apiBase() + u + (u.includes('?') ? '&' : '?') + 'tab=' + tab, { cache: 'no-store' }).then(r => r.json());
  let n = 0;
  const step = async () => {
    try {
      n++;
      if (want.includes('snd')) {
        const j = await J('/api/snd');
        S.onSnd({ en: j.en ? 1 : 0, ok: j.ok ? 1 : 0, lv: j.lv, pk: j.pk, hz: j.hz, e: j.e,
          b: (j.b || []).map(v => Math.round(v * 2.55).toString(16).padStart(2, '0')).join(''), w: '' });
      }
      if (want.includes('ble') && n % 8 === 1) {
        const j = await J('/api/ble');
        S.onBle({ on: j.on ? 1 : 0, n: j.count, m: j.macs, d: j.dev.map(d => [d.id, d.r, d.t, d.v, d.n, d.age, d.m]) });
      }
      if (want.includes('wifi') && n % 40 === 1) S.onWifi(await J('/api/wifi'));
      if (want.includes('sub') && n % 2 === 1) {
        const m = /sm=(\d+)/.exec(want), b = /sb=(\d+)/.exec(want);
        S.onSub(await J('/api/sub?m=' + (m ? m[1] : 0) + '&b=' + (b ? b[1] : 1)));
      }
      if (n % 16 === 1) { const i = await J('/api/info'); S.onSt({ name: 'TENGAN', up: i.up, bat: i.bat.ok ? i.bat.soc : -1, usb: i.bat.usb ? 1 : 0, cli: i.cli }); T.app.status(); }
      Stream.lastMsg = performance.now();
    } catch (e) { Stream.setLink('off'); }
  };
  Stream.pollT = setInterval(step, 125); step();
};
