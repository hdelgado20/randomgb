#!/usr/bin/env python3
"""Convierte collection.txt (Artista<TAB>Album<TAB>Formato) en src/preload.h para meterlo en la ROM.
Formato: 0=vinilo 1=cd 2=casete 3=otro (tambien vale vinilo, vinyl, cd, casete, cassette...).
La Game Boy solo tiene letras ASCII: los acentos se quitan y lo demas pasa a '?'."""
import sys, unicodedata
src = sys.argv[1] if len(sys.argv) > 1 else 'collection.txt'
dst = sys.argv[2] if len(sys.argv) > 2 else 'src/preload.h'
MAXA, A_LEN, T_LEN = 170, 20, 27
def fold(s, n):
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(ch for ch in s if not unicodedata.combining(ch))
    s = ''.join(ch if 32 <= ord(ch) < 127 else '?' for ch in s).strip()
    return s[:n - 1]
def parse_fmt(s):
    s = s.strip().lower()
    if s[:1].isdigit(): return min(int(s[:1]), 3)
    if s.startswith(('vin', 'lp')): return 0
    if s.startswith('cd'): return 1
    if s.startswith(('cas', 'tape')): return 2
    return 3
rows = []
try:
    for line in open(src, encoding='utf-8-sig'):
        line = line.rstrip('\r\n')
        if not line.strip() or line.lstrip().startswith('#'): continue
        parts = line.split('\t')
        if len(parts) < 2: continue
        a, t = fold(parts[0], A_LEN), fold(parts[1], T_LEN)
        if not a or not t: continue
        rows.append((a, t, parse_fmt(parts[2]) if len(parts) > 2 else 3))
except FileNotFoundError:
    pass
rows = rows[:MAXA]
blob = bytearray()
for a, t, f in rows:
    blob += a.encode('ascii').ljust(A_LEN, b'\0')
    blob += t.encode('ascii').ljust(T_LEN, b'\0')
    blob.append(f)
if not blob: blob = bytearray(1)
with open(dst, 'w') as f:
    f.write('/* Generado por tools/mkdata.py desde collection.txt - no editar a mano */\n#ifndef PRELOAD_H\n#define PRELOAD_H\n')
    f.write('#define PRELOAD_N %d\n' % len(rows))
    f.write('static const unsigned char preload_blob[%d] = {\n' % len(blob))
    for i in range(0, len(blob), 16):
        f.write('  ' + ','.join('0x%02X' % b for b in blob[i:i+16]) + ',\n')
    f.write('};\n#endif\n')
print('preload.h: %d discos' % len(rows))
