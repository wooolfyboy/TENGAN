#!/usr/bin/env python3
"""Capture (.cu8, 8-bit I/Q) -> OOK pulses in µs, as the CC1101 would output on GDO0.

    python3 demod.py file.cu8 [rate] > file.pulses

Envelope, threshold midway between noise and signal (with hysteresis),
glitches shorter than 40 µs are merged — same as the firmware does.
Output: "pulse gap" lines, bursts separated by an empty line (silence > 20 ms).
"""
import sys, re
import numpy as np

def demod(path, rate):
    raw = np.fromfile(path, dtype=np.uint8).astype(np.float32) - 127.4
    i, q = raw[0::2], raw[1::2]
    mag = np.sqrt(i * i + q * q)
    k = max(1, int(rate * 12e-6))                      # smoothing ~12 µs
    env = np.convolve(mag, np.ones(k) / k, mode="same")
    noise = np.percentile(env, 50)
    peak = np.percentile(env, 99.8)
    hi, lo = noise + (peak - noise) * 0.35, noise + (peak - noise) * 0.2
    us = 1e6 / rate
    level, run, segs = 0, 0, []
    for v in env:
        nl = level
        if level == 0 and v > hi: nl = 1
        elif level == 1 and v < lo: nl = 0
        if nl != level:
            segs.append((level, run * us)); run = 0; level = nl
        run += 1
    segs.append((level, run * us))
    # merge glitches < 40 µs
    out = []
    for lv, d in segs:
        if out and (d < 40 or out[-1][0] == lv):
            out[-1] = (out[-1][0], out[-1][1] + d)
        else:
            out.append((lv, d))
    return out

def trains(segs):
    res, cur = [], []
    i = 0
    while i < len(segs) and segs[i][0] == 0: i += 1
    while i < len(segs):
        p = segs[i][1] if segs[i][0] == 1 else 0
        g = segs[i + 1][1] if i + 1 < len(segs) else 40000
        cur.append((int(p), int(min(g, 65535))))
        if g > 20000:
            if len(cur) >= 8: res.append(cur)
            cur = []
        i += 2
    if len(cur) >= 8: res.append(cur)
    return res

if __name__ == "__main__":
    path = sys.argv[1]
    m = re.search(r"_(\d+)k\.cu8$", path)
    rate = float(sys.argv[2]) if len(sys.argv) > 2 else (float(m.group(1)) * 1000 if m else 250000)
    for t in trains(demod(path, rate)):
        for p, g in t: print(p, g)
        print()
