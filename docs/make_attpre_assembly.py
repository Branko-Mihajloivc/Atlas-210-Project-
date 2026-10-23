"""attpre_assembly.html: component placement over the ATT / PRE PCB (top view, not mirrored),
taken from the EasyEDA Pro layout. Board coordinates in mm, origin bottom left."""
import os

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'attpre_assembly.html')
W, H = 48, 60
X0, X1, Y0, Y1 = -3, 51, -2, 68          # drawing window (relays overhang the top edge)

# designator, value, pads [(number, x, y, w, h)] with w/h already along x/y
P = {
    'C1':  ('100n', [(2, 9, 39.4, 1.7, 1.5), (1, 9, 42.6, 1.7, 1.5)]),
    'C3':  ('100n', [(2, 26.5, 35.4, 1.7, 1.5), (1, 26.5, 38.6, 1.7, 1.5)]),
    'C4':  ('100n', [(2, 17, 44.1, 1.7, 1.5), (1, 17, 40.9, 1.7, 1.5)]),
    'C5':  ('100n', [(2, 16, 13.4, 1.7, 1.5), (1, 16, 16.6, 1.7, 1.5)]),
    'C6':  ('100n', [(2, 33, 22.4, 1.7, 1.5), (1, 33, 25.6, 1.7, 1.5)]),
    'C7':  ('100n', [(2, 30, 7.4, 1.7, 1.5), (1, 30, 10.6, 1.7, 1.5)]),
    'C8':  ('10µ', [(2, 36, 22.4, 1.7, 1.5), (1, 36, 25.6, 1.7, 1.5)]),
    'C9':  ('10µ', [(2, 33, 7.4, 1.7, 1.5), (1, 33, 10.6, 1.7, 1.5)]),
    'CE':  ('100n', [(2, 18.5, 34.6, 1.7, 1.5), (1, 18.5, 31.4, 1.7, 1.5)]),
    'CE2': ('10µ', [(2, 23.5, 34.6, 1.7, 1.5), (1, 23.5, 31.4, 1.7, 1.5)]),
    'FB1': ('bead', [(2, 13, 40.5, 1.7, 1.2), (1, 13, 37.5, 1.7, 1.2)]),
    'L1':  ('100µH', [(2, 21, 42.6, 2.0, 1.2), (1, 21, 39.4, 2.0, 1.2)]),
    'L2':  ('22µH', [(2, 13, 21.6, 2.0, 1.2), (1, 13, 18.4, 2.0, 1.2)]),
    'R1':  ('1k5', [(2, 17.5, 18, 1.2, 1.7), (1, 20.5, 18, 1.2, 1.7)]),
    'R2':  ('390', [(2, 10, 13.5, 1.7, 1.2), (1, 10, 16.5, 1.7, 1.2)]),
    'R6':  ('47', [(2, 30, 23.5, 1.7, 1.2), (1, 30, 20.5, 1.7, 1.2)]),
    'R10': ('68', [(2, 31, 39.5, 1.7, 1.2), (1, 31, 42.5, 1.7, 1.2)]),
    'R11': ('150', [(2, 36.5, 43, 1.2, 1.7), (1, 33.5, 43, 1.2, 1.7)]),
    'R12': ('68', [(2, 39, 39.5, 1.7, 1.2), (1, 39, 42.5, 1.7, 1.2)]),
    'RE1': ('10', [(2, 20.5, 29, 1.2, 1.7), (1, 17.5, 29, 1.2, 1.7)]),
    'RE2': ('22', [(2, 21, 34.5, 1.7, 1.2), (1, 21, 31.5, 1.7, 1.2)]),
    'RF':  ('220', [(2, 26.5, 29.5, 1.7, 1.2), (1, 26.5, 32.5, 1.7, 1.2)]),
}
# label position (x, y, anchor) for designator; value goes on the next line
LBL = {
    'C1': (7.6, 41.3, 'end'), 'C3': (28.0, 37.3, 'start'), 'C4': (15.6, 43.7, 'end'),
    'C5': (14.6, 15.3, 'end'), 'C6': (33, 18.2, 'middle'), 'C7': (28.6, 9.3, 'end'),
    'C8': (36, 18.2, 'middle'), 'C9': (34.4, 9.3, 'start'), 'CE': (18.5, 38.05, 'middle'),
    'CE2': (23.5, 38.05, 'middle'), 'FB1': (11.8, 39.3, 'end'), 'L1': (22.4, 41.3, 'start'),
    'L2': (14.4, 20.3, 'start'), 'R1': (19, 15.6, 'middle'), 'R2': (8.6, 15.3, 'end'),
    'R6': (28.6, 22.3, 'end'), 'R10': (29.6, 41.3, 'end'), 'R11': (35, 41.0, 'middle'),
    'R12': (40.4, 41.3, 'start'), 'RE1': (19, 26.6, 'middle'), 'RE2': (21, 38.05, 'middle'),
    'RF': (28.0, 31.3, 'start'),
}
TRACKS = '''17,44.1,17,46.3,1.2;18.5,31.4,20.9,31.4,1.1;20.9,31.4,21,31.5,1.1;20.9,31.4,20.9,29.4,1.1;
20.9,29.4,20.5,29,1.1;21,31.5,23.4,31.5,1.1;23.4,31.5,23.5,31.4,1.1;13,18.4,14.5,18.4,1.1;
14.5,18.4,16,16.9,1.1;16,16.9,16,16.6,1.1;16,16.9,16.5,16.9,1.1;16.5,16.9,17.5,18,1.1;
13,18.4,11.9,18.4,1.1;11.9,18.4,10,16.5,1.1;39,42.7,36.8,42.7,1.1;36.8,42.7,36.5,43,1.1;
31,42.7,33.2,42.7,1.1;33.2,42.7,33.5,43,1.1;9,42.6,9,46.3,1.2;3,56,8.8,56,1.2;8.8,56,9,56.3,1.2;
45,56,39.3,56,1.2;39.3,56,39,56.3,1.2;30,10.6,33,10.6,1.2;30,10.6,30,20.5,1.1;30,10.6,26.4,10.6,1.2;
26.4,10.6,21.6,5.8,1.2;21.6,5.8,21.6,3.6,1.2;21.6,3.6,21.5,3.5,1.2;31,56.3,17,56.3,1.2;
13,35,13,37.5,1.1;26.5,35.4,26.5,32.5,1.1;15.3,29,17.5,29,1;15.3,29,15.3,30.8,1;15.3,30.8,14.1,32,1;
14.1,32,11.7,32,1;11.7,32,10.7,31,1;10.7,31,10.7,29,1;13,26.4,9.4,26.4,1;9.4,26.4,9,26.8,1;
9,26.8,9,39.4,1;13,26.4,23.4,26.4,1;23.4,26.4,26.5,29.5,1;21,39.4,18.5,39.4,1.1;18.5,39.4,17,40.9,1.1;
17,40.9,13.4,40.9,1.1;13.4,40.9,13,40.5,1.1;21,39.4,25.7,39.4,1.1;25.7,39.4,26.5,38.6,1.1;
21,42.6,26.1,42.6,1.1;26.1,42.6,28.5,40.2,1.1;28.5,40.2,28.5,30.1,1.1;28.5,30.1,33,25.6,1.1;
33,25.6,36,25.6,1.2;33,25.6,32.1,25.6,1.1;32.1,25.6,30,23.5,1.1;30,23.5,26,23.5,1.1;26,23.5,20.5,18,1.1;
31,51.3,39,51.3,1.2;9,51.3,17,51.3,1.2;39,46.3,39,42.7,1.1;31,46.3,31,42.7,1.1;13,21.6,13,26.4,1;
13,26.4,13,29,1;10,13.5,10,11.3,1.5;16,13.4,16,11.2,1.5;21,34.5,21,36.7,1.5;18.5,34.6,18.5,36.8,1.5;
23.5,34.6,23.5,36.8,1.5;33,22.4,33,20.2,1.5;36,22.4,36,20.2,1.5;30,7.4,30,5.2,1.5;33,7.4,33,5.2,1.5;
31,39.5,31,37.3,1.5;39,39.5,39,37.3,1.5'''
VIAS = [(10, 11.3), (16, 11.2), (21, 36.7), (18.5, 36.8), (23.5, 36.8), (33, 20.2), (36, 20.2),
        (30, 5.2), (33, 5.2), (31, 37.3), (39, 37.3)]

