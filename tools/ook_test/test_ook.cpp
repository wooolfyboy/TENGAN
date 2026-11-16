// Тест декодеров на компьютере: читает импульсы (demod.py) и печатает найденное.
//   g++ -O1 -std=gnu++17 -I../../src test_ook.cpp ../../src/subghz/ook.cpp -o test_ook
//   ./test_ook file.pulses
#include <cstdio>
#include <cstdlib>
#include "subghz/ook.h"

static OokTrain T;
static void run() {
  if (!T.n) return;
  OokMsg m[8]; OokInfo I;
  int n = ookDecode(T, m, 8);
  ookAnalyze(T, I, m, n);
  for (int i = 0; i < n; i++) {
    printf("MSG proto=%s id=%u ch=%d kind=%d t=%.1f h=%d bat=%d btn=%d\n",
           m[i].proto, (unsigned)m[i].id, m[i].ch, m[i].kind, m[i].t, m[i].h, m[i].bat, m[i].btn);
  }
  printf("INFO pulses=%d bits=%d rows=%d mod=%s short=%d long=%d gs=%d gl=%d sync=%d cls=%s proto=%s hex=%s hash=%04x\n",
         I.pulses, I.bits, I.rows, I.mod, I.shortW, I.longW, I.shortG, I.longG, I.syncG, I.cls, I.proto, I.hex, I.hash);
  T.n = 0;
}
int main(int argc, char** argv) {
  FILE* f = argc > 1 ? fopen(argv[1], "r") : stdin;
  if (!f) return 1;
  char line[64];
  while (fgets(line, sizeof line, f)) {
    int p, g;
    if (sscanf(line, "%d %d", &p, &g) == 2) {
      if (T.n < OOK_MAXP) { T.p[T.n] = p > 65535 ? 65535 : p; T.g[T.n] = g > 65535 ? 65535 : g; T.n++; }
    } else run();
  }
  run();
  return 0;
}
