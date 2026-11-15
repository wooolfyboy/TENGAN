// ---- приложение: вкладки, подвкладки, цикл кадров, полный экран ----
const App = T.app = { cur: null };
const nav = document.getElementById('nav'), subs = document.getElementById('subs'), main = document.getElementById('main');
App.status = () => {
  const s = S.st; if (!s) return;
  document.getElementById('bat').textContent = s.bat >= 0 ? 'BAT ' + s.bat + '%' + (s.usb ? ' ⚡' : '') : '';
  const r = s.radio || '', names = [['b', 'BLE'], ['w', 'WIFI'], ['s', 'SUB'], ['m', 'MIC'], ['p', 'PC']];
  document.getElementById('radios').innerHTML = names.map(([k, n]) => `<i class="${r.includes(k) ? 'on' : ''}">${n}</i>`).join('');
};
// печать 天眼 в шапке — из прошивки
(() => { const c = document.getElementById('seal'), im = T.sealImg(60, T.C.acc); if (im) { c.width = im.width; c.height = im.height; c.getContext('2d').drawImage(im, 0, 0); } })();
setInterval(() => { const d = new Date(); document.getElementById('clock').textContent = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0'); }, 1000);
App.renderSubs = () => {
  const v = App.cur; subs.innerHTML = '';
  document.body.classList.toggle('hassubs', !!(v.subs && v.subs.length));
  (v.subs || []).forEach((n, i) => {
    const b = T.el('button', i === v.sub ? 'on' : '', n);
    b.onclick = () => { v.sub = i; v.onSub && v.onSub(i); App.renderSubs(); try { localStorage.setItem('sub_' + v.id, i); } catch (e) {} App.rewant(); };
    subs.appendChild(b);
  });
};
App.go = id => {
  const v = T.views.find(x => x.id === id) || T.views[0];
  if (App.cur) App.cur.unmount();
  App.cur = v;
  try { const s = localStorage.getItem('sub_' + v.id); if (s != null && v.subs) v.sub = Math.min(+s, v.subs.length - 1); localStorage.setItem('view', id); } catch (e) {}
  [...nav.children].forEach(b => b.classList.toggle('on', b.dataset.id === v.id));
  const w = document.getElementById('wipe'); w.classList.remove('go'); void w.offsetWidth; w.classList.add('go');
  App.renderSubs();
  v.mount(main);
  App.rewant();
};
// подписка на потоки: вид может менять её на лету (want — строка или функция)
App.rewant = () => {
  const v = App.cur; if (!v) return;
  const want = (typeof v.want === 'function' ? v.want() : v.want) || 'st';   // галерее палка не нужна — только статус
  if (want !== T.Stream.want) T.Stream.connect(want);
};
T.views.forEach(v => { const b = T.el('button', '', v.title); b.dataset.id = v.id; b.onclick = () => App.go(v.id); nav.appendChild(b); });
document.getElementById('fs').onclick = () => {
  const d = document.documentElement;
  if (document.fullscreenElement) document.exitFullscreen(); else (d.requestFullscreen || d.webkitRequestFullscreen).call(d);
};
// цикл кадров: рисует планшет, данные «перетекают» между пакетами
let last = performance.now(), fpsN = 0, fpsT = 0;
const loop = now => {
  const dt = Math.min(0.1, (now - last) / 1000); last = now;
  S.tick(dt);
  if (App.cur) App.cur.frame(dt, now / 1000);
  fpsN++; fpsT += dt;
  if (fpsT >= 1) { document.getElementById('fps').textContent = fpsN + ' FPS'; T.fps = fpsN; fpsN = 0; fpsT = 0; }
  if (T.Stream.mode === 'live' && now - T.Stream.lastMsg > 6000) T.Stream.setLink('off');
  requestAnimationFrame(loop);
};
let first = 'ov'; try { first = localStorage.getItem('view') || 'ov'; } catch (e) {}
App.go(first);
T.boot && T.boot();
requestAnimationFrame(loop);
