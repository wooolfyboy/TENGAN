// ---- GALLERY: чистая математика — считает только планшет ----
// Tesseract · Torus · Life · Pendulum — те же формулы, что раньше, в версии B:
// свечение, глубина, фосфорный след. Sky и Globe — небо и Земля прямо сейчас
// (T.astro: та же математика, что в прошивке; город и орбиты присылает палка).
// ---- WebGL-версии сцен галереи (core/gl.js). Нет WebGL — рисуется прежняя 2D-версия ----
// Каждая сцена: шейдер рисует «тяжёлое» (сфера, небо, свечение), HUD и подписи — 2D поверх.
T.galGL = (() => {
  const G = {}, GL = T.GL;
  G.on = () => GL && GL.ok;
  const progs = {};
  const prog = (k, fs) => (k in progs ? progs[k] : (progs[k] = GL.prog(fs)));
  // кадр: область (x, y, w, h) в CSS-px вида; rendered → на 2D-холст
  const run = (g, x, y, w, h, scale, k, fs, vals, clear) => {
    const P = prog(k, fs); if (!P) return false;
    GL.begin(w, h, scale, clear);
    vals.css = [w, h];
    GL.quad(P, vals);
    GL.draw(g, x, y, w, h);
    return true;
  };
  const FRAG = `uniform vec2 res; uniform vec2 css; vec2 fragCss(){ return vec2(gl_FragCoord.x, res.y-gl_FragCoord.y)/res*css; }`;

  // ---------- карта суши (равнопромежуточная, 1024×512): контуры из прошивки + грубые материки, которых в ней нет ----
  const EXTRA = [
    // Северная Америка
    [[71,-156],[70,-145],[69,-135],[70,-125],[68,-110],[67,-98],[69,-90],[66,-85],[62,-94],[58,-94],[55,-82],[52,-79],[56,-77],[60,-78],[62,-73],[60,-65],[55,-60],[52,-56],[47,-53],[47,-60],[45,-61],[44,-66],[42,-70],[41,-72],[39,-74],[37,-76],[35,-76],[32,-81],[30,-81],[27,-80],[25,-80],[27,-82],[30,-84],[30,-89],[29,-94],[26,-97],[22,-98],[19,-96],[18,-94],[19,-91],[21,-87],[18,-88],[16,-88],[15,-84],[11,-84],[9,-79],[8,-77],[7,-78],[8,-80],[10,-85],[13,-88],[15,-93],[16,-96],[18,-103],[21,-105],[23,-106],[26,-109],[28,-112],[31,-113],[29,-114],[24,-110],[23,-110],[26,-112],[30,-115],[32,-117],[34,-120],[38,-123],[42,-124],[46,-124],[48,-125],[51,-128],[55,-131],[58,-136],[60,-145],[60,-150],[57,-157],[55,-162],[58,-158],[60,-164],[63,-166],[65,-168],[68,-166],[71,-156]],
    // Канадский архипелаг (грубо)
    [[73,-120],[76,-118],[78,-105],[76,-90],[74,-80],[72,-78],[70,-85],[71,-100],[73,-120]],
    [[80,-90],[83,-75],[82,-62],[79,-70],[77,-80],[80,-90]],
    // Южная Америка
    [[12,-72],[11,-64],[8,-60],[5,-52],[1,-50],[-2,-44],[-5,-36],[-8,-35],[-13,-39],[-18,-39],[-23,-42],[-25,-48],[-29,-49],[-34,-53],[-36,-57],[-39,-62],[-42,-64],[-46,-67],[-50,-69],[-54,-68],[-55,-66],[-54,-71],[-50,-75],[-45,-74],[-40,-73],[-35,-72],[-30,-71],[-25,-70],[-18,-70],[-14,-76],[-6,-81],[-2,-81],[2,-79],[6,-77],[9,-77],[11,-75],[12,-72]],
    // Гренландия, Исландия
    [[83,-35],[82,-20],[77,-18],[73,-22],[70,-22],[68,-30],[65,-38],[60,-43],[61,-48],[65,-52],[69,-51],[72,-55],[76,-60],[78,-72],[81,-65],[83,-35]],
    [[66,-23],[66,-14],[64,-14],[63,-20],[64,-23],[66,-23]],
    // Австралия, Новая Зеландия, Новая Гвинея, Борнео, Суматра, Мадагаскар, Япония, Британия
    [[-11,142],[-17,141],[-15,136],[-12,136],[-12,131],[-15,129],[-14,126],[-17,122],[-20,119],[-22,114],[-26,113],[-31,115],[-34,115],[-35,118],[-32,126],[-32,133],[-35,136],[-35,138],[-38,140],[-38,146],[-37,150],[-33,152],[-28,153],[-24,152],[-19,147],[-15,145],[-11,142]],
    [[-35,173],[-38,178],[-41,175],[-46,170],[-44,168],[-40,173],[-35,173]],
    [[-1,131],[-3,141],[-8,147],[-10,150],[-8,143],[-6,138],[-4,133],[-1,131]],
    [[7,117],[1,119],[-4,116],[-3,111],[1,109],[4,113],[7,117]],
    [[5,95],[-1,101],[-6,105],[-4,102],[1,98],[5,95]],
    [[-12,49],[-16,50],[-25,47],[-25,44],[-21,43],[-16,44],[-12,49]],
    [[45,142],[43,145],[41,141],[38,141],[35,140],[34,136],[33,130],[35,133],[37,137],[39,140],[41,140],[45,142]],
    [[58,-5],[58,-3],[55,-1.5],[53,0],[51,1],[50,-5],[52,-4],[54,-3],[55,-5],[58,-5]],
    // Антарктида
    [[-63,-58],[-66,-62],[-70,-75],[-72,-100],[-74,-130],[-77,-160],[-78,-180],[-90,-180],[-90,180],[-78,180],[-72,170],[-68,150],[-66,120],[-66,90],[-67,60],[-69,30],[-70,0],[-72,-15],[-75,-30],[-77,-45],[-72,-60],[-63,-58]],
  ];
  G.land = () => {
    if (G._land) return G._land;
    const W = 1024, H = 512, c = document.createElement('canvas'); c.width = W; c.height = H;
    const g = c.getContext('2d');
    g.fillStyle = '#000'; g.fillRect(0, 0, W, H); g.fillStyle = '#fff';
    const polys = T.astro.data().land.filter(l => l.length >= 3).concat(EXTRA);
    for (const l of polys) {
      // долгота без разрыва на ±180 (рисуем трижды: -360, 0, +360)
      const pts = []; let prev = null;
      for (const q of l) { let lo = q[1]; if (prev != null) { while (lo - prev > 180) lo -= 360; while (prev - lo > 180) lo += 360; } pts.push([q[0], lo]); prev = lo; }
      for (const off of [-360, 0, 360]) {
        g.beginPath(); pts.forEach((q, i) => { const x = (q[1] + off + 180) / 360 * W, y = (90 - q[0]) / 180 * H; i ? g.lineTo(x, y) : g.moveTo(x, y); }); g.closePath(); g.fill();
      }
    }
    // мягкий берег: шейдер берёт порог по размытой маске + шум — берег «живой», без ломаной
    const b = document.createElement('canvas'); b.width = W; b.height = H; const bg = b.getContext('2d');
    bg.filter = 'blur(3px)'; bg.drawImage(c, 0, 0);
    return (G._land = b);
  };

  // ---------- ЗЕМЛЯ ----------
  const GLOBE = FRAG + T.GL.NOISE + `
  uniform float t, l0, b0; uniform vec3 sun; uniform sampler2D land;
  const float PI=3.14159265;
  void main(){
    vec2 q=(gl_FragCoord.xy/res*2.-1.)*1.22; float r=length(q);
    if(r>1.){
      float d=r-1., s=dot(normalize(q),normalize(sun.xy+1e-4));
      float day=smoothstep(-.6,.6,s+sun.z*.4);
      float a=(exp(-d*16.)*(.25+.75*day)+exp(-d*60.)*.5*day)*smoothstep(.22,.12,d);
      vec3 c=mix(vec3(.10,.45,.75),vec3(.25,.9,1.),day)*a; gl_FragColor=vec4(c,clamp(a,0.,1.)); return;
    }
    vec3 n=vec3(q,sqrt(1.-r*r));
    float cb=cos(b0), sb=sin(b0);
    float c=cb*n.y+sb*n.z, a=-sb*n.y+cb*n.z, b=n.x;
    float lat=asin(clamp(c,-1.,1.)), lon=atan(b,a)+l0;
    vec2 uv=vec2(fract(lon/(2.*PI)+.5), .5-lat/PI);
    float m0=texture2D(land,uv).r;
    float det=fbm(uv*vec2(48.,24.));
    float m=smoothstep(.42,.58,m0+(det-.5)*.32);
    float lit=dot(n,normalize(sun));
    float day=smoothstep(-.10,.14,lit);
    float term=exp(-abs(lit)*22.);
    float limb=.55+.45*n.z;
    float cl=smoothstep(.55,.85,fbm(uv*vec2(14.,9.)+vec2(t*.004,0.))+.08*sin(lat*3.));   // облака
    vec3 col; float al=smoothstep(1.,.992,r);
    vec3 ocean=mix(vec3(.01,.05,.09),vec3(.02,.14,.22),day);
    vec3 grd=mix(vec3(.03,.10,.12),mix(vec3(.08,.32,.36),vec3(.30,.70,.72),det),day);
    col=mix(ocean,grd,m)*limb;
    float spec=pow(max(dot(reflect(-normalize(sun),n),vec3(0,0,1)),0.),36.)*(1.-m)*day;
    col+=vec3(.55,.9,1.)*spec*.7;
    // огни ночной стороны: суша, крупные пятна + мелкие точки
    vec2 cell=floor(uv*vec2(900.,450.)); float h=hash(cell);
    float city=step(.93,h)*smoothstep(.55,.8,fbm(uv*vec2(60.,30.)+3.))*m;
    col+=vec3(.55,.95,1.)*city*(1.-day)*(.6+.4*sin(t*2.+h*40.));
    col+=vec3(.25,.75,.85)*smoothstep(.62,.9,fbm(uv*vec2(30.,15.)+7.))*m*(1.-day)*.18;
    col+=vec3(.20,.75,.95)*term*.35;                       // полоса рассвета
    col=mix(col,vec3(.55,.75,.85)*(.25+.75*day),cl*.45);
    col+=vec3(.15,.6,.9)*pow(1.-n.z,3.)*(.3+.7*day);       // атмосфера по краю
    gl_FragColor=vec4(col*al,al);
  }`;
  G.globe = (g, cx, cy, R, l0, b0, sun, t) => {
    if (!G.on()) return false;
    const S2 = R * 1.22, land = GL.tex('gB_land', G._landUp ? null : G.land()); G._landUp = 1;
    return run(g, cx - S2, cy - S2, S2 * 2, S2 * 2, 0.8, 'globe', GLOBE, { t, l0, b0, sun, land }, [0, 0, 0, 0]);
  };

  // ---------- НЕБО: купол со звёздным полем и Млечным Путём (настоящие звёзды и планеты — 2D поверх) ----------
  const SKY = FRAG + T.GL.NOISE + `
  uniform float t, rot, lat, lst, sunAlt, sunAz, night, day;
  const float PI=3.14159265, D2R=.0174533;
  void main(){
    vec2 p=gl_FragCoord.xy/res*2.-1.; float r=length(p);
    if(r>1.){ gl_FragColor=vec4(0.); return; }
    float az=atan(-p.x,p.y)+rot, alt=PI*.5-2.*atan(r);
    float sl=sin(lat), cl=cos(lat);
    float sd=sin(alt)*sl+cos(alt)*cl*cos(az), dec=asin(clamp(sd,-1.,1.));
    float ha=atan(-sin(az)*cos(alt), sin(alt)*cl-cos(alt)*sl*cos(az)), ra=lst-ha;
    vec3 dir=vec3(cos(dec)*cos(ra),cos(dec)*sin(ra),sin(dec));
    // галактика: широта b и близость к центру Галактики (Стрелец)
    float gp=27.128*D2R, gr=192.859*D2R, cr=266.405*D2R, cd=-28.936*D2R;
    float sb=sin(dec)*sin(gp)+cos(dec)*cos(gp)*cos(ra-gr);
    float cgc=sin(dec)*sin(cd)+cos(dec)*cos(cd)*cos(ra-cr);
    float bb=asin(clamp(sb,-1.,1.));
    float f=fbm3(dir*5.), f2=fbm3(dir*14.+3.);
    float band=exp(-pow(bb/(.16+.06*f),2.))*(.35+.65*smoothstep(-.6,1.,cgc))*(.5+.9*f);
    float rift=exp(-pow((bb+.02)/.035,2.))*smoothstep(.3,.9,cgc)*smoothstep(.4,.7,f2);
    band=max(0.,band-rift*.55);
    // фоновые звёзды (слабые, мерцают)
    vec2 sp=vec2(ra*cos(dec),dec)*58.; vec2 id=floor(sp), fr=fract(sp)-.5;
    float h=hash(id), h2=hash(id+7.3);
    vec2 off=vec2(h,h2)-.5; float sd2=length(fr-off*.6);
    float star=step(.82,h)*smoothstep(.09,.0,sd2)*(.5+.5*sin(t*(1.5+h2*3.)+h*30.))*(h-.82)*5.;
    float ext=smoothstep(0.,.25,alt);                       // у горизонта гаснет
    float tw=smoothstep(.2,.9,1.-r*r);
    vec3 sky0=mix(mix(vec3(.04,.12,.17),vec3(.06,.20,.28),day),vec3(.02,.05,.09),night);
    vec3 sky1=mix(mix(vec3(.02,.07,.11),vec3(.03,.10,.15),day),vec3(.01,.03,.06),night);
    vec3 col=mix(sky1,sky0,r*r);
    // сумерки: зарево со стороны Солнца
    float da=abs(mod(az-sunAz+PI,2.*PI)-PI);
    float tw2=clamp(1.-abs(sunAlt*57.3+5.)/13.,0.,1.);
    col+=vec3(.15,.55,.75)*tw2*exp(-da*1.4)*smoothstep(.2,1.,r)*.55;
    col+=vec3(.35,.55,.75)*band*.42*night*ext + vec3(.6,.75,.85)*pow(band,3.)*.25*night*ext;
    col+=vec3(.75,.92,1.)*star*night*ext;
    gl_FragColor=vec4(col,1.);
  }`;
  G.sky = (g, cx, cy, R, o) => {
    if (!G.on()) return false;
    return run(g, cx - R, cy - R, R * 2, R * 2, 0.7, 'sky', SKY, Object.assign({}, o), [0, 0, 0, 0]);
  };

  // ---------- ТЕССЕРАКТ: 32 ребра как светящиеся трубки (расстояние до отрезков — в шейдере) ----------
  const TESS = FRAG + T.GL.NOISE + `
  uniform float u; uniform vec4 A[32]; uniform vec4 B[32];
  float seg(vec2 p, vec2 a, vec2 b){ vec2 pa=p-a, ba=b-a; float h=clamp(dot(pa,ba)/max(dot(ba,ba),1e-3),0.,1.); return length(pa-ba*h); }
  void main(){
    vec2 p=fragCss(); vec3 col=vec3(0.);
    for(int i=0;i<32;i++){
      float d=seg(p,A[i].xy,B[i].xy), v=A[i].z;
      float wc=u*(.08+.10*v);
      vec3 c=mix(vec3(.10,.45,.60),vec3(.75,.97,1.),v);
      col+=c*(smoothstep(wc,0.,d)*1.2 + exp(-d/(u*.9))*.10*(.4+v) + .9/(1.+pow(d/(u*.35),2.))*.05);
    }
    col=1.-exp(-col*1.3); float a=max(col.r,max(col.g,col.b));
    gl_FragColor=vec4(col,a);
  }`;
  const tA = new Float32Array(128), tB = new Float32Array(128);
  G.tess = (g, w, h, P, u) => {
    if (!G.on()) return false;
    let k = 0;
    for (let i = 0; i < 16; i++) for (let b = 0; b < 4; b++) {
      const j = i ^ (1 << b); if (j < i) continue;
      const v = T.clamp(0.45 + (P[i][2] + P[j][2]) / 2 * 0.35);
      tA.set([P[i][0], P[i][1], v, 0], k * 4); tB.set([P[j][0], P[j][1], v, 0], k * 4); k++;
    }
    return run(g, 0, 0, w, h, 0.6, 'tess', TESS, { u, A: tA, B: tB }, [0, 0, 0, 0]);
  };

  // ---------- ТОР: трассировка лучей, сетка «точек» по поверхности, френель и отражение ----------
  const TORUS = FRAG + T.GL.NOISE + `
  uniform float Aa, Bb, K1, K2; uniform vec2 cen; uniform float t;
  float sdT(vec3 p){ vec2 q=vec2(length(p.xz)-2.1,p.y); return length(q)-1.; }
  mat3 rx(float a){float c=cos(a),s=sin(a);return mat3(1,0,0,0,c,s,0,-s,c);}
  mat3 rz(float a){float c=cos(a),s=sin(a);return mat3(c,s,0,-s,c,0,0,0,1);}
  void main(){
    vec2 p=fragCss(); vec2 d2=(p-cen)/K1; d2.y=-d2.y;
    vec3 ro=vec3(0.), rd=normalize(vec3(d2,1.));
    mat3 M=rz(Bb)*rx(Aa), Mi=rx(-Aa)*rz(-Bb);     // мир -> тело тора
    vec3 c0=vec3(0.,0.,K2);
    float tt=K2-3.4, md=9.;
    vec3 q; bool hit=false;
    for(int i=0;i<80;i++){
      q=Mi*(ro+rd*tt-c0);
      float d=sdT(q); md=min(md,d);
      if(d<.002){hit=true;break;}
      tt+=d*.9; if(tt>K2+3.6)break;
    }
    if(!hit){
      float g=exp(-md*5.)*.22; vec3 c=vec3(.15,.6,.75)*g; gl_FragColor=vec4(c,g); return;
    }
    vec2 e=vec2(.002,0.);
    vec3 n=normalize(vec3(sdT(q+e.xyy)-sdT(q-e.xyy),sdT(q+e.yxy)-sdT(q-e.yxy),sdT(q+e.yyx)-sdT(q-e.yyx)));
    vec3 nw=M*n;                                  // нормаль в мире
    vec3 L=normalize(vec3(0.,.7071,-.7071));
    float lam=max(0.,dot(nw,L)), fres=pow(1.-max(0.,dot(nw,-rd)),3.);
    float th=atan(q.y,length(q.xz)-2.1), ph=atan(q.z,q.x);
    vec2 g=vec2(ph*120./6.2832+th*.95, th*30./6.2832);   // те же 120×30 «точек»
    vec2 f=fract(g)-.5; float dot0=smoothstep(.36,.14,length(f));
    float depth=clamp((K2+3.2-tt)/6.4,0.,1.);
    vec3 base=mix(vec3(.02,.08,.11),vec3(.05,.22,.28),lam);
    vec3 r=reflect(rd,nw); float env=smoothstep(.0,.08,r.y)*.25+exp(-abs(r.y)*18.)*.6;   // «горизонт» в отражении
    vec3 hot=mix(vec3(.15,.55,.7),vec3(.85,1.,1.),lam*lam);
    vec3 col=base+vec3(.2,.75,.9)*env*.35+hot*dot0*(.25+1.2*lam)*(.4+.6*depth)+vec3(.25,.85,1.)*fres*.8;
    gl_FragColor=vec4(col,1.);
  }`;
  G.torus = (g, w, h, o) => {
    if (!G.on()) return false;
    return run(g, 0, 0, w, h, 0.7, 'torus', TORUS, Object.assign({}, o), [0, 0, 0, 0]);
  };
  return G;
})();