E = []
Y = lambda y: Y1 - y + Y0     # board y (up) -> svg y (down)


def rect(x, y, w, h, cls, rx=0.15):
    E.append(f'<rect class="{cls}" x="{x - w / 2:.2f}" y="{Y(y) - h / 2:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}"/>')


def text(x, y, s, size=1.0, anchor='middle', cls='t', rot=0):
    tr = f' transform="rotate({rot} {x:.2f} {Y(y):.2f})"' if rot else ''
    E.append(f'<text class="{cls}" x="{x:.2f}" y="{Y(y):.2f}" font-size="{size}" text-anchor="{anchor}"{tr}>{s}</text>')


# board, tracks, pads
E.append(f'<rect class="board" x="0" y="{Y(H):.2f}" width="{W}" height="{H}"/>')
for seg in TRACKS.replace('\n', '').split(';'):
    x1, y1, x2, y2, w = map(float, seg.split(','))
    E.append(f'<line class="cu" x1="{x1}" y1="{Y(y1):.2f}" x2="{x2}" y2="{Y(y2):.2f}" stroke-width="{w}"/>')
for d, (v, pads) in P.items():
    for n, x, y, w, h in pads:
        rect(x, y, w, h, 'pad', 0.1)
# relays from the bottom: body outline dashed, pins, pin names
for d, cx, name in (('K2', 13, 'PRE'), ('K1', 35, 'ATT')):
    E.append(f'<rect class="relay" x="{cx - 5.05:.2f}" y="{Y(65.15):.2f}" width="10.1" height="20.3" rx="0.6"/>')
    for x in (cx - 4, cx + 4):
        for y in (63.75, 56.25, 51.25, 46.25):
            E.append(f'<circle class="th" cx="{x}" cy="{Y(y):.2f}" r="1.25"/><circle class="hole" cx="{x}" cy="{Y(y):.2f}" r="0.6"/>')
    for y, s in ((63.75, 'coil'), (56.25, 'COM'), (51.25, 'NC'), (46.25, 'NO')):
        text(cx, y - 0.35, s, 0.8, 'middle', 'pin')
    text(cx, 66.1, f'{d} {name}', 1.3, 'middle', 'big')
