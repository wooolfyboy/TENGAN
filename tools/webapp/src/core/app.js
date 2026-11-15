// ---- приложение: вкладки, подвкладки, цикл кадров, полный экран ----
const App = T.app = { cur: null };
const nav = document.getElementById('nav'), subs = document.getElementById('subs'), main = document.getElementById('main');
App.status = () => {
  const s = S.st; if (!s) return;
  document.getElementById('bat').textContent = s.bat >= 0 ? 'BAT ' + s.bat + '%' + (s.usb ? ' ⚡' : '') : '';
  const r = s.radio || '', names = [['b', 'BLE'], ['w', 'WIFI'], ['s', 'SUB'], ['m', 'MIC'], ['p', 'PC']];
  document.getElementById('radios').innerHTML = names.map(([k, n]) => `<i class="${r.includes(k) ? 'on' : ''}">${n}</i>`).join('');
};

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
// разделы живут в памяти планшета: при уходе сцена прячется, а не уничтожается — при возврате
// город (и любой раздел) показывается сразу, со всем, что уже вырос/накопил. Держим KEEP последних,
// самый старый освобождаем, чтобы не забивать память.
const KEEP = 3, alive = [];
App.go = id => {
  const v = T.views.find(x => x.id === id) || T.views[0];
  if (App.cur === v) return;
  if (App.cur && App.cur.root) App.cur.root.hidden = true;
  App.cur = v;
  const i = alive.indexOf(v); if (i >= 0) alive.splice(i, 1);
  alive.push(v);
  while (alive.length > KEEP) { const old = alive.shift(); old.unmount(); old.root = null; }
  if (i < 0) {                                         // новый (или выгруженный) — подставить сохранённую вкладку
    try { const s = localStorage.getItem('sub_' + v.id); if (s != null && v.subs) v.sub = Math.min(+s, v.subs.length - 1); } catch (e) {}
  }
  try { localStorage.setItem('view', id); } catch (e) {}
  [...nav.children].forEach(b => b.classList.toggle('on', b.dataset.id === v.id));
  const w = document.getElementById('wipe'); w.classList.remove('go'); void w.offsetWidth; w.classList.add('go');
  App.renderSubs();
  if (v.root) { v.root.hidden = false; v.cv && v.cv.fit(); } else v.mount(main);
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
// ---- стабильный режим планшета: не больше 60 кадров, картинка ~1920 по длинной стороне ----
// Свечение — единственный тяжёлый эффект: если 3 с подряд меньше 45 кадров, оно гаснет
// (страховка; на нормальном планшете не срабатывает).
const FPS_CAP = 60, STEP = 1000 / FPS_CAP;
const SHOT = new URLSearchParams(location.search).has('shot');   // для картинок в README: все эффекты, без счётчика кадров
if (SHOT) document.getElementById('fps').style.display = 'none';
let last = performance.now(), next = last, fpsN = 0, fpsT = 0, slow = 0;
const loop = now => {
  requestAnimationFrame(loop);
  if (now < next - 1) return;                          // экран 120/144 Гц: лишние кадры пропускаем
  next = Math.max(next + STEP, now - STEP);            // отстали — не догоняем пачкой
  const dt = Math.min(0.1, (now - last) / 1000); last = now;
  S.tick(dt);
  if (App.cur && !App.hold) App.cur.frame(dt, now / 1000);   // во время заставки сцена ждёт (данные идут)
  fpsN++; fpsT += dt;
  if (fpsT >= 1) {
    const fps = Math.round(fpsN / fpsT); T.fps = fps;
    if (!document.hidden && !SHOT) { slow = fps < 45 ? slow + 1 : 0; if (slow >= 3 && T.Q.bloom) { T.Q.bloom = 0; T.Q.scan = 0; } }
    document.getElementById('fps').textContent = fps + ' FPS' + (T.Q.bloom ? '' : ' · LITE');
    fpsN = 0; fpsT = 0;
  }
  if (T.Stream.mode === 'live' && now - T.Stream.lastMsg > 6000) T.Stream.setLink('off');
};
// старт (boot.js): сначала «запуск» со шкалой — связь, радио раздела (по умолчанию Bluetooth),
// данные, сцена; потом красивая заставка; потом раздел
let first = 'ble'; try { first = localStorage.getItem('view') || 'ble'; } catch (e) {}
if (T.boot) T.boot(first); else App.go(first);
requestAnimationFrame(loop);