T.views.push({
  id: 'gal', title: 'Gallery', subs: ['Tesseract', 'Torus', 'Life', 'Pendulum', 'Sky', 'Globe', 'Weather'], sub: 0,
  want() { return this.sub === 6 ? 'wx' : 'st'; },   // радиопогоде нужна палка, остальное считает планшет
  mount(main) {
    this.root = T.el('div', 'view');
    const st = T.el('div', 'stage'); this.root.appendChild(st); this.cv = T.canvas(st);
    main.appendChild(this.root);
    this.speed = 1; this.ph = 0; this.trail = []; this.trailT = 0; this.pd = null; this.life = null;
    this.dust = T.dust(160, 1.8, 1.4); this.dcam = T.orbit(0.3, 0.35, 3.4);
    this.skyRot = 0; this.skyRotT = 0; this.card = null;
    this.gl = { lon0: 30, lat0: 25, follow: -1, last: 0 };          // follow: -1 сами крутим, -2 дом, 0.. спутник
    const cv = this.cv.cv; let down = null;
    cv.addEventListener('pointerdown', e => { down = { x: e.offsetX, y: e.offsetY, moved: false, rot: this.skyRotT, lon: this.gl.lon0, lat: this.gl.lat0 }; cv.setPointerCapture(e.pointerId); });
    cv.addEventListener('pointermove', e => {
      if (!down) return;
      const dx = e.offsetX - down.x, dy = e.offsetY - down.y;
      if (Math.abs(dx) + Math.abs(dy) > 7) down.moved = true;
      if (!down.moved) return;
      if (this.sub === 4) { this.skyRotT = down.rot - dx * 0.25; this.skyRot = this.skyRotT; }
      if (this.sub === 5) { this.gl.follow = -1; this.gl.lon0 = down.lon - dx * 0.3; this.gl.lat0 = T.clamp(down.lat + dy * 0.3, -70, 70); this.gl.last = performance.now(); }
    });
    const up = e => { if (!down) return; const d = down; down = null; if (!d.moved && e.type === 'pointerup') this.tap(e.offsetX, e.offsetY); };
    cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
  },
  unmount() { this.cv.dispose(); this.root.remove(); },
  onSub() { this.trail = []; this.pd = null; this.card = null; },
  tap(x, y) {
    const hit = T.hits.at(x, y);
    if (hit && hit.act) {
      if (hit.act === '-') this.speed = Math.max(0.25, +(this.speed - 0.25).toFixed(2));
      if (hit.act === '+') this.speed = Math.min(4, +(this.speed + 0.25).toFixed(2));
      if (hit.act === 'new') { if (this.sub === 2) this.life = null; if (this.sub === 3) this.pd = null; }
      return;
    }
    if (this.sub === 2) { this.life = null; return; }
    if (this.sub === 3) { this.pd = null; return; }
    if (this.sub === 4) { this.card = hit && hit.sky ? hit.sky : null; return; }
    if (this.sub === 5) { if (hit && hit.follow != null) this.gl.follow = this.gl.follow === hit.follow ? -1 : hit.follow; return; }
  },
  frame(dt, t) {
    const o = this.cv, { g, w, h } = o;
    if (w < 40 || h < 40) return;
    T.hits.clear();
    const u = T.unit(w, h), m = u * 1.6, y0 = 50;
    this.ph += dt * this.speed;
    this.dust.tick(dt * this.speed); this.dcam.yaw += dt * 0.05;
    const L = { u, m, y0, top: y0 + u * 3.2, bot: h - m * 0.6 - u * 2.2 };
    T.backdrop(g, w, h, 0.5, 0.5);
    let st = '';
    if (this.sub === 0) st = this.tess(g, w, h, L, dt);
    else if (this.sub === 1) st = this.torus(g, w, h, L);
    else if (this.sub === 2) st = this.lifeF(g, w, h, L, dt);
    else if (this.sub === 3) st = this.pend(g, w, h, L, dt);
    else if (this.sub === 4) st = this.sky(g, w, h, L, dt, t);
    else if (this.sub === 5) st = this.globe(g, w, h, L, dt, t);
    else st = this.weather(g, w, h, L, dt, t);                 // views/weather.js
    T.bloom(o, this.sub === 2 ? 0.7 : this.sub === 6 ? 0.8 : this.sub >= 4 ? 0.6 : 0.85);
    // HUD поверх — плоский и чёткий
    T.header(g, m, y0, w - 2 * m, u, this.subs[this.sub], st.status);
    T.dots(g, m, y0 + u * 2.75, u, this.subs.length, this.sub);
    if (this.sub <= 3) this.speedCtl(g, w, h, L);
    if (st.draw) st.draw();
    T.footer(g, m, h - m * 0.6 - u * 1.6, w - 2 * m, u, st.hint || '', st.right || 'math · on tablet');
    T.finish(g, w, h);
  },
  // кнопки скорости (и «новый мир») прямо в кадре, справа внизу
  speedCtl(g, w, h, L) {
    const { u, m } = L, bh = u * 2.2, y = h - m * 0.6 - u * 1.6 - bh - u * 0.8;
    let x = w - m;
    const btn = (label, act, bw, on) => {
      x -= bw;
      T.panel(g, x, y, bw, bh, u * 0.6, on ? `rgba(${T.RGB.acc},.12)` : `rgba(${T.RGB.bg},.55)`, on ? T.C.acc : `rgba(${T.RGB.acc},.35)`);
      T.txt(g, label, x + bw / 2, y + bh * 0.68, u * 1.15, on ? T.C.hot : T.C.acc, 't', 'center');
      if (act) T.hits.rect(x, y, bw, bh, { act });
      x -= u * 0.5;
    };
    btn('+', '+', bh); btn('×' + this.speed.toFixed(2).replace(/0$/, ''), null, u * 4.6); btn('−', '-', bh);
    if (this.sub === 2 || this.sub === 3) btn(this.sub === 2 ? 'NEW WORLD' : 'NEW SWING', 'new', u * 7.5, true);
  },

  // ---------- 4D-гиперкуб: вращение в XW, YW, ZW, XY; перспектива 4D->3D->экран ----------
  tess(g, w, h, L, dt) {
    const { u, top, bot } = L;
    this.dcam.frame(w / 2, (top + bot) / 2 + u * 2, Math.min(w, h) * 1.1);
    this.dust.draw(g, this.dcam);
    const t = this.ph, A = [t * 0.9, t * 0.55, t * 0.35, t * 0.25], cx = w / 2, cy = (top + bot) / 2, K = Math.min(w, bot - top) * 0.5;
    const P = [];
    for (let i = 0; i < 16; i++) {
      let x = i & 1 ? 1 : -1, y = i & 2 ? 1 : -1, z = i & 4 ? 1 : -1, ww = i & 8 ? 1 : -1, c, s, tx;
      c = Math.cos(A[0]); s = Math.sin(A[0]); tx = x * c - ww * s; ww = x * s + ww * c; x = tx;
      c = Math.cos(A[1]); s = Math.sin(A[1]); tx = y * c - ww * s; ww = y * s + ww * c; y = tx;
      c = Math.cos(A[2]); s = Math.sin(A[2]); tx = z * c - ww * s; ww = z * s + ww * c; z = tx;
      c = Math.cos(A[3]); s = Math.sin(A[3]); tx = x * c - y * s; y = x * s + y * c; x = tx;
      const k4 = 2.6 / (3.2 - ww); x *= k4; y *= k4; z *= k4;
      c = Math.cos(0.5); s = Math.sin(0.5); tx = y * c - z * s; z = y * s + z * c; y = tx;
      const k3 = K / (5.2 - z) * 2.2;
      P.push([cx + x * k3, cy + y * k3, ww, z]);
    }
    // фосфорный след: кадр раз в 1/60 с, независимо от частоты экрана
    this.trailT += dt;
    if (this.trailT >= 1 / 60 || !this.trail.length) { this.trailT = 0; this.trail.unshift(P); if (this.trail.length > 14) this.trail.pop(); } else this.trail[0] = P;
    g.lineCap = 'round';
    for (let f = this.trail.length - 1; f >= 1; f--) {
      const Q = this.trail[f]; g.strokeStyle = `rgba(${T.RGB.acc},${0.16 * (1 - f / 14)})`; g.lineWidth = 1; g.beginPath();
      for (let i = 0; i < 16; i++) for (let b = 0; b < 4; b++) { const j = i ^ (1 << b); if (j < i) continue; g.moveTo(Q[i][0], Q[i][1]); g.lineTo(Q[j][0], Q[j][1]); }
      g.stroke();
    }
    // рёбра: WebGL — светящиеся трубки; иначе 2D-линии (дальние по w — тусклее и тоньше)
    const E = [];
    const glT = T.galGL.tess(g, w, h, P, u);
    if (!glT)
    for (let i = 0; i < 16; i++) for (let b = 0; b < 4; b++) { const j = i ^ (1 << b); if (j > i) E.push([i, j, (P[i][2] + P[j][2]) / 2]); }
    E.sort((a, b) => a[2] - b[2]);
    for (const [i, j, wv] of E) {
      const v = T.clamp(0.45 + wv * 0.35);
      g.strokeStyle = T.heat(v, 0.85); g.lineWidth = u * (0.1 + 0.12 * v);
      g.beginPath(); g.moveTo(P[i][0], P[i][1]); g.lineTo(P[j][0], P[j][1]); g.stroke();
    }
    if (glT) g.globalCompositeOperation = 'lighter';     // вершины поверх трубок — светом, не тёмными точками
    for (const p of P) { const v = T.clamp(0.5 + p[2] * 0.4); g.fillStyle = T.heat(v); g.beginPath(); g.arc(p[0], p[1], u * (0.22 + 0.22 * v), 0, T.TAU); g.fill(); }
    g.globalCompositeOperation = 'source-over'; g.lineCap = 'butt';
    const deg = a => String(Math.round(((a * 180 / Math.PI) % 360 + 360) % 360)).padStart(3, '0');
    return {
      status: '4D hypercube · 16 vertices · 32 edges',
      hint: '±: speed', right: 'xw ' + deg(A[0]) + '°  yw ' + deg(A[1]) + '°  zw ' + deg(A[2]) + '°  xy ' + deg(A[3]) + '°',
    };
  },

  // ---------- тор из точек: освещение и глубина (как donut.c), 3600 точек ----------
  torus(g, w, h, L) {
    const { u, top, bot } = L;
    this.dcam.frame(w / 2, (top + bot) / 2 + u * 2, Math.min(w, h) * 1.1);
    this.dust.draw(g, this.dcam);
    const Aa = this.ph * 0.8, Bb = this.ph * 0.45, cA = Math.cos(Aa), sA = Math.sin(Aa), cB = Math.cos(Bb), sB = Math.sin(Bb);
    const R1 = 1, R2 = 2.1, K2 = 6.5, K1 = Math.min(w, bot - top) * 0.88, cx = w / 2, cy = (top + bot) / 2, U = 120, V = 30;
    if (T.galGL.torus(g, w, h, { Aa, Bb, K1, K2, cen: [cx, cy], t: this.ph })) return { status: 'ray-traced torus · 120×30 lights', hint: '±: speed', right: 'R1 1.0  R2 2.1  K2 6.5 · webgl' };
    const pts = this.tp || (this.tp = new Float32Array(U * V * 4));
    let n = 0;
    for (let i = 0; i < V; i++) {
      const th = i * 2 * Math.PI / V, ct = Math.cos(th), st = Math.sin(th);
      for (let j = 0; j < U; j++) {
        const ph = j * 2 * Math.PI / U + i * 0.06, cp = Math.cos(ph), sp = Math.sin(ph), ox = R2 + R1 * ct, oy = R1 * st;
        const x = ox * (cB * cp + sA * sB * sp) - oy * cA * sB, y = ox * (sB * cp - sA * cB * sp) + oy * cA * cB, z = K2 + cA * ox * sp + oy * sA;
        const ny = ct * (sB * cp - sA * cB * sp) + st * cA * cB, nz = cA * ct * sp + st * sA;
        pts[n++] = cx + K1 * x / z; pts[n++] = cy - K1 * y / z; pts[n++] = z; pts[n++] = ny * 0.7071 - nz * 0.7071;
      }
    }
    const N = U * V, idx = this.ti || (this.ti = new Uint16Array(N));
    for (let i = 0; i < N; i++) idx[i] = i;
    idx.sort((a, b) => pts[b * 4 + 2] - pts[a * 4 + 2]);
    // точки по 12 уровням яркости: один путь на уровень
    const B = 12, paths = this._tb || (this._tb = Array.from({ length: B }, () => []));
    for (const p of paths) p.length = 0;
    const s = u / 16;
    for (let k = 0; k < N; k++) {
      const q = idx[k] * 4, depth = T.clamp((9.5 - pts[q + 2]) / 7, 0.15, 1), Lm = Math.max(0, pts[q + 3]);
      const v = T.clamp(0.08 + 0.92 * Lm * depth + 0.12 * depth);
      paths[Math.min(B - 1, v * B | 0)].push(q);
    }
    for (let b = 0; b < B; b++) {
      const v = (b + 0.5) / B, r = (0.9 + v * 2.6) * s;
      g.fillStyle = T.heat(v, 0.35 + 0.65 * v); g.beginPath();
      for (const q of paths[b]) g.rect(pts[q] - r / 2, pts[q + 1] - r / 2, r, r);
      g.fill();
    }
    return { status: '3600 points · light + depth', hint: '±: speed', right: 'R1 1.0  R2 2.1  K2 6.5' };
  },

  // ---------- «Жизнь» Конвея: большое поле, светящиеся клетки и угасающие призраки ----------
  lifeF(g, w, h, L, dt) {
    const { u, m, top, bot } = L;
    const portrait = h > w * 1.05, W = portrait ? 200 : 320, H = portrait ? 320 : 200;
    const aw = w - 2 * m, ah = bot - top - u * 3.4;
    const cell = Math.min(aw / W, ah / H);
    if (W < 8 || H < 8 || cell <= 0) return {};
    let Lf = this.life;
    if (!Lf || Lf.W !== W || Lf.H !== H) {
      Lf = this.life = { W, H, a: new Uint8Array(W * H), b: new Uint8Array(W * H), glow: new Float32Array(W * H), t: 0, gen: 0, still: 0, prev: -1, pop: [],
        img: new ImageData(W, H), off: Object.assign(document.createElement('canvas'), { width: W, height: H }) };
      for (let i = 0; i < W * H; i++) Lf.a[i] = Math.random() < 0.28 ? 1 : 0;
    }
    Lf.t += dt * this.speed * 12;
    let steps = 0;
    while (Lf.t >= 1 && steps++ < 4) {
      Lf.t -= 1; Lf.gen++; let pop = 0;
      for (let y = 0; y < H; y++) {
        const ym = ((y + H - 1) % H) * W, yy = y * W, yp = ((y + 1) % H) * W;
        for (let x = 0; x < W; x++) {
          const xm = (x + W - 1) % W, xp = (x + 1) % W;
          const nb = (Lf.a[ym + xm] > 0) + (Lf.a[ym + x] > 0) + (Lf.a[ym + xp] > 0) + (Lf.a[yy + xm] > 0) + (Lf.a[yy + xp] > 0) + (Lf.a[yp + xm] > 0) + (Lf.a[yp + x] > 0) + (Lf.a[yp + xp] > 0);
          const cur = Lf.a[yy + x];
          const nv = cur ? (nb === 2 || nb === 3 ? Math.min(cur + 1, 255) : 0) : (nb === 3 ? 1 : 0);
          if (cur && !nv) Lf.glow[yy + x] = 1;
          Lf.b[yy + x] = nv; if (nv) pop++;
        }
      }
      [Lf.a, Lf.b] = [Lf.b, Lf.a];
      Lf.still = Math.abs(pop - Lf.prev) <= 2 ? Lf.still + 1 : 0; Lf.prev = pop;
      Lf.pop.push(pop); if (Lf.pop.length > 240) Lf.pop.shift();
      if (pop < 30) { this.life = null; return {}; }
      if (Lf.still > 60) {                                // застой — подбросить «кляксы»
        for (let k = 0; k < 8; k++) { const cx0 = Math.random() * W | 0, cy0 = Math.random() * H | 0; for (let q = 0; q < 40; q++) Lf.a[((cy0 + (Math.random() * 7 | 0)) % H) * W + (cx0 + (Math.random() * 7 | 0)) % W] = 1; }
        Lf.still = 0;
      }
    }
    if (Lf.t > 4) Lf.t = 0;
    const D = Lf.img.data, fade = Math.pow(0.05, dt);
    for (let i = 0; i < W * H; i++) {
      const a = Lf.a[i];
      let r = 5, gg = 8, bb = 13;
      if (a) { const c = a === 1 ? [235, 250, 255] : T.heatRGB(Math.max(0.42, 0.95 - a * 0.025)); r = c[0]; gg = c[1]; bb = c[2]; }
      else if (Lf.glow[i] > 0.01) { const k = Lf.glow[i] * 0.5; r = 5 + 35 * k; gg = 8 + 87 * k; bb = 13 + 117 * k; Lf.glow[i] *= fade; }
      D[i * 4] = r; D[i * 4 + 1] = gg; D[i * 4 + 2] = bb; D[i * 4 + 3] = 255;
    }
    Lf.off.getContext('2d').putImageData(Lf.img, 0, 0);
    const fw = W * cell, fh = H * cell, fx = (w - fw) / 2, fy = top + u * 0.6;
    g.imageSmoothingEnabled = false;
    g.drawImage(Lf.off, fx, fy, fw, fh);
    g.imageSmoothingEnabled = true;
    T.brackets(g, fx - u * 0.4, fy - u * 0.4, fw + u * 0.8, fh + u * 0.8, u * 1.2, `rgba(${T.RGB.acc},.6)`, 1.5);
    const pop = Lf.pop;
    return {
      status: `gen ${Lf.gen}  ·  cells ${Lf.prev < 0 ? '—' : Lf.prev}  ·  ${W}×${H}`,
      hint: 'tap: new world  ·  ±: speed', right: 'b3/s23 · torus field',
      draw: () => {
        // график населения — слева внизу, на уровне кнопок
        const pw = Math.min(u * 16, w * 0.3), ph = u * 2.2, px = m, py = h - m * 0.6 - u * 1.6 - ph - u * 0.8;
        if (pop.length > 2) {
          const lo = Math.min(...pop), hi = Math.max(...pop) + 1;
          T.panel(g, px, py, pw, ph, u * 0.6, `rgba(${T.RGB.bg},.55)`);
          T.spark(g, px + u * 0.5, py + u * 0.35, pw - u, ph - u * 0.7, pop, lo - (hi - lo) * 0.2, hi);
        }
      },
    };
  },

  // ---------- двойной маятник: Лагранж + Рунге-Кутта 4, длинный светящийся след ----------
  pend(g, w, h, L, dt) {
    const { u, top, bot } = L;
    if (!this.pd) { this.pd = { y: [2 + Math.random(), 1.5 + Math.random() * 1.6, 0, 0] }; this.trail = []; }
    const f = y => { const gr = 9.81, [t1, t2, w1, w2] = y, d = t1 - t2, den = 3 - Math.cos(2 * d);
      return [w1, w2, (-gr * 3 * Math.sin(t1) - gr * Math.sin(t1 - 2 * t2) - 2 * Math.sin(d) * (w2 * w2 + w1 * w1 * Math.cos(d))) / den,
        (2 * Math.sin(d) * (w1 * w1 * 2 + gr * 2 * Math.cos(t1) + w2 * w2 * Math.cos(d))) / den]; };
    const R = Math.min(w, bot - top) * 0.22, cx = w / 2, cy = top + (bot - top) * 0.5 - R * 0.1, steps = 8, hs = Math.min(dt, 0.05) * this.speed / steps;
    for (let s = 0; s < steps; s++) {
      const y = this.pd.y, k1 = f(y), k2 = f(y.map((v, i) => v + k1[i] * hs / 2)), k3 = f(y.map((v, i) => v + k2[i] * hs / 2)), k4 = f(y.map((v, i) => v + k3[i] * hs));
      this.pd.y = y.map((v, i) => v + hs / 6 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]));
    }
    // точка следа ~240 раз в секунду (не зависит от частоты экрана)
    this.trailT += dt;
    const [t1, t2, w1, w2] = this.pd.y, x1 = cx + R * Math.sin(t1), y1 = cy + R * Math.cos(t1), x2 = x1 + R * Math.sin(t2), y2 = y1 + R * Math.cos(t2);
    if (this.trailT >= 1 / 240) { this.trailT = 0; this.trail.push([x2 - cx, y2 - cy, Math.min(1, (Math.abs(w2) + Math.abs(w1) * 0.5) / 12)]); }
    if (this.trail.length > 2400) this.trail.splice(0, this.trail.length - 2400);
    // круг досягаемости и сетка
    g.strokeStyle = T.C.line; g.lineWidth = 1; g.beginPath(); g.arc(cx, cy, 2 * R, 0, T.TAU); g.stroke();
    g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.setLineDash([2, 6]); g.stroke(); g.setLineDash([]);
    // след: пачки по возрасту и скорости — один путь на пачку
    const n = this.trail.length, AG = 16, SP = 4;
    g.lineCap = 'round';
    for (let a = 0; a < AG; a++) for (let s = 0; s < SP; s++) {
      g.beginPath(); let any = false;
      const i0 = Math.max(1, Math.floor(a * n / AG)), i1 = Math.floor((a + 1) * n / AG);
      for (let i = i0; i < i1; i++) {
        const p = this.trail[i - 1], q = this.trail[i];
        if (Math.min(SP - 1, q[2] * SP | 0) !== s) continue;
        g.moveTo(cx + p[0], cy + p[1]); g.lineTo(cx + q[0], cy + q[1]); any = true;
      }
      if (!any) continue;
      const age = (a + 1) / AG;
      g.strokeStyle = T.heat(0.35 + 0.65 * (s + 0.5) / SP, 0.05 + 0.95 * age * age); g.lineWidth = u * (0.05 + 0.16 * age);
      g.stroke();
    }
    g.strokeStyle = T.C.label; g.lineWidth = u * 0.16; g.beginPath(); g.moveTo(cx, cy); g.lineTo(x1, y1); g.lineTo(x2, y2); g.stroke();
    g.lineCap = 'butt';
    T.diamond(g, cx, cy, u * 0.35, T.C.dim, true);
    g.fillStyle = T.C.acc; g.beginPath(); g.arc(x1, y1, u * 0.55, 0, T.TAU); g.fill();
    g.fillStyle = T.C.hot; g.beginPath(); g.arc(x2, y2, u * 0.7, 0, T.TAU); g.fill();
    // энергия (в единицах m = l = 1): видно, что RK4 держит её почти постоянной
    const E = 0.5 * (2 * w1 * w1 + w2 * w2 + 2 * w1 * w2 * Math.cos(t1 - t2)) - 9.81 * (2 * Math.cos(t1) + Math.cos(t2));
    const dg = a => Math.round((((a * 180 / Math.PI) % 360) + 540) % 360 - 180);
    return { status: `θ1 ${dg(t1)}°  θ2 ${dg(t2)}°  ·  E ${E.toFixed(2)}`, hint: 'tap: new swing  ·  ±: speed', right: 'lagrange · rk4 · chaos' };
  },

  // ======================================================================
  //  SKY — купол над городом прямо сейчас (зенит в центре, север вверху, восток слева)
  // ======================================================================
  skyCalc(now) {
    const c = this.sc;
    if (c && Math.abs(now - c.t) < 1) return c;
    const A = T.astro, D = A.data(), P = A.place(), jd = A.jd(now), gm = A.gmst(jd);
    const aa = (ra, dec) => A.altAz(ra, dec, jd, P.lat, P.lon, gm);
    const stars = D.stars.map(s => aa(s.ra, s.dec));
    const lines = D.lines.map(Lp => Lp.map(p => aa(p[0], p[1])));
    const sun = A.sun(jd), sunA = aa(sun.ra, sun.dec);
    const mo = A.moon(jd), moA = aa(mo.ra, mo.dec);
    const pl = A.PLANET_RU.map((_, i) => { const r = A.planet(i, jd); return aa(r.ra, r.dec); });
    const con = D.con.map(k => aa(k.ra, k.dec));
    // спутники: след ±10 минут по небу
    const sats = A.sats().map(s => { const tr = []; for (let k = -20; k <= 20; k++) tr.push(A.satLook(A.satPos(s, now + k * 30), P.lat, P.lon)); return { s, tr }; });
    return (this.sc = { t: now, P, D, stars, lines, sun: sunA, moon: moA, phase: mo.phase, waxing: mo.waxing, pl, con, sats });
  },
  sky(g, w, h, L, dt, tt) {
    const { u, m, top, bot } = L, A = T.astro, now = A.now(), c = this.skyCalc(now);
    this.skyRot += (this.skyRotT - this.skyRot) * T.ease(dt, 0.25);
    const portrait = h > w * 1.05;
    const R = portrait ? Math.min(w / 2 - m - u * 2, (bot - top) * 0.3) : Math.min((bot - top) / 2 - u * 1.8, w * 0.3);
    const cx = portrait ? w / 2 : m + R + u * 3, cy = portrait ? top + R + u * 2.2 : (top + bot) / 2 + u * 0.3;
    const rot = this.skyRot, D2R = Math.PI / 180;
    const xy = (a) => { if (a.alt < 0) return null; const r = R * Math.tan((90 - a.alt) * D2R / 2), q = (a.az - rot) * D2R; return [cx - r * Math.sin(q), cy - r * Math.cos(q)]; };
    const sunAlt = c.sun.alt, day = sunAlt > -6, night = sunAlt < -12;
    // купол: цвет неба по высоте Солнца
    const sky0 = day ? [16, 52, 72] : night ? [6, 14, 22] : [10, 30, 44], sky1 = day ? [8, 26, 38] : night ? [4, 9, 15] : [6, 18, 28];
    const glSky = T.galGL.sky(g, cx, cy, R, {
      t: tt, rot: rot * D2R, lat: c.P.lat * D2R, lst: (A.gmst(A.jd(now)) + c.P.lon) * D2R,
      sunAlt: sunAlt * D2R, sunAz: c.sun.az * D2R, day: T.clamp((sunAlt + 6) / 8), night: T.clamp((-6 - sunAlt) / 8),
    });
    if (!glSky) { const bg = g.createRadialGradient(cx, cy, 0, cx, cy, R); bg.addColorStop(0, T.rgb(sky1)); bg.addColorStop(1, T.rgb(sky0)); g.fillStyle = bg; g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.fill(); }
    // зарево у горизонта со стороны Солнца (сумерки) — в WebGL оно в шейдере
    if (!glSky && sunAlt > -18 && sunAlt < 8) {
      const q = (c.sun.az - rot) * D2R, gx = cx - R * Math.sin(q), gy = cy - R * Math.cos(q), k = 1 - Math.abs(sunAlt + 5) / 13;
      const gr = g.createRadialGradient(gx, gy, 0, gx, gy, R * 0.9); gr.addColorStop(0, `rgba(${T.RGB.acc},${0.22 * T.clamp(k)})`); gr.addColorStop(1, `rgba(${T.RGB.acc},0)`);
      g.save(); g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.clip(); g.fillStyle = gr; g.fillRect(cx - R, cy - R, 2 * R, 2 * R); g.restore();
    }
    // круги высоты 30° и 60°
    g.strokeStyle = `rgba(${T.RGB.dim},.35)`; g.lineWidth = 1; g.setLineDash([2, 5]);
    for (const alt of [30, 60]) { g.beginPath(); g.arc(cx, cy, R * Math.tan((90 - alt) * D2R / 2), 0, T.TAU); g.stroke(); }
    g.setLineDash([]);
    // линии созвездий
    g.strokeStyle = `rgba(${T.RGB.acc},${day ? 0.16 : 0.32})`; g.lineWidth = 1; g.beginPath();
    for (const Lp of c.lines) { let have = false; for (const a of Lp) { const p = xy(a); if (p) { have ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); have = true; } else have = false; } }
    g.stroke();
    // звёзды: днём видны только самые яркие; яркие мерцают
    const magLim = day ? 1.5 : night ? 5.2 : 3.0, D = c.D, s = u / 16, buckets = [[], [], [], []];
    for (let i = 0; i < D.stars.length; i++) {
      const st = D.stars[i]; if (st.mag > magLim) break;
      const p = xy(c.stars[i]); if (!p) continue;
      if (st.mag < 2.2) {
        const tw = 0.75 + 0.25 * Math.sin(tt * (2.2 + (i % 7) * 0.4) + i * 1.7), r = s * Math.max(1.2, (2.6 - st.mag) * 1.6) * (0.85 + 0.15 * tw);
        g.fillStyle = T.heat(0.7 + 0.3 * tw * T.clamp((2.2 - st.mag) / 3)); g.beginPath(); g.arc(p[0], p[1], r, 0, T.TAU); g.fill();
        if (st.mag < 0.8) { g.strokeStyle = `rgba(${T.RGB.hot},${0.35 * tw})`; g.lineWidth = 1; g.beginPath(); g.moveTo(p[0] - r * 3, p[1]); g.lineTo(p[0] + r * 3, p[1]); g.moveTo(p[0], p[1] - r * 3); g.lineTo(p[0], p[1] + r * 3); g.stroke(); }
        if (D.names[i]) T.hits.circle(p[0], p[1], u * 1.4, { sky: { kind: 'star', name: D.names[i], mag: st.mag, a: c.stars[i] } });
      } else buckets[Math.min(3, (st.mag - 2.2) / 0.8 | 0)].push(p);
    }
    buckets.forEach((b, k) => { const r = s * (1.5 - k * 0.25); g.fillStyle = T.heat(0.62 - k * 0.1); g.beginPath(); for (const p of b) g.rect(p[0] - r / 2, p[1] - r / 2, r, r); g.fill(); });
    // подписи без наложений
    const used = [];
    const label = (txt, x, y, px, col, f = 'c') => {
      const tw2 = T.tw(g, txt, px, f), bx = x - tw2 / 2, by = y - px;
      for (const q of used) if (bx < q[0] + q[2] && q[0] < bx + tw2 && by < q[1] + q[3] && q[1] < by + px * 1.2) return;
      used.push([bx, by, tw2, px * 1.2]); T.txt(g, txt, x, y, px, col, f, 'center');
    };
    // планеты
    const plUp = [];
    c.pl.forEach((a, i) => {
      const p = xy(a); if (!p) return;
      plUp.push([i, a.alt]);
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(p[0], p[1], u * 0.3, 0, T.TAU); g.fill();
      g.strokeStyle = T.C.acc; g.lineWidth = 1.2; g.beginPath(); g.arc(p[0], p[1], u * 0.65, 0, T.TAU); g.stroke();
      label(A.planetName(i), p[0], p[1] - u * 1, u * 0.78, T.C.hot);
      T.hits.circle(p[0], p[1], u * 1.5, { sky: { kind: 'planet', name: A.planetName(i), a } });
    });
    // Луна с фазой
    let p = xy(c.moon);
    if (p) {
      const r = u * 0.9, k = c.phase;
      g.fillStyle = `rgba(${T.RGB.hot},.12)`; g.beginPath(); g.arc(p[0], p[1], r, 0, T.TAU); g.fill();
      g.save(); g.translate(p[0], p[1]); if (!c.waxing) g.scale(-1, 1);
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(0, 0, r, -Math.PI / 2, Math.PI / 2); g.ellipse(0, 0, r * Math.abs(1 - 2 * k), r, 0, Math.PI / 2, -Math.PI / 2, k > 0.5); g.fill();
      g.restore();
      label(T.L('Moon ', 'Луна ') + Math.round(k * 100) + '%', p[0], p[1] - r - u * 0.5, u * 0.78, T.C.label);
      T.hits.circle(p[0], p[1], u * 1.6, { sky: { kind: 'moon', name: T.L('Moon', 'Луна'), a: c.moon, phase: k } });
    }
    // Солнце
    p = xy(c.sun);
    if (p) {
      const gr = g.createRadialGradient(p[0], p[1], 0, p[0], p[1], u * 3); gr.addColorStop(0, `rgba(${T.RGB.hot},.9)`); gr.addColorStop(0.25, `rgba(${T.RGB.acc},.35)`); gr.addColorStop(1, `rgba(${T.RGB.acc},0)`);
      g.fillStyle = gr; g.beginPath(); g.arc(p[0], p[1], u * 3, 0, T.TAU); g.fill();
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(p[0], p[1], u * 0.8, 0, T.TAU); g.fill();
      T.hits.circle(p[0], p[1], u * 2, { sky: { kind: 'sun', name: T.L('Sun', 'Солнце'), a: c.sun } });
    }
    // спутники над горизонтом: след и точка «сейчас»
    const satsUp = [];
    for (const { s: sat, tr } of c.sats) {
      g.strokeStyle = 'rgba(255,255,255,.0)'; g.lineWidth = 1.2;
      let have = false; g.beginPath();
      for (const a of tr) { const q = xy({ alt: a.el, az: a.az }); if (q) { have ? g.lineTo(q[0], q[1]) : g.moveTo(q[0], q[1]); have = true; } else have = false; }
      g.strokeStyle = `rgba(${T.RGB.acc},.45)`; g.setLineDash([4, 4]); g.stroke(); g.setLineDash([]);
      const look = A.satLook(A.satPos(sat, now), c.P.lat, c.P.lon), q = xy({ alt: look.el, az: look.az });
      if (q) {
        satsUp.push([sat.name, look.el]);
        const pulse = 0.5 + 0.5 * Math.sin(tt * 5);
        T.diamond(g, q[0], q[1], u * 0.55, T.C.hot, true);
        g.strokeStyle = `rgba(${T.RGB.hot},${1 - pulse})`; g.lineWidth = 1.2; g.beginPath(); g.arc(q[0], q[1], u * (0.8 + pulse * 1.2), 0, T.TAU); g.stroke();
        label(sat.name, q[0], q[1] - u * 1.1, u * 0.78, T.C.hot, 'm');
        T.hits.circle(q[0], q[1], u * 1.6, { sky: { kind: 'sat', name: sat.name, a: { alt: look.el, az: look.az } } });
      }
    }
    // созвездия (по-русски)
    if (!day) D.con.forEach((k, i) => { const a = c.con[i]; if (a.alt < 12) return; const q = xy(a); if (q) label(T.L(k.ab, k.ru), q[0], q[1], u * 0.7, `rgba(${T.RGB.acc},.6)`); });
    // горизонт: кольцо с рисками и стороны света
    g.strokeStyle = T.C.acc; g.lineWidth = 1.6; g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.stroke();
    g.lineWidth = 1; g.beginPath();
    for (let az = 0; az < 360; az += 10) { const q = (az - rot) * D2R, l = az % 30 ? 1.025 : 1.05; g.moveTo(cx - R * Math.sin(q), cy - R * Math.cos(q)); g.lineTo(cx - R * l * Math.sin(q), cy - R * l * Math.cos(q)); }
    g.strokeStyle = T.C.dim; g.stroke();
    ['N', 'E', 'S', 'W'].forEach((sd, k) => { const q = (k * 90 - rot) * D2R; T.txt(g, sd, cx - (R + u * 1.6) * Math.sin(q), cy - (R + u * 1.6) * Math.cos(q) + u * 0.45, u * 1.2, k === 0 ? T.C.hot : T.C.acc, 't', 'center'); });
    // карточка выбранного светила
    const card = this.card;
    const d = new Date(), hh = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0') + ':' + String(d.getSeconds()).padStart(2, '0');
    const state = day ? (sunAlt > 0 ? 'DAY' : 'TWILIGHT') : night ? 'NIGHT' : 'TWILIGHT';
    return {
      status: `${c.P.city} · ${c.P.lat.toFixed(1)}° ${c.P.lon.toFixed(1)}° · ${state}`,
      hint: 'drag: turn the dome  ·  tap: star / planet', right: 'stars ' + D.stars.length + ' · jpl + meeus',
      draw: () => {
        // панель справа (или снизу в портрете)
        const pw = portrait ? w - 2 * m : Math.min(u * 22, w - cx - R - m - u * 5), px = portrait ? m : w - m - pw;
        const py = portrait ? cy + R + u * 3 : top + u * 0.4, ph = portrait ? Math.min(u * 24, Math.max(u * 10, bot - py - u * 3.4)) : Math.min(bot - top - u * 3.6, u * (18 + 1.5 * (plUp.length + A.sats().length)));
        if (pw < u * 10) return;
        T.panel(g, px, py, pw, ph, u);
        let y = py + u * 3.4;
        T.txt(g, hh, px + u, y, u * 3, T.C.hot, 't');
        T.txt(g, 'LOCAL TIME · ' + d.toLocaleDateString('ru-RU'), px + u, y + u * 1.2, u * 0.72, T.C.label);
        y += u * 3;
        const row = (k, v, col = T.C.hot) => { if (y > py + ph - u * 0.8) return; T.txt(g, k, px + u, y, u * 0.8, T.C.dim); T.txt(g, v, px + pw - u, y, u * 0.85, col, 'c', 'right'); y += u * 1.5; };
        row('SUN', `${sunAlt >= 0 ? '+' : ''}${Math.round(sunAlt)}°  ${state}`);
        row('MOON', `${Math.round(c.phase * 100)}%  ${c.moon.alt >= 0 ? '+' : ''}${Math.round(c.moon.alt)}°  ${c.waxing ? T.L('waxing', 'растёт') : T.L('waning', 'убывает')}`);
        y += u * 0.4;
        T.txt(g, plUp.length ? 'PLANETS UP' : 'NO PLANETS UP', px + u, y, u * 0.75, T.C.acc); y += u * 1.4;
        plUp.sort((a, b) => b[1] - a[1]).forEach(([i, alt]) => row(A.planetName(i), Math.round(alt) + '°'));
        y += u * 0.4;
        if (y < py + ph - u * 2) {
          const sats = A.sats();
          T.txt(g, sats.length ? 'SATELLITES' : 'NO ORBITS · TENGAN DESKTOP SENDS THEM', px + u, y, u * 0.75, T.C.acc); y += u * 1.4;
          for (const sat of sats) { const up = satsUp.find(q => q[0] === sat.name); row(sat.name, up ? 'UP ' + Math.round(up[1]) + '°' : 'below', up ? T.C.hot : T.C.dim); }
        }
        if (card) {
          const a = card.a, cw = u * 15, ch = u * 6.2, cxx = portrait ? m : cx - cw / 2, cyy = portrait ? py - ch - u * 0.6 : bot - ch - u * 3.6;
          T.panel(g, cxx, cyy, cw, ch, u, `rgba(${T.RGB.bg},.78)`, T.C.acc);
          T.txt(g, card.name, cxx + u, cyy + u * 1.9, u * 1.35, T.C.hot, card.kind === 'star' || card.kind === 'sat' ? 't' : 'c');
          const kind = { star: 'STAR', planet: 'PLANET', moon: 'MOON', sun: 'SUN', sat: 'SATELLITE' }[card.kind];
          T.txt(g, kind + (card.mag != null ? '  ·  MAG ' + card.mag.toFixed(1) : '') + (card.phase != null ? '  ·  ' + Math.round(card.phase * 100) + '%' : ''), cxx + u, cyy + u * 3.1, u * 0.75, T.C.label);
          T.txt(g, `ALT ${a.alt.toFixed(1)}°   AZ ${a.az.toFixed(1)}°`, cxx + u, cyy + u * 4.9, u * 1.05, T.C.acc);
          T.hits.rect(cxx, cyy, cw, ch, { sky: null });
        }
      },
    };
  },

  // ======================================================================
  //  GLOBE — Земля с днём и ночью, спутники на орбитах, пролёт над городом
  // ======================================================================
  globe(g, w, h, L, dt, tt) {
    const { u, m, top, bot } = L, A = T.astro, now = A.now(), P = A.place(), sats = A.sats(), gl = this.gl, D2R = Math.PI / 180;
    const portrait = h > w * 1.05;
    const R = portrait ? Math.min(w / 2 - m - u * 2, (bot - top) * 0.3) : Math.min((bot - top) / 2 - u * 2, w * 0.28);
    const cx = portrait ? w / 2 : m + R + u * 5, cy = portrait ? top + R + u * 3 : (top + bot) / 2 + u * 0.3;
    // спутники сейчас
    const pos = sats.map(s => A.satPos(s, now));
    // куда смотрим
    if (gl.follow >= 0 && gl.follow >= sats.length) gl.follow = -1;
    let tLon = null, tLat = null;
    if (gl.follow === -2) { tLon = P.lon; tLat = P.lat; } else if (gl.follow >= 0) { tLon = pos[gl.follow].lon; tLat = pos[gl.follow].lat; }
    if (tLon != null) {
      let dl = ((tLon - gl.lon0) % 360 + 540) % 360 - 180;
      gl.lon0 += dl * T.ease(dt, 0.6); gl.lat0 += (T.clamp(tLat * 0.7 + 8, -55, 60) - gl.lat0) * T.ease(dt, 0.8);
    } else if (performance.now() - gl.last > 3000) { gl.lon0 += dt * 6; gl.lat0 += (25 - gl.lat0) * T.ease(dt, 3); }
    const l0 = gl.lon0 * D2R, b0 = gl.lat0 * D2R, cb = Math.cos(b0), sb = Math.sin(b0);
    // мир -> вид: [x, y, z]; z>0 — лицевая сторона
    const view = (lat, lon, r = 1) => {
      const la = lat * D2R, dl = lon * D2R - l0, a = Math.cos(la) * Math.cos(dl), b = Math.cos(la) * Math.sin(dl), c = Math.sin(la);
      return [b * r, (cb * c - sb * a) * r, (sb * c + cb * a) * r];
    };
    const scr = v => [cx + R * v[0], cy - R * v[1]];
    // фон: звёзды
    if (!this.bgStars) { this.bgStars = []; for (let i = 0; i < 260; i++) this.bgStars.push([Math.random(), Math.random(), Math.random()]); }
    g.fillStyle = `rgba(${T.RGB.label},.5)`; g.beginPath();
    for (const s of this.bgStars) { const r = 0.6 + s[2] * 1.2; g.rect(s[0] * w, s[1] * h, r, r); }
    g.fill();
    // WebGL: освещённая Земля (суша, океан с бликом, огни ночи, облака, атмосфера); иначе 2D-диск
    const jd0 = A.jd(now), sun0 = A.sun(jd0), sLon0 = ((sun0.ra - A.gmst(jd0)) % 360 + 540) % 360 - 180, sv0 = view(sun0.dec, sLon0);
    const glG = T.galGL.globe(g, cx, cy, R, l0, b0, sv0, tt);
    if (!glG) {
    const at = g.createRadialGradient(cx, cy, R * 0.96, cx, cy, R * 1.14); at.addColorStop(0, `rgba(${T.RGB.acc},.45)`); at.addColorStop(0.3, `rgba(${T.RGB.acc},.12)`); at.addColorStop(1, `rgba(${T.RGB.acc},0)`);
    g.fillStyle = at; g.beginPath(); g.arc(cx, cy, R * 1.14, 0, T.TAU); g.fill();
    // день и ночь: свет считается для каждой точки диска (в малом разрешении)
    const jd = A.jd(now), sun = A.sun(jd), sLon = ((sun.ra - A.gmst(jd)) % 360 + 540) % 360 - 180, sv = view(sun.dec, sLon);
    const N = 160;
    if (!this.gImg) { this.gOff = Object.assign(document.createElement('canvas'), { width: N, height: N }); this.gImg = new ImageData(N, N); }
    const Dd = this.gImg.data;
    for (let j = 0; j < N; j++) {
      const ny = 1 - (j + 0.5) / N * 2;
      for (let i = 0; i < N; i++) {
        const nx = (i + 0.5) / N * 2 - 1, q = (j * N + i) * 4, rr = nx * nx + ny * ny;
        if (rr > 1) { Dd[q + 3] = 0; continue; }
        const nz = Math.sqrt(1 - rr), lit = nx * sv[0] + ny * sv[1] + nz * sv[2];
        const k = T.clamp((lit + 0.08) / 0.2), limb = 0.55 + 0.45 * nz;
        const tw = Math.max(0, 1 - Math.abs(lit) / 0.025) * 0.55;                    // полоса терминатора
        Dd[q] = (6 + 14 * k) * limb + 40 * tw; Dd[q + 1] = (12 + 46 * k) * limb + 120 * tw; Dd[q + 2] = (18 + 58 * k) * limb + 140 * tw; Dd[q + 3] = 255;
      }
    }
    this.gOff.getContext('2d').putImageData(this.gImg, 0, 0);
    g.imageSmoothingEnabled = true; g.drawImage(this.gOff, cx - R, cy - R, 2 * R, 2 * R);
    }
    // сетка 30°
    g.lineWidth = 1; g.strokeStyle = `rgba(${T.RGB.acc},.12)`; g.beginPath();
    const poly = pts => { let have = false; for (const v of pts) { if (v[2] > 0) { const p = scr(v); have ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); have = true; } else have = false; } };
    for (let la = -60; la <= 60; la += 30) { const a = []; for (let lo = -180; lo <= 180; lo += 5) a.push(view(la, lo)); poly(a); }
    for (let lo = -180; lo < 180; lo += 30) { const a = []; for (let la = -90; la <= 90; la += 5) a.push(view(la, lo)); poly(a); }
    g.stroke();
    // материки: днём ярко, ночью тускло
    const land = A.data().land, dayP = new Path2D(), nightP = new Path2D();
    for (const Lp of land) {
      let prev = null;
      for (const ll of Lp) {
        const v = view(ll[0], ll[1]);
        if (v[2] > 0 && prev && prev[2] > 0) {
          const a = scr(prev), b = scr(v), lit = (v[0] * sv0[0] + v[1] * sv0[1] + v[2] * sv0[2]) > 0;
          const pp = lit ? dayP : nightP; pp.moveTo(a[0], a[1]); pp.lineTo(b[0], b[1]);
        }
        prev = v;
      }
    }
    if (!glG) {
      g.lineWidth = 1.3; g.strokeStyle = `rgba(${T.RGB.acc},.9)`; g.stroke(dayP); g.strokeStyle = `rgba(${T.RGB.acc},.35)`; g.stroke(nightP);
      g.strokeStyle = `rgba(${T.RGB.acc},.7)`; g.lineWidth = 1.5; g.beginPath(); g.arc(cx, cy, R, 0, T.TAU); g.stroke();
    }
    // дом
    const hv = view(P.lat, P.lon);
    if (hv[2] > 0) {
      const p = scr(hv), pulse = (tt % 2) / 2;
      T.diamond(g, p[0], p[1], u * 0.45, T.C.hot, true);
      g.strokeStyle = `rgba(${T.RGB.hot},${1 - pulse})`; g.lineWidth = 1; g.beginPath(); g.arc(p[0], p[1], u * (0.6 + pulse * 1.6), 0, T.TAU); g.stroke();
      T.txt(g, P.city.toUpperCase(), p[0] + u * 0.9, p[1] + u * 0.3, u * 0.75, T.C.hot);
      T.hits.circle(p[0], p[1], u * 1.6, { follow: -2 });
    }
    // спутники: наземный след ±45 мин и сам спутник на своей высоте
    if (!this.trk || now - this.trk.t > 20 || this.trk.n !== sats.length) {
      this.trk = { t: now, n: sats.length, tr: sats.map(s => { const a = []; for (let k = -45; k <= 45; k++) { const q = A.satPos(s, now + k * 60); a.push([q.lat, q.lon, k]); } return a; }) };
    }
    sats.forEach((s, i) => {
      const tr = this.trk.tr[i], sel = gl.follow === i;
      for (const half of [0, 1]) {
        g.beginPath(); let have = false;
        for (const [la, lo, k] of tr) {
          if ((k < 0) !== (half === 0) && k !== 0) continue;
          const v = view(la, lo);
          if (v[2] > 0) { const p = scr(v); have ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); have = true; } else have = false;
        }
        g.strokeStyle = half === 0 ? `rgba(${T.RGB.acc},${sel ? 0.35 : 0.22})` : `rgba(${T.RGB.hot},${sel ? 0.7 : 0.45})`;
        g.lineWidth = sel ? 1.8 : 1.2; if (half === 1) g.setLineDash([5, 5]); g.stroke(); g.setLineDash([]);
      }
      const q = pos[i], r = 1 + q.alt / A.RE * 2.2, sv2 = view(q.lat, q.lon, r), ground = view(q.lat, q.lon);
      const front = sv2[2] > 0 || sv2[0] * sv2[0] + sv2[1] * sv2[1] > 1;
      if (!front) return;
      const p = scr(sv2), gp = scr(ground), pulse = 0.5 + 0.5 * Math.sin(tt * 5 + i);
      if (ground[2] > 0) { g.strokeStyle = `rgba(${T.RGB.hot},.4)`; g.lineWidth = 1; g.setLineDash([2, 3]); g.beginPath(); g.moveTo(gp[0], gp[1]); g.lineTo(p[0], p[1]); g.stroke(); g.setLineDash([]); }
      g.fillStyle = T.C.hot; g.beginPath(); g.arc(p[0], p[1], u * (0.35 + 0.15 * pulse), 0, T.TAU); g.fill();
      g.strokeStyle = `rgba(${T.RGB.acc},${0.9 - pulse * 0.6})`; g.lineWidth = 1.2; g.beginPath(); g.arc(p[0], p[1], u * (0.8 + pulse), 0, T.TAU); g.stroke();
      T.txt(g, s.name, p[0] + u * 1.2, p[1] - u * 0.5, u * 0.85, sel ? T.C.hot : T.C.acc, 't');
      T.txt(g, Math.round(q.alt) + ' km', p[0] + u * 1.2, p[1] + u * 0.6, u * 0.68, T.C.label);
      T.hits.circle(p[0], p[1], u * 1.8, { follow: i });
    });
    // ближайший пролёт над городом: раз в минуту
    const fi = gl.follow >= 0 ? gl.follow : 0, fs = sats[fi];
    if (fs && (!this.pass || this.pass.name !== fs.name || now - this.pass.t > 60 || this.pass.city !== P.city)) {
      this.pass = { name: fs.name, t: now, city: P.city, p: A.nextPass(fs, now, P.lat, P.lon, 10, 24) };
    }
    const followName = gl.follow === -2 ? 'HOME' : gl.follow >= 0 ? sats[gl.follow].name : 'AUTO';
    return {
      status: `${P.city} · ${sats.length ? sats.length + ' orbits' : 'no orbits'} · view ${followName}`,
      hint: 'drag: rotate  ·  tap satellite / home: follow', right: 'kepler + j2 · ' + new Date().toLocaleTimeString('ru-RU'),
      draw: () => {
        const pw = portrait ? w - 2 * m : Math.min(u * 22, w - cx - R - m - u * 6), px = portrait ? m : w - m - pw;
        const py = portrait ? cy + R + u * 3.2 : top + u * 0.4, ph = portrait ? Math.min(u * 24, Math.max(u * 9, bot - py - u * 3.4)) : Math.min(bot - top - u * 3.6, u * 22);
        if (pw < u * 10) return;
        T.panel(g, px, py, pw, ph, u);
        let y = py + u * 2;
        if (!fs) {
          T.txt(g, 'NO ORBITS YET', px + u, y + u, u * 1.4, T.C.hot, 't');
          T.txt(g, 'ORBITS COME FROM TENGAN DESKTOP', px + u, y + u * 2.6, u * 0.75, T.C.label);
          T.txt(g, 'ONCE — THEN IT WORKS OFFLINE', px + u, y + u * 3.8, u * 0.75, T.C.label);
          return;
        }
        const q = pos[fi];
        T.txt(g, fs.name, px + u, y + u * 0.8, u * 2, T.C.hot, 't'); y += u * 2.4;
        const row = (k, v, col = T.C.hot) => { if (y > py + ph - u * 0.6) return; T.txt(g, k, px + u, y, u * 0.8, T.C.dim); T.txt(g, v, px + pw - u, y, u * 0.9, col, 'm', 'right'); y += u * 1.5; };
        row('LAT / LON', `${Math.abs(q.lat).toFixed(1)}°${q.lat >= 0 ? 'N' : 'S'}  ${Math.abs(q.lon).toFixed(1)}°${q.lon >= 0 ? 'E' : 'W'}`);
        row('ALTITUDE', Math.round(q.alt) + ' km');
        row('SPEED', q.speed.toFixed(2) + ' km/s');
        const age = (now - fs.epoch) / 86400;
        row('ORBIT AGE', age.toFixed(1) + ' d', age > 7 ? T.C.alert : T.C.hot);
        const look = A.satLook(q, P.lat, P.lon);
        row('FROM ' + P.city.toUpperCase().slice(0, 10), look.el > 0 ? 'UP ' + Math.round(look.el) + '°' : 'below horizon', look.el > 0 ? T.C.acc : T.C.label);
        y += u * 0.6;
        if (y < py + ph - u * 4) {
          T.txt(g, 'NEXT PASS · ABOVE 10°', px + u, y, u * 0.75, T.C.acc); y += u * 2.4;
          const ps = this.pass && this.pass.p;
          if (ps && ps.found) {
            const tm = s => new Date(s * 1000).toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
            const inMin = Math.max(0, Math.round((ps.rise - now) / 60));
            T.txt(g, tm(ps.rise), px + u, y, u * 2.4, inMin === 0 ? T.C.acc : T.C.hot, 't');
            T.txt(g, inMin === 0 ? 'NOW' : 'IN ' + (inMin >= 60 ? Math.floor(inMin / 60) + ' h ' + inMin % 60 + ' min' : inMin + ' min'), px + pw - u, y - u * 0.2, u * 0.85, T.C.label, 'm', 'right');
            y += u * 1.6;
            const dirs = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
            row('MAX ELEVATION', Math.round(ps.maxEl) + '°');
            row('RISES IN', dirs[Math.round(ps.azRise / 45) % 8] + ' · ' + Math.round((ps.set - ps.rise) / 60) + ' min');
          } else T.txt(g, 'NONE IN 24 H', px + u, y, u * 1.1, T.C.label, 't');
        }
      },
    };
  },
});
