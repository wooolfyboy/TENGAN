// ---- GALLERY › WEATHER: радиопогода — эфир вокруг как небо над городом ----
// Палка раз в секунду шлёт «сводку» (событие wx): облачность = сколько устройств рядом,
// ветер = как быстро они сменяются, дождь = всплески на 433 МГц, гроза = громкие звуки.
// Красота важнее точности: всё плавно перетекает (S.wxs), молния — на каждый новый раскат.
// Ночное небо HUD со свечением.
(() => {
  const V = T.views.find(v => v.id === 'gal');
  const NAMES = {
    calm: ['CALM', 'штиль', '静'], clear: ['CLEAR', 'ясно', '晴'], cloudy: ['CLOUDY', 'облачно', '曇'],
    overcast: ['OVERCAST', 'пасмурно', '曇'], windy: ['WINDY', 'ветрено', '風'], rain: ['RAIN', 'дождь', '雨'], storm: ['STORM', 'гроза', '雷'],
  };
  const rnd = (s => () => { s = (s * 16807) % 2147483647; return s / 2147483647; })(9);

  // ---- облака: несколько готовых текстур (рисуются один раз), дальше их только двигаем ----
  const cloudTex = {};
  const makeCloud = seed => {
    const W = 512, H = 220, c = document.createElement('canvas'); c.width = W; c.height = H;
    const g = c.getContext('2d'), r = (s => () => { s = (s * 16807) % 2147483647; return s / 2147483647; })(seed * 7919 + 13);
    const puffs = 26;
    for (let i = 0; i < puffs; i++) {
      const k = i / puffs, x = W * (0.12 + 0.76 * r()), y = H * (0.42 + 0.28 * Math.sin(k * Math.PI) * (r() - 0.2)), rr = H * (0.16 + 0.24 * r()) * (0.6 + 0.4 * Math.sin(Math.PI * (x / W)));
      const gr = g.createRadialGradient(x, y - rr * 0.2, 0, x, y, rr);
      gr.addColorStop(0, 'rgba(150,185,205,.22)'); gr.addColorStop(0.55, 'rgba(60,95,120,.12)'); gr.addColorStop(1, 'rgba(20,40,54,0)');
      g.fillStyle = gr; g.beginPath(); g.arc(x, y, rr, 0, T.TAU); g.fill();
    }
    {                                                         // подсвеченный край сверху
      const gr = g.createLinearGradient(0, H * 0.2, 0, H * 0.55); gr.addColorStop(0, 'rgba(62,230,255,.10)'); gr.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = gr; g.globalCompositeOperation = 'source-atop'; g.fillRect(0, 0, W, H); g.globalCompositeOperation = 'source-over';
    }
    return c;
  };
  const tex = i => cloudTex[i] || (cloudTex[i] = makeCloud(i + 1));

  // ---- состояние сцены ----
  const N_CLOUD = 16, N_RAIN = 420, N_STAR = 220, N_CITY = 46;
  const clouds = Array.from({ length: N_CLOUD }, (_, i) => ({ x: rnd(), y: rnd(), s: 0.6 + rnd() * 0.9, tex: i % 6, layer: i % 3, a: 0, sp: 0.6 + rnd() * 0.8 }));
  const rain = Array.from({ length: N_RAIN }, () => ({ x: rnd(), y: rnd(), l: 0.4 + rnd() * 0.6, v: 0.8 + rnd() * 0.5 }));
  const stars = Array.from({ length: N_STAR }, () => ({ x: rnd(), y: rnd() * 0.9, m: rnd(), tw: rnd() * 6 }));
  const city = (() => { const b = []; let x = 0; while (x < 1) { const w = 0.01 + rnd() * 0.035, h = 0.02 + Math.pow(rnd(), 2) * 0.09; b.push({ x, w, h, win: rnd() }); x += w + rnd() * 0.004; } return b; })();
  const W8 = { flash: 0, bolt: null, boltSeen: 0, boltT: 0, cityC: null, cityKey: '' };

  // молния: ломаная от облаков к горизонту
  const makeBolt = (w, top, bot) => {
    const pts = []; let x = w * (0.2 + 0.6 * Math.random()), y = top;
    pts.push([x, y]);
    while (y < bot) { y += (bot - top) * (0.05 + Math.random() * 0.07); x += (Math.random() - 0.5) * (bot - top) * 0.12; pts.push([x, Math.min(y, bot)]); }
    const branch = pts.slice(2 + (Math.random() * 3 | 0)); let bx = branch[0] ? branch[0][0] : x, by = branch[0] ? branch[0][1] : y; const br = [[bx, by]];
    for (let i = 0; i < 4; i++) { bx += (Math.random() - 0.3) * (bot - top) * 0.1; by += (bot - top) * 0.06; br.push([bx, by]); }
    return { pts, br };
  };

  // ---- WebGL: небо, облака (fbm, три слоя, едут по ветру), дождь, вспышка — одним шейдером ----
  const WXFS = `uniform vec2 res; uniform vec2 css; uniform float t, wx, clouds, rain, storm, flash, night, slant, u, trend; uniform vec3 sun;
  ` + T.GL.NOISE + `
  vec2 fragCss(){ return vec2(gl_FragCoord.x, res.y-gl_FragCoord.y)/res*css; }
  void main(){
    vec2 p=fragCss(); float H=css.y, y=p.y/H;                 // 0 — верх, 1 — горизонт
    vec2 q=p/H;
    vec3 col; float dark=1.-storm*.5;
    col=mix(vec3(.02,.03,.05),vec3(.055,.13,.19)*dark,y*y);
    // светило: ночью луна, днём — мягкое свечение
    float sd=length(p-sun.xy);
    col+=vec3(.7,.9,1.)*(smoothstep(sun.z*.8,sun.z*.75,sd)*.85*night + exp(-sd/(sun.z*3.))*.10);
    // звёзды сквозь просветы
    vec2 sc=floor(p/(u*1.7)); float hs=hash(sc), hs2=hash(sc+3.1);
    float star=step(.93,hs)*smoothstep(.10,.0,length(fract(p/(u*1.7))-.5-(vec2(hs,hs2)-.5)*.6))*(.55+.45*sin(t*1.3+hs*60.))*(.4+(hs-.93)*8.6)*smoothstep(.85,.4,y);
    // облака: три слоя, ближние крупнее и быстрее
    float cover=0., light=0.;
    for(int i=0;i<3;i++){
      float fi=float(i), sc2=1.6-fi*.45;
      vec2 uv=q*vec2(sc2*1.4,sc2*2.6)+vec2(wx*(.5+fi*.35)*1.6+fi*13.1, fi*3.7);
      float d=fbm(uv)+.5*fbm(uv*2.3+vec2(t*.015,0.))-.25;
      float thr=mix(.78,.28,clouds)-fi*.03;
      float prof=smoothstep(.0,.15,y)*smoothstep(1.,.45,y-fi*.12);
      float a=smoothstep(thr,thr+.22,d)*prof*(.55+fi*.2);
      float lt=clamp((fbm(uv-vec2(0.,.06))-fbm(uv))*4.+.5,0.,1.);   // верх облака светлее
      vec3 cc=mix(vec3(.05,.09,.12)*dark,vec3(.30,.44,.52)*(.6+.4*dark),lt);
      cc+=vec3(.45,.60,.70)*flash*(.25+.45*lt);
      col=mix(col,cc,a);
      cover=max(cover,a);
    }
    col+=vec3(.92,.98,1.)*star*(1.-cover)*clamp(1.-clouds*1.1,0.,1.)*.8;
    // свечение города у горизонта
    col+=vec3(.15,.6,.75)*exp(-length(vec2((p.x/css.x-.5)*1.4,1.-y))*3.)*(.10+.06*(1.-clouds));
    // дождь: косые штрихи, два слоя
    if(rain>.01){
      for(int k=0;k<2;k++){
        float fk=float(k), cw=u*(.9-fk*.35);
        vec2 r=vec2(p.x+p.y*slant, p.y);
        float cx=floor(r.x/cw), hh=hash(vec2(cx,fk));
        if(hh<rain*.9){
          float sp=(1.3+hh*.8)*(1.+fk*.4), len=(.06+.06*rain)*(1.-fk*.3);
          float ph=fract(y*(1.+fk)-t*sp+hh*7.);
          float s=smoothstep(len,0.,ph)*smoothstep(.0,.02,ph)*smoothstep(.5,.0,abs(fract(r.x/cw)-.5)-.12);
          col+=vec3(.45,.6,.7)*s*(.35+.3*rain)*(1.-fk*.4);
        }
      }
    }
    // вспышка молнии — всё небо
    col+=vec3(.20,.28,.34)*flash*.45;
    gl_FragColor=vec4(col,1.);
  }`;
  let WXP;
  const glSky = (g, w, hz, u, t, s, F, night, sun) => {
    const GL = T.GL; if (!GL || !GL.ok) return false;
    if (WXP === undefined) WXP = GL.prog(WXFS);
    if (!WXP) return false;
    GL.begin(w, hz, 0.5, [0, 0, 0, 1]);
    GL.quad(WXP, { css: [w, hz], t, wx: W8.wx, clouds: s.clouds, rain: s.rain, storm: s.storm, flash: F, night: night ? 1 : 0, slant: 0.15 + s.wind * 0.6, u, trend: s.trend, sun });
    GL.draw(g, 0, 0, w, hz);
    return true;
  };
  W8.wx = 0;

  V.weather = function (g, w, h, L, dt, t) {
    const { u, m, top, bot } = L, d = S.wx, s = S.wxs;
    if (!d || !d.ok) {
      T.emptyState(g, w, h, u, 'WEATHER', 'waiting for the stick');
      return { status: 'radio weather', hint: 'open the Web section on the stick', right: 'wx · stick' };
    }
    const hz = top + (bot - top) * 0.82;                       // горизонт
    const night = (() => { const hh = new Date().getHours(); return hh < 6 || hh >= 20; })();
    // ---- время, ветер, молния ----
    const wind = 0.02 + s.wind * 0.22;
    for (const c of clouds) c.x = (c.x + dt * wind * c.sp * (0.5 + c.layer * 0.35) + 1.4) % 1.4;
    if (S.wxBolt !== W8.boltSeen) { W8.boltSeen = S.wxBolt; W8.flash = 1; W8.bolt = makeBolt(w, top + (hz - top) * 0.25, hz); W8.boltT = 0; }
    else if (s.storm > 0.55 && Math.random() < dt * s.storm * 0.25) { W8.flash = 0.8; W8.bolt = makeBolt(w, top + (hz - top) * 0.25, hz); W8.boltT = 0; }
    W8.flash = Math.max(0, W8.flash - dt * 2.2); W8.boltT += dt;
    const F = W8.flash * (0.6 + 0.4 * Math.sin(W8.boltT * 60));   // мерцание вспышки
    W8.wx += dt * wind;
    const sx0 = w * 0.72, sy0 = top + (hz - top) * 0.32;
    const GLW = glSky(g, w, hz, u, t, s, F, night, [sx0, sy0, u * 2.2]);
    if (!GLW) {

    // ---- небо ----
    {
      const sky = g.createLinearGradient(0, top, 0, hz);
      const dark = 1 - s.storm * 0.5;
      sky.addColorStop(0, `rgba(5,8,13,1)`); sky.addColorStop(1, `rgba(${14 * dark | 0},${34 * dark | 0},${48 * dark | 0},1)`);
      g.fillStyle = sky; g.fillRect(0, 0, w, hz);
      // звёзды — сквозь просветы
      const sv = T.clamp(1 - s.clouds * 1.1);
      if (sv > 0.02) {
        g.fillStyle = `rgba(235,250,255,${0.8 * sv})`; g.beginPath();
        for (const st of stars) { const y = top + (hz - top) * st.y; if (y > hz - u) continue; const tw = 0.6 + 0.4 * Math.sin(t * 1.3 + st.tw); const r = (0.5 + st.m * 1.1) * tw; g.rect(w * st.x - r / 2, y - r / 2, r, r); }
        g.fill();
      }
      // мягкое свечение у горизонта (город)
      const glow = g.createRadialGradient(w / 2, hz, 0, w / 2, hz, w * 0.6);
      glow.addColorStop(0, `rgba(62,230,255,${0.08 + 0.05 * (1 - s.clouds)})`); glow.addColorStop(1, 'rgba(62,230,255,0)');
      g.fillStyle = glow; g.fillRect(0, top, w, hz - top);
    }
    // вспышка подсвечивает всё небо
    if (F > 0.01) { g.fillStyle = `rgba(150,185,205,${0.22 * F})`; g.fillRect(0, 0, w, hz); }

    // ---- облака: три слоя, сколько видно — по облачности ----
    const nVis = s.clouds * N_CLOUD;
    for (let layer = 0; layer < 3; layer++) for (let i = 0; i < N_CLOUD; i++) {
      const c = clouds[i]; if (c.layer !== layer) continue;
      const a = T.clamp(nVis - i) * (0.45 + layer * 0.25) * (0.7 + 0.3 * s.storm);
      if (a < 0.01) continue;
      const cw = w * (0.32 + 0.22 * layer) * c.s, ch = cw * 0.43;
      const x = (c.x - 0.2) * (w + cw) - cw * 0.5, y = top + (hz - top) * (0.05 + 0.5 * c.y * (1 - layer * 0.25)) - ch * 0.3;
      g.globalAlpha = Math.min(1, a);
      g.drawImage(tex(c.tex), x, y, cw, ch);
      if (F > 0.02) { g.globalAlpha = Math.min(1, a) * F * 0.7; g.globalCompositeOperation = 'lighter'; g.drawImage(tex(c.tex), x, y, cw, ch); g.globalCompositeOperation = 'source-over'; }
    }
    g.globalAlpha = 1;
    }

    // ---- молния ----
    if (W8.bolt && W8.boltT < 0.45) {
      const al = 1 - W8.boltT / 0.45, path = pts => { g.beginPath(); pts.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1])); g.stroke(); };
      g.lineJoin = 'round'; g.lineCap = 'round';
      g.strokeStyle = `rgba(62,230,255,${0.5 * al})`; g.lineWidth = u * 0.6; path(W8.bolt.pts);
      g.strokeStyle = `rgba(235,250,255,${al})`; g.lineWidth = Math.max(1.5, u * 0.16); path(W8.bolt.pts);
      g.lineWidth = Math.max(1, u * 0.09); path(W8.bolt.br);
    }

    // ---- дождь: косые штрихи, сколько капель — по дождю, наклон — по ветру ----
    const nRain = GLW ? 0 : Math.round(s.rain * N_RAIN);
    if (nRain > 0) {
      const slant = 0.15 + s.wind * 0.6, len = u * (1.2 + s.rain * 1.6), vy = (bot - top) * (0.9 + s.rain * 0.8);
      g.strokeStyle = `rgba(150,185,205,${0.35 + 0.25 * s.rain})`; g.lineWidth = 1.2; g.lineCap = 'round';
      g.beginPath();
      for (let i = 0; i < nRain; i++) {
        const r = rain[i]; r.y += dt * vy * r.v / (bot - top); if (r.y > 1) { r.y -= 1; r.x = Math.random(); }
        const x = ((r.x + r.y * slant * 0.3) % 1.1) * w, y = top + (hz - top + u * 4) * r.y, l = len * r.l;
        g.moveTo(x, y); g.lineTo(x - slant * l, y + l);
      }
      g.stroke();
    }

    // ---- земля: город, рисуется в слой при смене размера ----
    {
      const key = w + 'x' + h + ':' + Math.round(hz);
      if (W8.cityKey !== key) {
        const c = W8.cityC || (W8.cityC = document.createElement('canvas')), d2 = this.cv.dpr; c.width = Math.round(w * d2); c.height = Math.round((h - hz + u * 8) * d2);
        const x = c.getContext('2d'); x.setTransform(d2, 0, 0, d2, 0, 0); const base = u * 8, H2 = Math.max(h - hz, (hz - top) * 0.16);   // высота домов — от неба, не от земли
        x.fillStyle = '#060b11';
        for (const b of city) x.fillRect(b.x * w, base - Math.min(base, b.h * H2 * 4), b.w * w, Math.min(base, b.h * H2 * 4) + (h - hz));
        x.fillStyle = 'rgba(62,230,255,.55)';                  // окна — точки света
        for (const b of city) { const bh = Math.min(base, b.h * H2 * 4); for (let yy = base - bh + u * 0.5; yy < base - u * 0.3; yy += u * 0.7) for (let xx = b.x * w + u * 0.3; xx < (b.x + b.w) * w - u * 0.3; xx += u * 0.6) if (((xx * 13 + yy * 7) | 0) % 5 === 0) x.fillRect(xx, yy, 1.5, 1.5); }
        x.fillStyle = 'rgba(62,230,255,.6)'; x.fillRect(0, base, w, 1);
        W8.cityC = c; W8.cityKey = key;
      }
      g.fillStyle = '#05080d'; g.fillRect(0, hz, w, h - hz);
      g.drawImage(W8.cityC, 0, hz - u * 8, w, h - hz + u * 8);
      if (F > 0.02) { g.fillStyle = `rgba(150,185,205,${0.12 * F})`; g.fillRect(0, hz, w, h - hz); }
    }

    // ---- HUD (плоский, поверх свечения) ----
    const nm = NAMES[d.cond] || NAMES.calm;
    const draw = () => {
      const x0 = m, y1 = top + u * 1.2;
      T.txt(g, '// ' + nm[0], x0, y1 + u * 3.1, u * 3.6, d.cond === 'storm' ? T.C.alert : T.C.hot, 't');
      T.txt(g, nm[1].toUpperCase(), x0 + u * 0.2, y1 + u * 4.8, u * 1.05, T.C.label, 'c');
      // показания
      const rows = [['CLOUDS', d.ble + ' dev'], ['WIND', (d.turn | 0) + ' /min'], ['RAIN 433', (d.sub | 0) + ' /min' + (d.sub ? '  ' + Math.round(d.subPk) + ' dBm' : '')], ['THUNDER', Math.round(d.pk) + ' dBFS']];
      const ry = y1 + u * 7.6;
      // подложка, чтобы показания читались поверх облаков и дождя; цифры крупно, как на метеостанции
      T.panel(g, x0 - u * 0.8, ry - u * 1.5, u * 22, u * 9.8, u * 0.6, 'rgba(5,8,13,.62)', 'rgba(62,230,255,.25)');
      rows.forEach(([k, v], i) => { T.txt(g, k, x0, ry + i * u * 2.3, u * 0.95, T.C.label); T.txt(g, v, x0 + u * 8, ry + i * u * 2.3 + u * 0.15, u * 1.55, T.C.hot, 't'); });
      // за окном: настоящая температура с датчика 433 (если слышно за 10 мин)
      if (d.temp != null) {
        const oy = ry + u * 9.3;
        T.panel(g, x0 - u * 0.8, oy, u * 22, u * 6.4, u * 0.6, 'rgba(5,8,13,.62)', 'rgba(62,230,255,.25)');
        T.txt(g, 'OUTSIDE · 433', x0, oy + u * 1.7, u * 0.95, T.C.label);
        T.bigNum(g, (d.temp > 0 ? '+' : '') + Number(d.temp).toFixed(1), x0, oy + u * 5.3, u * 3.6, T.C.hot, '°C');
        T.txt(g, (d.hum != null ? d.hum + '%' : ''), x0 + u * 13, oy + u * 3.6, u * 1.55, T.C.hot, 't');
        T.txt(g, String(d.tsrc || '').toUpperCase(), x0 + u * 13, oy + u * 5.1, u * 0.8, T.C.dim);
      }
      // барометр: давление «падает» к буре
      const bx = w - m - u * 5.5, by = y1 + u * 5, br = u * 4.2, p = T.clamp(1 - (s.clouds * 0.45 + s.rain * 0.3 + s.storm * 0.35));
      { T.panel(g, bx - br - u * 1.2, by - br - u * 1.2, (br + u * 1.2) * 2, (br + u * 1.2) * 2 + u * 2.2, u * 0.6); g.strokeStyle = 'rgba(62,230,255,.5)'; g.lineWidth = 1.5; g.beginPath(); g.arc(bx, by, br, Math.PI * 0.8, Math.PI * 2.2); g.stroke(); }
      for (let k = 0; k <= 10; k++) { const a = Math.PI * (0.8 + 1.4 * k / 10), l = k % 5 ? 0.88 : 0.8; g.strokeStyle = 'rgba(90,112,128,.9)'; g.lineWidth = 1; g.beginPath(); g.moveTo(bx + Math.cos(a) * br * l, by + Math.sin(a) * br * l); g.lineTo(bx + Math.cos(a) * br * 0.96, by + Math.sin(a) * br * 0.96); g.stroke(); }
      const na = Math.PI * (0.8 + 1.4 * p);
      g.strokeStyle = T.C.hot; g.lineWidth = 2; g.beginPath(); g.moveTo(bx, by); g.lineTo(bx + Math.cos(na) * br * 0.82, by + Math.sin(na) * br * 0.82); g.stroke();
      T.txt(g, 'STORM', bx - br * 0.95, by + br * 0.75, u * 0.62, T.C.dim, 'm', 'left'); T.txt(g, 'FAIR', bx + br * 0.95, by + br * 0.75, u * 0.62, T.C.dim, 'm', 'right');
      const tr = s.trend, arrow = tr > 0.15 ? '▲ RISING' : tr < -0.15 ? '▼ FALLING' : '◆ STEADY';
      T.txt(g, arrow, bx, by + br + u * 1.4, u * 0.8, tr < -0.15 ? T.C.alert : T.C.label, 'm', 'center');
      // история: облачность — заливка, дождь — столбики
      const hist = d.hist || [];
      if (hist.length > 1) {
        const hw = u * 14, hh = u * 3.2, hx = w - m - hw, hy = by + br + u * 2.8;
        T.txt(g, 'LAST ' + hist.length + ' MIN', hx, hy - u * 0.4, u * 0.62, T.C.dim);
        T.spark(g, hx, hy, hw, hh, hist.map(q => q[0]), 0, 1, T.C.acc);
        g.fillStyle = 'rgba(150,185,205,.55)';
        hist.forEach((q, i) => { if (q[1] > 0.02) { const bw = hw / hist.length; g.fillRect(hx + i * bw, hy + hh - q[1] * hh, Math.max(1, bw * 0.6), q[1] * hh); } });
      }
    };
    return { status: (d.cond || '').toUpperCase() + ' · ' + d.ble + ' DEV', hint: 'radio weather · devices = clouds · 433 = rain · loud = thunder', right: 'wx · stick 1 Hz', draw };
  };
})();
