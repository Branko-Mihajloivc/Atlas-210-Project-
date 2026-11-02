"""ptt_board.html: PTT / data audio board (40 x 70 mm: 30 mm usable + 5 mm screw margin each side).
Through-hole parts on the top, tracks + edge pads on the underside (single-sided, GND by wires).
Checks connections and clearances, then writes: parts + tracks view, 1:1 etching artwork, wiring list."""
import math
import os
import sys

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ptt_board.html')
W, H = 40, 70
TRACK = 1.0
MIN_GAP = 0.6
PAD_R = 1.0

# all coordinates: seen from the parts side (x-ray through the board); mm
# holes: name -> (x, y, net). Net GND = joined by wires on the parts side.
HOLES = {
    'T1-1': (22.5, 7.5, 'RX2'), 'T1-2': (22.5, 13, 'GND'), 'T1-3': (32.5, 7.5, 'J47'), 'T1-4': (32.5, 13, 'J46'),
    'T2-1': (22.5, 51.5, 'TX1'), 'T2-2': (22.5, 57, 'GND'), 'T2-3': (32.5, 51.5, 'J46'), 'T2-4': (32.5, 57, 'J45'),
    'K-coilU': (16.4, 27, '12V'), 'K-coilL': (16.4, 35, 'COIL'),
    'K-COM1': (23.9, 27, 'KEY'), 'K-NC1': (28.9, 27, 'NC1'), 'K-NO1': (33.9, 27, 'GND'),
    'K-COM2': (23.9, 35, 'MICOUT'), 'K-NC2': (28.9, 35, 'MICIN'), 'K-NO2': (33.9, 35, 'PCAUD'),
    'D1-K': (13.6, 27, '12V'), 'D1-A': (13.6, 35, 'COIL'),
    'Q-E': (10.1, 33, 'GND'), 'Q-B': (10.1, 35.7, 'BASE'), 'Q-C': (10.1, 38.4, 'COIL'),
    'R4k7-1': (8, 54, 'PTT'), 'R4k7-2': (8, 44, 'BASE'),
    'R47kB-1': (8, 35.7, 'BASE'), 'R47kB-2': (8, 24.7, 'GND'),
    'LINK-1': (12, 43, 'KEY1'), 'LINK-2': (21.3, 23.5, 'KEY'),
    'C1-1': (19, 20, 'RX'), 'C1-2': (19, 15, 'RX1'), 'R10k-2': (19, 5, 'RX2'),
    'R47kT-1': (19.4, 50, 'TX1'), 'R47kT-2': (19.4, 40, 'PCAUD'),
    'R470-1': (16.6, 40, 'PCAUD'), 'R470-2': (16.6, 50, 'GND'),
    'GT1': (22.5, 20.5, 'GND'), 'GT2': (19.5, 57, 'GND'), 'GK': (33.9, 22.5, 'GND'),
    'GP1': (15.5, 66.5, 'GND'), 'GP2': (21.5, 66.5, 'GND'), 'GP3': (27.5, 66.5, 'GND'), 'GP4': (33.5, 66.5, 'GND'),
}
# GND holes, joined by wires as built
GND_MARKS = ['T1-2', 'T2-2', 'K-NO1', 'Q-E', 'R47kB-2', 'R470-2', 'GT1', 'GT2', 'GK', 'GP1', 'GP2', 'GP3', 'GP4']
# edge pads: name, x, y, w, h, net
PADS = [(n, 6.5 + i * 3 - 1, 65, 2, 5, net) for i, (n, net) in enumerate([
    ('12V', '12V'), ('PTT', 'PTT'), ('KEY', 'KEY1'), ('GND', 'GND'), ('RX', 'RX'), ('GND', 'GND'),
    ('MIC-IN', 'MICIN'), ('GND', 'GND'), ('MIC-OUT', 'MICOUT'), ('GND', 'GND')])]
