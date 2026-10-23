"""attpre_pcb.html: PCB plan for the ATT / PRE board (48 x 60 mm): floor plan to scale,
design rules for home etching, net list for EasyEDA."""
import os

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'attpre_pcb.html')
S = 6.0   # px per mm
E = []


def R(x, y, w, h, label=None, sub=None, dash=False, fill='#f3f3f3'):
    d = ' stroke-dasharray="5 4"' if dash else ''
    f = 'none' if dash else fill
    E.append(f'<rect x="{x*S}" y="{y*S}" width="{w*S}" height="{h*S}" rx="3" fill="{f}" stroke="#111" stroke-width="1.3"{d}/>')
    if label:
        E.append(f'<text x="{(x+w/2)*S}" y="{(y+h/2)*S - (4 if sub else -4)}" font-size="11" text-anchor="middle" font-weight="700">{label}</text>')
    if sub:
        for i, line in enumerate(sub.split('|')):
            E.append(f'<text x="{(x+w/2)*S}" y="{(y+h/2)*S + 11 + i*12}" font-size="9.5" text-anchor="middle">{line}</text>')


def T(x, y, s, size=10, anchor='start', bold=False):
    b = ' font-weight="700"' if bold else ''
    E.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}"{b}>{s}</text>')


def line(pts, w=2.2, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    E.append('<polyline points="' + ' '.join(f'{a*S},{b*S}' for a, b in pts) + f'" fill="none" stroke="#111" stroke-width="{w}"{d}/>')


OX, OY = 6, 14          # board origin (mm) inside the drawing
W, H = 48, 60
E.append(f'<g transform="translate({OX*S},{OY*S})">')
R(0, 0, W, H, dash=False, fill='#ececec')
# relays overhanging the top edge (coil end outside the board)
R(2, -6, 20.3, 10.4, 'K2 PRE', 'coil end overhangs')
R(25.7, -6, 20.3, 10.4, 'K1 ATT', 'coil end overhangs')
# J1 / J2
R(-4, 6, 6, 6, 'J1')
R(46, 6, 6, 6, 'J2')
# preamp block (left), pad (right), power (bottom)
R(2, 16, 22, 30, 'Preamp', 'Q1 BFG135 (SOT-223)|L1 100 µH, L2 22 µH|RF, RE1, RE2, CE|R1, R2, C1-C5|ferrite bead')
R(27, 16, 19, 18, 'π-pad', 'R10 68 · R11 150|R12 68 (1206)')
R(27, 37, 19, 9, 'C7', '100 nF + 10 µF')
R(2, 49, 44, 9, 'Power / ground pads', 'J3: +12 V, GND · Q2 + R13 option · stitch pins')
# signal path arrows
line([(2, 9), (4, 9)])
line([(22.3, 3), (25.7, 3)])
line([(46, 9), (44, 9)])
E.append('</g>')
T(OX*S, (OY+H)*S + 22, '48 mm', 11, 'start', True)
T((OX+W)*S + 40, (OY+H/2)*S, '60 mm', 11, 'start', True)
T(OX*S, 18, 'Floor plan, to scale (6 px = 1 mm), component side', 11, 'start', True)
T((OX-2)*S, (OY+12)*S + 8, 'IN', 11, 'end', True)
T((OX+W+8)*S, (OY+12)*S + 8, 'OUT', 11, 'start', True)
svg = f'<svg viewBox="0 0 {int((OX+W+22)*S)} {int((OY+H+8)*S)}" xmlns="http://www.w3.org/2000/svg" font-family="Arial, sans-serif">' + ''.join(E) + '</svg>'

nets = [
    ('RF_IN', 'J1 signal, K2 COM-A'),
    ('PRE_BYP', 'K2 NC-A, K2 NC-B (short wide track)'),
    ('PRE_IN', 'K2 NO-A, C1'),
    ('BASE', 'C1, Q1 B, L2, RF-feedback (RF), (bias via L2)'),
    ('BIAS', 'L2, R1, R2, C5'),
    ('EMIT', 'Q1 E, RE1'),
    ('E2', 'RE1, RE2, CE'),
    ('COLL', 'Q1 C (tab), ferrite bead'),
    ('COLL2', 'ferrite bead, L1, C4, C3'),
    ('FB', 'C3, RF'),
    ('PRE_OUT', 'C4, K2 NO-B'),
    ('MID', 'K2 COM-B, K1 COM-A'),
    ('ATT_BYP', 'K1 NC-A, K1 NC-B (short wide track)'),
    ('PAD_IN', 'K1 NO-A, R10, R11'),
    ('PAD_OUT', 'R11, R12, K1 NO-B'),
    ('RF_OUT', 'K1 COM-B, J2 signal'),
    ('V12', 'J3-1, C7, R6 (Q2 emitter if fitted)'),
    ('V12F', 'R6, C6, R1, L1 (Q2 collector feeds R6 if fitted)'),
    ('GND', 'J1/J2 shields, J3-2, R2, R10, R12, RE2, CE, C5, C6, C7, bottom plane'),
]
rows = ''.join(f'<tr><td><code>{n}</code></td><td>{c}</td></tr>' for n, c in nets)

html = '''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>ATT PRE PCB plan</title>
<style>
@page { size: A4 landscape; margin: 8mm; }
body { margin: 0; background: #fff; color: #111; font: 9.5pt/1.35 "Segoe UI", Arial, sans-serif; }
.page { width: 281mm; margin: 0 auto; }
h1 { font-size: 14pt; margin: 0 0 1mm; }
.sub { color: #555; font-size: 9pt; margin-bottom: 2mm; }
h2 { font-size: 10.5pt; margin: 2mm 0 1mm; border-bottom: 1.2px solid #111; }
.grid { display: grid; grid-template-columns: 0.9fr 1.1fr; gap: 6mm; }
svg { width: 100%; height: auto; display: block; }
table { border-collapse: collapse; width: 100%; font-size: 9pt; }
th, td { border: .6px solid #999; padding: .6mm 1.6mm; text-align: left; vertical-align: top; }
th { background: #eee; }
ul { margin: 1mm 0 0 4mm; padding: 0; }
li { margin: .5mm 0; }
@media screen { body { background: #ccc; } .page { background: #fff; padding: 8mm; margin: 6mm auto; } }
</style></head><body><div class="page">
<h1>ATT / PRE board &mdash; PCB plan for EasyEDA</h1>
<div class="sub">48 &times; 60 mm, on the crystal filter can. IN top left &rarr; K2 PRE &rarr; K1 ATT &rarr; OUT top right. Preamp on the left (away from the Nano), pad on the right.</div>
<div class="grid">
<div>''' + svg + '''
<h2>Design rules (home etching)</h2>
<ul>
<li>Parts 1206 (resistors, capacitors), BFG135 SOT-223, chokes 1210 or axial.</li>
<li>RF tracks <b>1.5 mm</b>, power <b>2 mm</b>, others &ge; 1 mm; clearance &ge; <b>0.6 mm</b>.</li>
<li>Top: tracks + ground pour (0.8 mm clearance). Bottom: <b>unetched solid copper = ground plane</b>.</li>
<li>Top ground to bottom: wire pins through drilled holes, at least next to every GND pad of the RF parts, at J1 / J2 and around the preamp.</li>
<li>Relay holes: no relay pin is GND &mdash; clear the bottom copper (&oslash; 2.5 mm) around <b>every</b> relay pin and every other non-GND through-hole.</li>
<li>Coil pins overhang the edge: flyback 1N4148 and sense diode soldered directly on the coil pins, wires to the rocker / Nano.</li>
</ul>
</div>
<div>
<h2>Net list</h2>
<table><tr><th>Net</th><th>Connects</th></tr>''' + rows + '''</table>
<div class="sub" style="margin-top:2mm">Relay pin names are generic (COM / NC / NO, poles A and B): map them to the Finder 30.22 footprint (NC = rest position). K2 = PRE, K1 = ATT. Values on the ATT / PRE board sheet.</div>
</div>
</div>
</div></body></html>'''
open(out, 'w', encoding='utf-8', newline='\n').write(html)
print('written')
