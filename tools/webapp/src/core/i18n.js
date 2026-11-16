// ---- язык страницы: EN / RU (выбирается при сборке: build.py --lang en|ru) ----
// Исходный текст интерфейса — английский. В русской сборке build.py вставляет словарь
// tools/webapp/i18n/ru.json (T.DICT), а перевод делается в одном месте — при выводе текста:
//  • на холстах — в fillText / strokeText / measureText (поэтому разметка меряет уже русский текст);
//  • в DOM — через T.tr(...) там, где строка вставляется (вкладки, настройки, панели).
// Правила словаря: точное совпадение строки → перевод; иначе — замена известных фраз внутри строки
// (по границам слов, длинные фразы первыми). Регистр сохраняется: «DEVICES» → «УСТРОЙСТВА».
// Результаты кэшируются, так что на кадр это несколько обращений к Map.
(() => {
  T.LANG = /*@LANG*/'en';
  const DICT = /*@DICT*/null;
  T.L = (en, ru) => T.LANG === 'ru' && ru != null ? ru : en;          // явный выбор по месту
  if (!DICT || T.LANG === 'en') { T.tr = s => s; return; }
  document.documentElement.lang = 'ru';
  const low = new Map();                                               // ключ в нижнем регистре -> перевод
  for (const k in DICT) { low.set(k.toLowerCase(), DICT[k]); const t = k.trim(); if (t !== k && !low.has(t.toLowerCase())) low.set(t.toLowerCase(), DICT[k].trim()); }
  const keys = [...low.keys()].filter(k => k.length >= 3).sort((a, b) => b.length - a.length);
  const esc = s => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const RX = keys.length ? new RegExp('(?<![A-Za-z0-9])(' + keys.map(esc).join('|') + ')(?![A-Za-z0-9])', 'gi') : null;
  const isUp = s => s === s.toUpperCase() && s !== s.toLowerCase();
  const isCap = s => s[0] === s[0].toUpperCase() && s.slice(1) === s.slice(1).toLowerCase() && s !== s.toLowerCase();
  const fit = (src, dst) => isUp(src) ? dst.toUpperCase() : isCap(src) ? dst[0].toUpperCase() + dst.slice(1) : dst;
  const cache = new Map();
  T.tr = s => {
    if (typeof s !== 'string' || !s || !/[A-Za-z]/.test(s)) return s;
    let r = cache.get(s);
    if (r !== undefined) return r;
    const exact = DICT[s] ?? low.get(s.toLowerCase());
    if (exact != null) r = DICT[s] != null ? exact : fit(s, exact);
    else r = RX ? s.replace(RX, m => fit(m, low.get(m.toLowerCase()))) : s;
    if (cache.size > 4000) cache.clear();
    cache.set(s, r);
    return r;
  };
  // холсты: перевод при выводе и измерении текста
  const P = CanvasRenderingContext2D.prototype, F = P.fillText, Sx = P.strokeText, M = P.measureText;
  P.fillText = function (s, ...a) { return F.call(this, T.tr(String(s)), ...a); };
  P.strokeText = function (s, ...a) { return Sx.call(this, T.tr(String(s)), ...a); };
  P.measureText = function (s) { return M.call(this, T.tr(String(s))); };
})();