# coax / power solder pads
for d, x, y, rot, lab in (('J1', 3, 53.6, 270, 'IN'), ('J2', 45, 53.6, 270, 'OUT')):
    E.append(f'<circle class="th" cx="{x}" cy="{Y(56):.2f}" r="1.25"/><circle class="hole" cx="{x}" cy="{Y(56):.2f}" r="0.5"/>')
    rect(x, 51.25, 5, 4, 'pad', 0.2)
    E.append(f'<circle class="hole" cx="{x}" cy="{Y(51.25):.2f}" r="0.75"/>')
    text(x, 58.2, f'{d} {lab}', 1.1, 'middle', 'big')
    text(x, 48.2, 'braid', 0.8, 'middle', 'pin')
E.append(f'<circle class="th" cx="21.5" cy="{Y(3.5):.2f}" r="1.25"/><circle class="hole" cx="21.5" cy="{Y(3.5):.2f}" r="0.5"/>')
rect(26.25, 3.5, 4, 5, 'pad', 0.2)
E.append(f'<circle class="hole" cx="26.25" cy="{Y(3.5):.2f}" r="0.75"/>')
text(18.9, 3.1, '+12 V', 1.0, 'end', 'big')
text(29.0, 3.1, 'GND', 1.0, 'start', 'big')
text(24, 7.0, 'J3', 1.1, 'middle', 'big')
# Q1 BFG135 (SOT-223): pins 1 E, 2 B, 3 E below, tab C above
rect(13, 32, 6.5, 3.5, 'body', 0.3)
for x, s in ((10.7, 'E'), (13, 'B'), (15.3, 'E')):
    rect(x, 29, 1.1, 2.5, 'pad', 0.1)
    text(x, 27.25, s, 0.8, 'middle', 'pin')
rect(13, 35, 3.6, 2.3, 'pad', 0.1)
text(13, 34.65, 'C', 0.8, 'middle', 'pin')
text(13, 31.65, 'Q1 BFG135', 1.0, 'middle', 'big')
# 2-pad parts: body outline + designator + value
for d, (v, pads) in P.items():
    (_, x1, y1, w1, h1), (_, x2, y2, w2, h2) = pads
    cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
    if abs(x1 - x2) > abs(y1 - y2):
        rect(cx, cy, abs(x1 - x2) + w1 + 0.5, max(h1, h2) + 0.5, 'body', 0.2)
    else:
        rect(cx, cy, max(w1, w2) + 0.5, abs(y1 - y2) + h1 + 0.5, 'body', 0.2)
    lx, ly, an = LBL[d]
    text(lx, ly, d, 0.85 if d in ('CE', 'RE2', 'CE2') else 1.0, an, 'big')
    if d not in ('CE', 'RE2', 'CE2'):
        text(lx, ly - 1.05, v, 0.8, an, 'val')
# ground wires (vias) and the etched text
for x, y in VIAS:
    E.append(f'<circle class="via" cx="{x}" cy="{Y(y):.2f}" r="0.9"/><circle class="hole" cx="{x}" cy="{Y(y):.2f}" r="0.45"/>')
text(44.3, 23, 'YT3CT ATT/PRE 2026', 2.0, 'middle', 'etch', 90)
svg = (f'<svg viewBox="{X0} {Y0} {X1 - X0} {Y1 - Y0}" xmlns="http://www.w3.org/2000/svg" '
       f'font-family="Arial, sans-serif">' + ''.join(E) + '</svg>')

