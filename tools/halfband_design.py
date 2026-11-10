#!/usr/bin/env python3
"""Designs the polyphase half-band IIR coefficients used by vf::fx::Oversampler2x.

Elliptic half-band design after the method documented by Laurent de Soras for
HIIR (closed-form, independent implementation). Usage:

    python3 tools/halfband_design.py [num_coefs] [transition_bw]

Prints the coefficients (even indices -> path A, odd -> path B) and the
magnitude response at a few normalised frequencies (fraction of the 2x rate).
"""
import cmath
import math
import sys


def transition_params(tbw):
    k = math.tan((1 - tbw * 2) * math.pi / 4) ** 2
    kk = (1 - k * k) ** 0.25
    e = 0.5 * (1 - kk) / (1 + kk)
    e4 = e ** 4
    return k, e * (1 + e4 * (2 + e4 * (15 + 150 * e4)))


def _series(q, order, c, numerator):
    i, sign, acc = (0, 1, 0.0) if numerator else (1, -1, 0.0)
    while True:
        if numerator:
            term = q ** (i * (i + 1)) * math.sin((i * 2 + 1) * c * math.pi / order) * sign
        else:
            term = q ** (i * i) * math.cos(i * 2 * c * math.pi / order) * sign
        acc += term
        sign, i = -sign, i + 1
        if abs(term) <= 1e-100:
            return acc


def design(n, tbw):
    k, q = transition_params(tbw)
    order = n * 2 + 1
    out = []
    for idx in range(n):
        c = idx + 1
        ww = _series(q, order, c, True) * q ** 0.25 / (_series(q, order, c, False) + 0.5)
        w2 = ww * ww
        x = math.sqrt((1 - w2 * k) * (1 - w2 / k)) / (1 + w2)
        out.append((1 - x) / (1 + x))
    return out


def response_db(coefs, f):
    z = cmath.exp(2j * math.pi * f)
    a = b = 1
    for c in coefs[0::2]:
        a *= (c + z ** -2) / (1 + c * z ** -2)
    for c in coefs[1::2]:
        b *= (c + z ** -2) / (1 + c * z ** -2)
    return 20 * math.log10(abs(0.5 * (a + b / z)))


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    tbw = float(sys.argv[2]) if len(sys.argv) > 2 else 0.04
    cs = design(n, tbw)
    print("path A:", ", ".join(f"{c:.10f}f" for c in cs[0::2]))
    print("path B:", ", ".join(f"{c:.10f}f" for c in cs[1::2]))
    for f in (0.20, 0.23, 0.25, 0.27, 0.28, 0.30, 0.40, 0.49):
        print(f"  {f:.2f} fs: {response_db(cs, f):8.2f} dB")
