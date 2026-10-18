#!/usr/bin/env python3
"""Genera src/disc_tiles.h: un vinilo de 32x32 px (4x4 tiles) con 4 fotogramas de giro.
Colores Game Boy: 0 blanco (transparente en sprites), 1 gris claro, 2 gris oscuro, 3 negro."""
import math, sys
SIZE = 32
C = (SIZE - 1) / 2.0
def angdiff(a, b):
    return abs((a - b + math.pi) % (2 * math.pi) - math.pi)
def pixel(x, y, phase):
    dx, dy = x - C, y - C
    r = math.hypot(dx, dy)
    if r > 15.6: return 0                       # fuera del disco (transparente)
    if r > 14.4: return 3                       # borde
    if r <= 1.3: return 0                       # agujero central
    if r <= 6.6: return 1                       # etiqueta
    ang = math.atan2(dy, dx)
    if r > 7.6 and (angdiff(ang, phase) < 0.32 or angdiff(ang, phase + math.pi) < 0.32):
        return 1                                # reflejo giratorio
    return 3 if int(r) % 2 == 0 else 2          # surcos
frames = []
for f in range(4):
    phase = f * math.pi / 4
    frames.append([[pixel(x, y, phase) for x in range(SIZE)] for y in range(SIZE)])
def tile(img, tx, ty):
    out = []
    for row in range(8):
        lo = hi = 0
        for col in range(8):
            c = img[ty * 8 + row][tx * 8 + col]
            lo = (lo << 1) | (c & 1)
            hi = (hi << 1) | ((c >> 1) & 1)
        out += [lo, hi]
    return out
data = []
for img in frames:
    for ty in range(4):
        for tx in range(4):
            data += tile(img, tx, ty)
with open('src/disc_tiles.h', 'w') as f:
    f.write('/* Generado por tools/gen_disc.py - no editar a mano */\n#ifndef DISC_TILES_H\n#define DISC_TILES_H\n')
    f.write('static const unsigned char disc_tiles[%d] = {\n' % len(data))
    for i in range(0, len(data), 16):
        f.write('  ' + ','.join('0x%02X' % b for b in data[i:i+16]) + ',\n')
    f.write('};\n#endif\n')
if '--preview' in sys.argv:
    chars = ' .:#'
    for img in frames[:2]:
        for row in img[::2]:
            print(''.join(chars[c] * 2 for c in row))
        print('-' * 64)
print('disc_tiles.h:', len(data), 'bytes')
