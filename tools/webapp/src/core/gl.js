// ---- WebGL: один общий контекст на всё приложение ----
// Вид рисует сцену шейдером в общий GL-холст (вне страницы), потом кладёт картинку
// на свой 2D-холст одной командой drawImage и сверху рисует HUD как обычно.
// Один контекст — без лимита браузера на число WebGL-холстов и без пересоздания при смене раздела.
// Нет WebGL (старый планшет, ?nogl) — T.GL.ok = false, вид рисует свою старую 2D-версию.
//
//   const P = T.GL.prog(FS)                 // фрагментный шейдер на весь кадр (uniform vec2 res, float t)
//   const P = T.GL.prog(FS, VS)             // свой вершинный шейдер (меши)
//   T.GL.begin(w, h, scale)                 // размер кадра в CSS-px; scale — доля разрешения (0.5 — вдвое меньше пикселей)
//   T.GL.quad(P, { t, foo: [1,2,3], tex: T.GL.tex('name', data, w, h) })   // полноэкранный проход
//   T.GL.draw(g, x, y, w, h)               // положить результат на 2D-холст вида
(() => {
const GL = T.GL = { ok: false, cv: null, gl: null, progs: [], texs: new Map(), bufs: new Map() };
const off = new URLSearchParams(location.search).has('nogl');
try {
  if (!off) {
    GL.cv = document.createElement('canvas');
    GL.gl = GL.cv.getContext('webgl', { alpha: true, premultipliedAlpha: true, antialias: false, depth: true, preserveDrawingBuffer: false, powerPreference: 'high-performance' });
    GL.ok = !!GL.gl;
  }
} catch (e) { GL.ok = false; }
if (!GL.ok) return;
const gl = GL.gl;
GL.fl = gl.getExtension('OES_texture_float'); gl.getExtension('OES_standard_derivatives'); GL.u32 = gl.getExtension('OES_element_index_uint');
GL.cv.addEventListener('webglcontextlost', e => { e.preventDefault(); GL.ok = false; }, false);

const VS = 'attribute vec2 p;varying vec2 uv;void main(){uv=p*.5+.5;gl_Position=vec4(p,0.,1.);}';
const HEAD = 'precision highp float;\n';
const sh = (type, src) => {
  const s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) { console.error('GL shader:', gl.getShaderInfoLog(s), src.slice(0, 300)); return null; }
  return s;
};
// перекраска под тему одной функцией в конце любого фрагментного шейдера: шейдеры пишут цвета DENKO,
// а в KURENAI (T.GLK = 1) бирюзовое и оранжевое становится красным, остальное — серым той же яркости
const THEMED = fs => fs.replace(/\bgl_FragColor\b/g, 'tg_FC').replace(/\bvoid\s+main\s*\(/, 'void tg_main(') + `
uniform float tg_k;
vec3 tg_map(vec3 c){ float l=dot(c,vec3(.299,.587,.114));
 float k=max(smoothstep(.12,.42,min(c.g,c.b)-c.r), smoothstep(.15,.45,c.r-max(c.g,c.b)));
 return mix(vec3(l)*vec3(1.,.988,.96), vec3(.898,.149,.184)*min(1.6,l*1.5+.15), k); }
void main(){ tg_FC=vec4(0.); tg_main(); gl_FragColor = tg_k > .5 ? vec4(tg_map(tg_FC.rgb), tg_FC.a) : tg_FC; }`;
// программа с автоматическим поиском uniform/attribute по имени
GL.prog = (fs, vs = VS) => {
  fs = (fs.includes('precision') ? '' : HEAD) + fs;
  // объявление tg_FC — сразу после precision (и #extension), чтобы шейдер мог писать его до main
  fs = THEMED(fs).replace(/(precision\s+\w+\s+float\s*;)/, '$1\nvec4 tg_FC;');
  const p = gl.createProgram(), a = sh(gl.VERTEX_SHADER, vs), b = sh(gl.FRAGMENT_SHADER, fs);
  if (!a || !b) return null;
  gl.attachShader(p, a); gl.attachShader(p, b); gl.linkProgram(p);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) { console.error('GL link:', gl.getProgramInfoLog(p)); return null; }
  const P = { p, u: {}, a: {} };
  const nu = gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS);
  for (let i = 0; i < nu; i++) { const inf = gl.getActiveUniform(p, i), n = inf.name.replace(/\[0\]$/, ''); P.u[n] = { loc: gl.getUniformLocation(p, inf.name), type: inf.type, size: inf.size }; }
  const na = gl.getProgramParameter(p, gl.ACTIVE_ATTRIBUTES);
  for (let i = 0; i < na; i++) { const inf = gl.getActiveAttrib(p, i); P.a[inf.name] = gl.getAttribLocation(p, inf.name); }
  GL.progs.push(P); return P;
};
// значения uniform: число, массив, Float32Array, текстура {tex}
let unit = 0;
GL.set = (P, vals) => {
  unit = 0;
  if (P.u.tg_k) gl.uniform1f(P.u.tg_k.loc, T.GLK || 0);   // тема для перекраски (THEMED)
  for (const k in vals) {
    const U = P.u[k]; if (!U) continue; const v = vals[k], l = U.loc;
    switch (U.type) {
      case gl.FLOAT: U.size > 1 ? gl.uniform1fv(l, v) : gl.uniform1f(l, v); break;
      case gl.FLOAT_VEC2: gl.uniform2fv(l, v); break;
      case gl.FLOAT_VEC3: gl.uniform3fv(l, v); break;
      case gl.FLOAT_VEC4: gl.uniform4fv(l, v); break;
      case gl.INT: case gl.BOOL: gl.uniform1i(l, v); break;
      case gl.FLOAT_MAT4: gl.uniformMatrix4fv(l, false, v); break;
      case gl.FLOAT_MAT3: gl.uniformMatrix3fv(l, false, v); break;
      case gl.SAMPLER_2D: gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, v.tex || v); gl.uniform1i(l, unit); unit++; break;
    }
  }
};
// текстура из массива (Uint8 RGBA/LUMINANCE) или из холста/картинки; переиспользуется по имени
GL.tex = (name, data, w, h, opt = {}) => {
  let T0 = GL.texs.get(name);
  if (!T0) {
    const t = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, t);
    const f = opt.nearest ? gl.NEAREST : gl.LINEAR, wr = opt.repeat ? gl.REPEAT : gl.CLAMP_TO_EDGE;
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, f); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, f);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, wr); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, wr);
    T0 = { tex: t, w: 0, h: 0 }; GL.texs.set(name, T0);
  }
  if (data == null) return T0;
  gl.bindTexture(gl.TEXTURE_2D, T0.tex);
  const fmt = opt.fmt === 'l' ? gl.LUMINANCE : gl.RGBA;
  gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
  if (data instanceof HTMLCanvasElement || data instanceof HTMLImageElement || (window.ImageBitmap && data instanceof ImageBitmap)) {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, data); T0.w = data.width; T0.h = data.height;
  } else if (T0.w === w && T0.h === h && T0.fmt === fmt) gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, fmt, gl.UNSIGNED_BYTE, data);
  else { gl.texImage2D(gl.TEXTURE_2D, 0, fmt, w, h, 0, fmt, gl.UNSIGNED_BYTE, data); T0.w = w; T0.h = h; T0.fmt = fmt; }
  return T0;
};
// буфер вершин/индексов по имени (меши: рельеф, город, сфера)
GL.buf = (name, data, index = false, dynamic = false) => {
  let b = GL.bufs.get(name); const tgt = index ? gl.ELEMENT_ARRAY_BUFFER : gl.ARRAY_BUFFER;
  if (!b) { b = gl.createBuffer(); GL.bufs.set(name, b); }
  gl.bindBuffer(tgt, b);
  if (data) gl.bufferData(tgt, data, dynamic ? gl.DYNAMIC_DRAW : gl.STATIC_DRAW);
  return b;
};
const quadB = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, quadB); gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);
// начало кадра: размер в CSS-px × доля разрешения (как у 2D-холстов: не больше T.RES по длинной стороне)
GL.begin = (w, h, scale = 1, clear = [0, 0, 0, 0]) => {
  const d = Math.min(window.devicePixelRatio || 1, 3, T.RES / Math.max(1, w, h)) * scale * (T.RS || 1);
  const W = Math.max(1, Math.round(w * d)), H = Math.max(1, Math.round(h * d));
  if (GL.cv.width !== W || GL.cv.height !== H) { GL.cv.width = W; GL.cv.height = H; }
  GL.w = W; GL.h = H;
  gl.viewport(0, 0, W, H); gl.clearColor(clear[0], clear[1], clear[2], clear[3]);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  return GL;
};
GL.res = () => [GL.w, GL.h];
// полноэкранный проход фрагментным шейдером; blend: 'add' — сложение (свет), иначе обычная прозрачность
GL.quad = (P, vals = {}, blend) => {
  if (!P) return;
  gl.useProgram(P.p);
  gl.disable(gl.DEPTH_TEST);
  gl.enable(gl.BLEND);
  if (blend === 'add') gl.blendFunc(gl.ONE, gl.ONE); else gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  gl.bindBuffer(gl.ARRAY_BUFFER, quadB);
  const loc = P.a.p; gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
  if (P.u.res && !('res' in vals)) vals.res = [GL.w, GL.h];
  GL.set(P, vals);
  gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
};
// результат на 2D-холст вида (в CSS-координатах)
GL.draw = (g, x = 0, y = 0, w, h) => { g.drawImage(GL.cv, x, y, w != null ? w : GL.cv.width, h != null ? h : GL.cv.height); };
// общий кусок GLSL: шум и fbm — вставлять в шейдеры как ${T.GL.NOISE}
GL.NOISE = `
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float hash3(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);}
float noise3(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(mix(hash3(i),hash3(i+vec3(1,0,0)),f.x),mix(hash3(i+vec3(0,1,0)),hash3(i+vec3(1,1,0)),f.x),f.y),
            mix(mix(hash3(i+vec3(0,0,1)),hash3(i+vec3(1,0,1)),f.x),mix(hash3(i+vec3(0,1,1)),hash3(i+vec3(1,1,1)),f.x),f.y),f.z);}
float fbm(vec2 p){float v=0.,a=.5;for(int i=0;i<5;i++){v+=a*noise(p);p=p*2.03+vec2(1.7,9.2);a*=.5;}return v;}
float fbm3(vec3 p){float v=0.,a=.5;for(int i=0;i<5;i++){v+=a*noise3(p);p=p*2.03+vec3(1.7,9.2,3.1);a*=.5;}return v;}
`;
// матрицы для 3D-мешей (без библиотек)
GL.persp = (fov, asp, n, f) => { const t = 1 / Math.tan(fov / 2), r = 1 / (n - f); return new Float32Array([t / asp, 0, 0, 0, 0, t, 0, 0, 0, 0, (n + f) * r, -1, 0, 0, 2 * n * f * r, 0]); };
GL.lookAt = (e, c, up = [0, 1, 0]) => {
  let z = [e[0] - c[0], e[1] - c[1], e[2] - c[2]]; let l = Math.hypot(...z); z = z.map(v => v / l);
  let x = [up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]]; l = Math.hypot(...x); x = x.map(v => v / l);
  const y = [z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]];
  return new Float32Array([x[0], y[0], z[0], 0, x[1], y[1], z[1], 0, x[2], y[2], z[2], 0, -(x[0] * e[0] + x[1] * e[1] + x[2] * e[2]), -(y[0] * e[0] + y[1] * e[1] + y[2] * e[2]), -(z[0] * e[0] + z[1] * e[1] + z[2] * e[2]), 1]);
};
GL.mul = (a, b) => { const o = new Float32Array(16); for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) { let s = 0; for (let k = 0; k < 4; k++) s += a[k * 4 + j] * b[i * 4 + k]; o[i * 4 + j] = s; } return o; };
GL.gl = gl;
})();