parts = [
    ('Q1', 'BFG135', 'SOT-223, tab = collector (C), mark: pins E B E toward the bottom edge'),
    ('RF', '220 Ω', 'feedback, collector side → base'),
    ('RE1 / RE2', '10 Ω / 22 Ω', 'emitter'),
    ('R1 / R2', '1.5 k / 390 Ω', 'base bias'),
    ('R6', '47 Ω', '+12 V feed to the preamp'),
    ('R10, R12 / R11', '68 Ω / 150 Ω', 'π-pad (16 dB)'),
    ('C1, C3, C4, C5, C6, C7, CE', '100 nF', '1206'),
    ('C8, C9, CE2', '10 µF', '1206, 25 V or more'),
    ('L1 / L2', '100 µH / 22 µH', '1210; L1 rated ≥ 100 mA'),
    ('FB1', 'ferrite bead', '1206, right at the collector'),
    ('K1 ATT, K2 PRE', 'Finder 30.22.7.012', 'from the BOTTOM side, soldered on top'),
    ('J1 / J2', 'coax', 'core to the round pad, braid to the square pad'),
    ('J3', 'wires', '+12 V round pad, GND square pad'),
]
rows = ''.join(f'<tr><td>{a}</td><td>{b}</td><td>{c}</td></tr>' for a, b, c in parts)

html = '''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>ATT PRE assembly</title>
<style>
@page { size: A4 landscape; margin: 8mm; }
body { margin: 0; background: #fff; color: #111; font: 9.5pt/1.35 "Segoe UI", Arial, sans-serif; }
.page { width: 281mm; margin: 0 auto; }
h1 { font-size: 14pt; margin: 0 0 1mm; }
.sub { color: #555; font-size: 9pt; margin-bottom: 2mm; }
h2 { font-size: 10.5pt; margin: 2mm 0 1mm; border-bottom: 1.2px solid #111; }
.grid { display: grid; grid-template-columns: 134mm 1fr; gap: 6mm; }
svg { width: 134mm; height: auto; display: block; }
table { border-collapse: collapse; width: 100%; font-size: 8.8pt; }
th, td { border: .6px solid #999; padding: .5mm 1.5mm; text-align: left; vertical-align: top; }
th { background: #eee; }
ul, ol { margin: 1mm 0 0 4mm; padding: 0; }
li { margin: .5mm 0; }
svg .board { fill: none; stroke: #111; stroke-width: 0.25; }
svg line.cu { stroke: #c98a55 !important; stroke-opacity: .55; stroke-linecap: round; }
svg rect.pad, svg circle.th, svg circle.via { fill: #c98a55 !important; stroke: none !important; fill-opacity: .8; }
svg circle.hole { fill: #fff !important; stroke: none !important; }
svg rect.body { fill: none; stroke: #111; stroke-width: 0.18; }
svg rect.relay { fill: none; stroke: #111; stroke-width: 0.22; stroke-dasharray: 0.8 0.5; }
svg text { fill: #111; }
svg text.big { font-weight: 700; }
svg text.val, svg text.pin { fill: #444; }
svg text.etch { fill: #999 !important; font-weight: 700; }
@media screen { body { background: #ccc; } .page { background: #fff; padding: 8mm; margin: 6mm auto; } }
</style>
</head><body><div class="page">
<h1>ATT / PRE board &mdash; component placement</h1>
<div class="sub">Top view (component side, NOT mirrored), from the EasyEDA layout. Copper shown light, part outlines in black. Relays K1 / K2 go in from the bottom side.</div>
<div class="grid">
<div>''' + svg + '''</div>
<div>
<h2>Parts</h2>
<table><tr><th>Ref</th><th>Value</th><th>Note</th></tr>''' + rows + '''</table>
<h2>Order of work</h2>
<ol>
<li>After etching: beep every track against GND &mdash; no beep except the GND tracks.</li>
<li>Drill: relay pins and coax / J3 pads 1.2 mm, braid pads 1.5 mm, the 11 ground holes (small circles at the GND pads) 0.9 mm.</li>
<li>Bottom copper: countersink (&oslash; 3 mm drill, by hand) every relay hole and the J1 / J2 / J3 <b>core</b> holes. Ground holes and braid holes stay connected.</li>
<li>Ground wires: bare wire through each of the 11 ground holes, solder top and bottom.</li>
<li>SMD flat parts first (resistors, capacitors), then L1, L2, FB1, then Q1 (tab soldered fully &mdash; heat).</li>
<li>Relays last, from the bottom, raised ~1 mm; solder the pins on top. Coil pins stay outside the board: flyback and sense diodes on them, wires to the rocker / Nano.</li>
<li>Coax: core to the round pad, braid flat onto the square pad (top), short pigtails.</li>
</ol>
</div>
</div>
</div></body></html>'''
open(out, 'w', encoding='utf-8', newline='\n').write(html)
print('written')