PADS += [('J4-7', 37.4, 25, 2.6, 2, 'J47'), ('J4-6', 37.4, 29, 2.6, 2, 'J46'), ('J4-5', 37.4, 33, 2.6, 2, 'J45')]
SCREWS = [(2.5, 2.5), (37.5, 2.5), (2.5, 67.5), (37.5, 67.5)]
TIES = [(20.5, 62.8), (32.7, 62.5)]
# tracks: net, polyline
TRACKS = [
    ('12V', [(6.5, 65), (6.5, 63), (5.6, 62), (5.6, 27), (16.4, 27)]),
    ('PTT', [(9.5, 65), (9.5, 56), (8, 55), (8, 54)]),
    ('BASE', [(8, 44), (8, 35.7), (10.1, 35.7)]),
    ('COIL', [(16.4, 35), (12.2, 35), (12.2, 38.4), (10.1, 38.4)]),
    ('KEY1', [(12.5, 65), (12.5, 45), (12, 44), (12, 43)]),
    ('KEY', [(21.3, 23.5), (23.9, 23.5), (23.9, 27)]),
    ('RX', [(18.5, 65), (18.5, 63.5), (14.4, 60), (14.4, 37.8), (19, 37.8), (19, 20)]),
    ('RX2', [(19, 5), (22.5, 5), (22.5, 7.5)]),
    ('TX1', [(19.4, 50), (22.5, 50), (22.5, 51.5)]),
    ('PCAUD', [(16.6, 40), (21.5, 40), (21.5, 30), (33.9, 30), (33.9, 35)]),
    ('MICIN', [(24.5, 65), (24.5, 63.5), (25, 62.5), (25, 41), (28.9, 41), (28.9, 35)]),
    ('MICOUT', [(30.5, 65), (30.5, 63.5), (28, 62), (28, 43), (31.4, 43), (31.4, 32.4), (23.9, 32.4), (23.9, 35)]),
    ('GND', [(22.5, 13), (22.5, 20.5)]),
    ('GND', [(22.5, 57), (19.5, 57)]),
    ('GND', [(33.9, 27), (33.9, 22.5)]),
    ('J47', [(32.5, 7.5), (38, 7.5), (38, 25)]),
    ('J46', [(32.5, 13), (36, 13), (36, 30), (37.4, 30)]),
    ('J46', [(36, 30), (36, 51.5), (32.5, 51.5)]),
    ('J45', [(32.5, 57), (38, 57), (38, 35)]),
]
# parts for the top view: name, x, y, w, h (body), tall
BODIES = [
    ('T1', 21, 3, 13, 14.5, 1), ('K1', 15, 25.8, 20.3, 10.4, 1), ('T2', 21, 47, 13, 14.5, 1),
    ('D1', 12.5, 29, 2.2, 4, 0), ('Q1', 8.3, 32.2, 3.6, 7, 0),
    ('4.7 k', 6.7, 45, 2.6, 8, 0), ('47 k', 6.8, 25.6, 2.4, 6.4, 0), ('C1 1 u', 17.4, 14.5, 3.2, 6, 0),
    ('10 k', 17.7, 6, 2.6, 8, 0), ('47 k', 18.1, 41, 2.6, 8, 0), ('470', 15.3, 41, 2.6, 8, 0),
]


def seg_dist(a, b, c, d):
    """distance between segments ab and cd"""
    def pt(p, q, r):
        dx, dy = r[0] - q[0], r[1] - q[1]
        L = dx * dx + dy * dy
        t = 0 if L == 0 else max(0, min(1, ((p[0] - q[0]) * dx + (p[1] - q[1]) * dy) / L))
        return math.hypot(p[0] - q[0] - t * dx, p[1] - q[1] - t * dy)

    def cross(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    if cross(a, b, c) * cross(a, b, d) < 0 and cross(c, d, a) * cross(c, d, b) < 0:
        return 0.0
    return min(pt(a, c, d), pt(b, c, d), pt(c, a, b), pt(d, a, b))


def rect_segs(x, y, w, h):
    return [((x, y), (x + w, y)), ((x + w, y), (x + w, y + h)), ((x + w, y + h), (x, y + h)), ((x, y + h), (x, y))]


def check():
    items = []   # (net, segment, half width)
    for net, pts in TRACKS:
        for a, b in zip(pts, pts[1:]):
            items.append((net, (a, b), TRACK / 2))
    for x, y, net in {(h[0], h[1], h[2]) for h in HOLES.values()}:
        if y == 66.5:
            continue                # these holes sit inside their own edge pads
        items.append((net, ((x, y), (x, y)), PAD_R))
    pad_rects = [(net, x, y, w, h) for _, x, y, w, h, net in PADS]
    errors = []
    worst = 99
    for i, (n1, s1, r1) in enumerate(items):
        for n2, s2, r2 in items[i + 1:]:
            if n1 == n2 or (s1[0] == s2[0] and s1[1] == s2[1]):
                continue
            g = seg_dist(*s1, *s2) - r1 - r2
            worst = min(worst, g)
            if g < MIN_GAP - 1e-6:
                errors.append(f'{n1} - {n2}: gap {g:.2f} at {s1} / {s2}')
        for net, x, y, w, h in pad_rects:
            if net == n1:
                continue
            g = min(seg_dist(*s1, *e) for e in rect_segs(x, y, w, h)) - r1
            worst = min(worst, g)
            if g < MIN_GAP - 1e-6:
                errors.append(f'{n1} - pad {net}: gap {g:.2f} at {s1}')
    for (px, py, w, h, net1), (qx, qy, w2, h2, net2) in zip(
            [(x, y, w, h, net) for net, x, y, w, h in pad_rects], [(x, y, w, h, net) for net, x, y, w, h in pad_rects[1:]]):
        if net1 != net2 and py == qy and qx - (px + w) < MIN_GAP:
            errors.append(f'edge pads {net1} - {net2} too close')
    # screw and cable tie holes keep 0.6 mm from copper
    for (x, y), d in [(s, 3.2) for s in SCREWS] + [(t, 1.8) for t in TIES]:
        for n1, s1, r1 in items:
            g = seg_dist((x, y), (x, y), *s1) - r1 - d / 2
            if g < MIN_GAP:
                errors.append(f'hole {x},{y} - {n1}: gap {g:.2f}')
        for net, px, py, w, h in pad_rects:
            g = min(seg_dist((x, y), (x, y), *e) for e in rect_segs(px, py, w, h)) - d / 2
            if g < MIN_GAP:
                errors.append(f'hole {x},{y} - pad {net}: gap {g:.2f}')
    # connectivity: each net one copper piece (parts and the LINK are not copper, so they have their own nets)
    parent = list(range(len(items)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for i, (n1, s1, r1) in enumerate(items):
        for j, (n2, s2, r2) in enumerate(items[:i]):
            if n1 == n2 and seg_dist(*s1, *s2) < r1 + r2:
                parent[find(i)] = find(j)
    groups = {}
    for i, (net, s, r) in enumerate(items):
        groups.setdefault(net, set()).add(find(i))
    pieces = {n: len(g) for n, g in groups.items() if n != 'GND'}
    for name, x, y, w, h, net in PADS:
        if net == 'GND':
            continue
        if not any(n == net and min(seg_dist(*s, *e) for e in rect_segs(x, y, w, h)) <= r for n, s, r in items):
            errors.append(f'pad {name} ({net}) has no track')
    return errors, worst, pieces


errors, worst, pieces = check()
# BASE and PCAUD join through nothing but copper, so every net must be one piece
split = {n: c for n, c in pieces.items() if c != 1}
print('min gap %.2f mm' % worst, '| split nets:', split or 'none')
for e in errors:
    print('ERROR', e)
if errors or split:
    print('layout has problems')
    if '--force' not in sys.argv:
        sys.exit(1)


def copper(mono=False):
    ink = '#000' if mono else '#b55'
    e = []
    for net, pts in TRACKS:
        e.append('<polyline points="' + ' '.join(f'{a},{b}' for a, b in pts) + f'" fill="none" stroke="{ink}" stroke-width="{TRACK}" stroke-linejoin="round" stroke-linecap="round"/>')
    for _, x, y, w, h, net in PADS:
        e.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{ink}"/>')
    seen = set()
    for x, y, net in HOLES.values():
        if (x, y) in seen:
            continue
        seen.add((x, y))
        e.append(f'<circle cx="{x}" cy="{y}" r="{PAD_R}" fill="{ink}"/>')
        e.append(f'<circle cx="{x}" cy="{y}" r="0.35" fill="#fff"/>')
    for x, y in SCREWS:
        e.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="none" stroke="{ink}" stroke-width=".2"/>')
    for x, y in TIES:
        e.append(f'<circle cx="{x}" cy="{y}" r=".9" fill="none" stroke="{ink}" stroke-width=".2"/>')
    return ''.join(e)


def outline():
    return f'<rect x="0" y="0" width="{W}" height="{H}" fill="none" stroke="#000" stroke-width=".15"/>'


def parts_view():
    e = [outline(), copper()]
    for name, x, y, w, h, tall in BODIES:
        sw, d = (.3, '') if tall else (.18, ' stroke-dasharray=".6 .4"')
        e.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx=".4" fill="none" stroke="#111" stroke-width="{sw}"{d}/>')
        if h > w * 1.5 and not tall:
            e.append(f'<text transform="translate({x + w / 2 + .4},{y + h / 2}) rotate(-90)" font-size="1.2" text-anchor="middle" font-weight="700">{name}</text>')
        else:
            e.append(f'<text x="{x + w / 2}" y="{y + h / 2 + .5}" font-size="{1.8 if tall else 1.2}" text-anchor="middle" font-weight="700">{name}</text>')
    e.append('<polyline points="12,43 13.9,38.5 13.9,24 21.3,23.5" fill="none" stroke="#06c" stroke-width=".35"/>')
    e.append('<text x="14.6" y="24.6" font-size="1.2" fill="#06c" font-weight="700">LINK (KEY)</text>')
    for n in GND_MARKS:
        x, y, _ = HOLES[n]
        e.append(f'<circle cx="{x}" cy="{y}" r="1.35" fill="none" stroke="#080" stroke-width=".3"/>')
    for name, x, y, w, h, net in PADS:
        if y == 65:
            e.append(f'<text transform="translate({x + 1.4},71) rotate(-90)" font-size="1.2" text-anchor="end">{name}</text>')
        else:
            e.append(f'<text x="40.8" y="{y + 1.45}" font-size="1.2">{name}</text>')
    return ''.join(e)


ruler = ''.join(f'<line x1="{i}" y1="0" x2="{i}" y2="{2.5 if i % 10 == 0 else 1.2}" stroke="#000" stroke-width=".2"/>' for i in range(0, 51))
ruler = f'<svg style="width:54mm;height:7mm" viewBox="-2 -1 54 7" xmlns="http://www.w3.org/2000/svg" font-family="Arial, sans-serif"><line x1="0" y1="0" x2="50" y2="0" stroke="#000" stroke-width=".3"/>{ruler}<text x="0" y="5" font-size="2">0</text><text x="50" y="5" font-size="2" text-anchor="end">50 mm</text></svg>'
big = f'<svg style="width:104mm;height:164mm" viewBox="-3 -2 52 82" xmlns="http://www.w3.org/2000/svg" font-family="Arial, sans-serif">{parts_view()}</svg>'
# alignment outline with gaps at the edge pads, so any copper left on the line cannot bridge them
ticks = f'<polyline points="4.5,{H} 0,{H} 0,0 {W},0 {W},24" fill="none" stroke="#000" stroke-width=".15"/><polyline points="{W},36 {W},{H} 35.5,{H}" fill="none" stroke="#000" stroke-width=".15"/>'
art = f'<svg style="width:44mm;height:74mm" viewBox="-2 -2 44 74" xmlns="http://www.w3.org/2000/svg">{ticks}{copper(True)}</svg>'

rows = [
    ('12V', 'radio +12 V (red)', 'relay coil + D1 band'),
    ('PTT', 'VariBeam D9 (yellow)', '4.7 k to the BC547 base; 47 k base to GND'),
    ('KEY', 'KEY line (white). <b>Never the T line</b>', 'LINK, relay upper row COM; NO = GND'),
    ('RX + GND', 'PC-300 pin 22, coax', '1 &micro;F, 10 k, T1'),
    ('MIC-IN + GND', 'mic socket, coax', 'relay lower row NC'),
    ('MIC-OUT + GND', 'PC-200 pin 17, coax', 'relay lower row COM'),
    ('J4-7 / J4-6 / J4-5', 'J4 pins 7, 6, 5, twisted', 'T1 and T2 PC side; J4-6 floating, never to GND'),
]
table = ''.join(f'<tr><td><b>{a}</b></td><td>{b}</td><td>{c}</td></tr>' for a, b, c in rows)

html = '''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>PTT audio board</title>
<style>
@page { size: A4 portrait; margin: 10mm; }
body { margin: 0; background: #fff; color: #111; font: 9.5pt/1.35 "Segoe UI", Arial, sans-serif; }
.page { width: 190mm; margin: 0 auto; }
.pb { break-before: page; }
h1 { font-size: 14pt; margin: 0 0 1mm; }
h2 { font-size: 10.5pt; margin: 2mm 0 1mm; }
.sub { color: #555; font-size: 9pt; margin-bottom: 2mm; }
svg { display: block; }
ul { margin: 1mm 0 0 4mm; padding: 0; font-size: 9pt; }
table { border-collapse: collapse; width: 100%; font-size: 9pt; margin-top: 2mm; }
th, td { border: .6px solid #999; padding: .8mm 1.6mm; text-align: left; vertical-align: top; }
th { background: #eee; }
@media screen { body { background: #ccc; } .page { background: #fff; padding: 8mm; margin: 6mm auto; } }
</style></head><body><div class="page">
<h1>PTT / data audio board &mdash; parts and tracks</h1>
<div class="sub">40 &times; 70 mm, 2 &times; size, seen from the parts side with the tracks shown through the board. Top = LCD end, left = front of the radio.</div>
''' + big + '''
<ul>
<li><b>Single-sided board, as built.</b> Tracks and edge pads on the underside (red); no top copper.</li>
<li><b>Green rings = GND</b>, joined by insulated wires on the parts side and short jumpers underneath: GND pad next to KEY, 47 k and BC547 emitter; last GND pad, T1 and relay NO; GND pad next to RX, T2 and 470 &Omega;. Check with the meter that all GND points are joined.</li>
<li><b>LINK</b> (blue) = one insulated wire on the parts side, around the relay's coil end, taking KEY across the other tracks.</li>
<li>Relay: coil end on the left. Upper row = KEY pole, lower row = audio pole. D1 band at the top (12 V).</li>
<li>BC547, pins in a column: E top, B middle, C bottom, flat side towards the left edge. Check E-B-C on your part. Check each transformer pair with the meter: each side must be one winding.</li>
</ul>
<table><tr><th>Pad</th><th>Goes to</th><th>On the board</th></tr>''' + table + '''</table>
</div>
<div class="page pb">
<h1>Etching artwork &mdash; underside copper</h1>
<div class="sub">Print at 100 % / Actual size; the ruler must read 50 mm. Already correct for toner transfer: do not mirror it again.</div>
''' + art + ruler + '''
<ul>
<li>Tracks 1.0 mm, pads &oslash; 2 mm, clearance at least 0.6 mm (checked by the script).</li>
<li>Drill the small holes 0.9&ndash;1.2 mm, screws 3.2 mm, cable tie 1.8 mm.</li>
<li>The outline is an alignment aid, with gaps at the edge pads. Remove any copper left on it before soldering.</li>
<li>Before etching: stand the real relay, transformers, capacitor and BC547 on this print and check the pins.</li>
</ul>
</div>
<div class="page pb">''' + art + '''</div></body></html>'''
open(out, 'w', encoding='utf-8', newline='\n').write(html)
print('written')
