"""Inserts the ATT / PRE board overview (relay chain, pi-pad, coils, sense) as page 1 of
preamp_bfg135.html. Page 2 (the BFG135 detail) stays as it is."""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
page_file = os.path.join(HERE, 'preamp_bfg135.html')
E = []


def L(x1, y1, x2, y2, w=1.5, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    E.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#111" stroke-width="{w}"{d}/>')


def T(x, y, s, size=12, anchor='start', bold=False):
    b = ' font-weight="700"' if bold else ''
    E.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}"{b}>{s}</text>')


def dot(x, y):
    E.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="#111"/>')


def ring(x, y):
    E.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="none" stroke="#111" stroke-width="1.4"/>')


def res_h(x1, x2, y, label, ly=-9):
    m = (x1 + x2) / 2
    a, b = m - 18, m + 18
    L(x1, y, a, y)
    pts = [(a, y)] + [(a + 3 + i * 6, y + (-6 if i % 2 == 0 else 6)) for i in range(6)] + [(b, y)]
    E.append('<polyline points="' + ' '.join(f'{p},{q}' for p, q in pts) + '" fill="none" stroke="#111" stroke-width="1.5"/>')
    L(b, y, x2, y)
    T(m, y + ly, label, 11, 'middle')


def res_v(x, y1, y2, label, lx=10):
    m = (y1 + y2) / 2
    a, b = m - 18, m + 18
    L(x, y1, x, a)
    pts = [(x, a)] + [(x + (-6 if i % 2 == 0 else 6), a + 3 + i * 6) for i in range(6)] + [(x, b)]
    E.append('<polyline points="' + ' '.join(f'{p},{q}' for p, q in pts) + '" fill="none" stroke="#111" stroke-width="1.5"/>')
    L(x, b, x, y2)
    T(x + lx, m + 4, label, 11)


def gnd(x, y):
    L(x, y, x, y + 8)
    L(x - 10, y + 8, x + 10, y + 8)
    L(x - 6, y + 12, x + 6, y + 12)
    L(x - 2, y + 16, x + 2, y + 16)


def pole(xc, y, side, label):
    """relay pole: common at (xc, y); NC throw at y-40 (rest), NO at y+40. side=+1 throws to the
    right of the common, -1 to the left"""
    xt = xc + side * 34
    ring(xc, y)
    ring(xt, y - 40)
    ring(xt, y + 40)
    L(xc, y, xt - side * 3, y - 37, 1.6)          # blade at rest on NC
    T(xt + side * 8, y - 44, 'NC', 10, 'start' if side > 0 else 'end')
    T(xt + side * 8, y + 52, 'NO', 10, 'start' if side > 0 else 'end')
    T(xc, y + 22 if side > 0 else y + 22, label, 10, 'middle')
    return xt


def diode_v_up(x, y1, y2):
    m = (y1 + y2) / 2
    L(x, y1, x, m - 8)
    L(x, m + 8, x, y2)
    E.append(f'<polygon points="{x-7},{m+8} {x+7},{m+8} {x},{m-8}" fill="none" stroke="#111" stroke-width="1.4"/>')
    L(x - 7, m - 8, x + 7, m - 8, 2)


def diode_h(x1, x2, y, band_right):
    m = (x1 + x2) / 2
    a, b = m - 8, m + 8
    L(x1, y, a, y)
    L(b, y, x2, y)
    if band_right:
        E.append(f'<polygon points="{a},{y-7} {a},{y+7} {b},{y}" fill="none" stroke="#111" stroke-width="1.4"/>')
        L(b, y - 7, b, y + 7, 2)
    else:
        E.append(f'<polygon points="{b},{y-7} {b},{y+7} {a},{y}" fill="none" stroke="#111" stroke-width="1.4"/>')
        L(a, y - 7, a, y + 7, 2)


Y = 140   # signal level
# ---- input ----
T(20, Y - 8, 'IN', 13, bold=True)
T(20, Y + 30, 'from PC-800 OUT', 10)
T(20, Y + 43, '(thin coax)', 10)
L(20, Y, 120, Y)
# ---- K1 ATT ----
T(330, 40, 'K1  ATT relay (Finder 30.22, DPDT)', 12, 'middle', True)
a_nc = pole(120, Y, 1, 'K1a')
b_nc = pole(540, Y, -1, 'K1b')
# bypass NC-NC
L(a_nc, Y - 40, b_nc, Y - 40)
T(330, Y - 46, 'bypass (NOR / PRE)', 10, 'middle')
# pi-pad NO-NO
L(a_nc, Y + 40, 220, Y + 40)
dot(220, Y + 40)
res_v(220, Y + 40, Y + 100, '68 Ω', 10)
gnd(220, Y + 100)
res_h(220, 440, Y + 40, '150 Ω')
dot(440, Y + 40)
res_v(440, Y + 40, Y + 100, '68 Ω', 10)
gnd(440, Y + 100)
L(440, Y + 40, b_nc, Y + 40)
T(330, Y + 128, '~16 dB π-pad (20 dB: 62 / 240 / 62 Ω)', 10, 'middle')
# K1b common -> K2a common
L(540, Y, 620, Y)
# ---- K2 PRE ----
T(840, 40, 'K2  PRE relay (Finder 30.22, DPDT)', 12, 'middle', True)
c_nc = pole(620, Y, 1, 'K2a')
d_nc = pole(1060, Y, -1, 'K2b')
L(c_nc, Y - 40, d_nc, Y - 40)
T(840, Y - 46, 'bypass (NOR / ATT)', 10, 'middle')
L(c_nc, Y + 40, 740, Y + 40)
E.append(f'<rect x="740" y="{Y + 20}" width="200" height="44" rx="5" fill="#f3f3f3" stroke="#111" stroke-width="1.4"/>')
T(840, Y + 38, 'BFG135 preamp ~10 dB', 12, 'middle', True)
T(840, Y + 55, '50 Ω in / out — page 3', 10, 'middle')
L(940, Y + 40, d_nc, Y + 40)
# output
L(1060, Y, 1150, Y)
T(1150, Y - 8, 'OUT', 13, 'end', True)
T(1150, Y + 30, 'to PC-100 pin 1', 10, 'end')
T(1150, Y + 43, '(REC. ANT. IN)', 10, 'end')

# ---- coils ----
def coil(x, label, sense, throw_label):
    yc = 350
    T(x, yc - 52, '+12 V', 11, 'middle')
    L(x, yc - 45, x, yc - 30)
    E.append(f'<rect x="{x-14}" y="{yc-30}" width="28" height="26" fill="none" stroke="#111" stroke-width="1.4"/>')
    T(x - 20, yc - 13, label, 11, 'end')
    L(x, yc - 4, x, yc + 20)
    dot(x, yc + 20)
    # flyback
    L(x, yc - 40, x + 40, yc - 40)
    diode_v_up(x + 40, yc - 40, yc + 20)
    L(x + 40, yc + 20, x, yc + 20)
    T(x + 50, yc - 6, 'flyback 1N4148', 10)
    # sense diode to Nano
    L(x, yc + 20, x, yc + 50)
    diode_h(x - 70, x, yc + 50, True)
    L(x - 140, yc + 50, x - 70, yc + 50)
    T(x - 142, yc + 54, sense, 11, 'end')
    # to rocker
    L(x, yc + 50, x, yc + 80)
    T(x + 8, yc + 78, throw_label, 10)
    return yc + 80


k1y = coil(330, 'K1 coil (720 Ω)', 'D5 (grey)', 'to rocker ATT throw')
k2y = coil(840, 'K2 coil (720 Ω)', 'D6 (red)', 'to rocker PRE throw')
# mechanical links coil -> poles
L(330, 290, 330, Y + 140, 1.0, '4 4')
L(840, 290, 840, Y + 80, 1.0, '4 4')
# rocker
ring(560, 480)
ring(620, 450)
ring(620, 510)
L(560, 480, 600, 480, 1.6)
T(560, 502, 'common = GND', 10, 'middle')
gnd(530, 480)
L(530, 480, 556, 480)
T(640, 454, 'ATT', 11, bold=True)
T(640, 514, 'PRE', 11, bold=True)
T(640, 486, 'NOR (centre)', 10)
L(330, k1y, 330, 450)
L(330, 450, 616, 450)
L(840, k2y, 840, 510)
L(840, 510, 624, 510)
T(560, 540, 'ATT / NOR / PRE rocker (SPDT ON-OFF-ON, front panel)', 11, 'middle', True)
# preamp supply note
T(990, 300, 'Preamp +12 V (Q2 option):', 11, 'start', True)
T(990, 316, 'permanent, or switched with', 10)
T(990, 330, 'PRE: PNP (BC557) emitter +12 V,', 10)
T(990, 344, 'base 4.7 k to the K2 coil low end,', 10)
T(990, 358, 'collector = preamp supply.', 10)

svg = '<svg viewBox="0 0 1170 555" xmlns="http://www.w3.org/2000/svg" font-family="Arial, sans-serif">' + ''.join(E) + '</svg>'

page1 = '''<div class="page" id="attpre">
  <h1>ATT / PRE board &mdash; relays, ~16 dB attenuator, BFG135 preamp</h1>
  <div class="sub">YT3CT &middot; 2026-10 &middot; proposal &middot; RX line between PC-800 OUT and PC-100 pin 1 (after the preselector). RX only &mdash; never in the TX path.</div>
  ''' + svg + '''
  <ul>
    <li>Relays at rest (both coils off) = <b>NOR</b>: signal goes IN &rarr; K1 bypass &rarr; K2 bypass &rarr; OUT.</li>
    <li>ATT: K1 pulls, both poles move to NO &rarr; signal through the &pi;-pad. PRE: K2 pulls &rarr; signal through the preamp. The rocker can only select one.</li>
    <li>Coil high ends permanently +12 V; the rocker grounds the coil low end. Flyback 1N4148 across each coil (band to +12 V). Sense 1N4148 from the coil low end to D5 / D6 (band to the coil) &mdash; Nano reads LOW = on.</li>
    <li>Thin coax (RG-174 / RG-316) in and out, short connections on the board, ground plane; keep the bypass track short and away from the preamp output.</li>
    <li>Check before cutting: the PC-800 &rarr; PC-100 coax must carry receive only (no TX signal).</li>
  </ul>
</div>
<div class="page pb" id="attpre3">
  <h1>ATT / PRE board &mdash; PCB design notes</h1>
  <div class="sub">Everything the board needs, for drawing the PCB. Values as on pages 1 and 3.</div>
  <div class="grid">
    <div>
      <h2>Connectors (board edge)</h2>
      <table>
        <tr><th>Ref</th><th>Signal</th><th>Goes to</th></tr>
        <tr><td>J1</td><td>RF IN + shield</td><td>coax from PC-800 OUT</td></tr>
        <tr><td>J2</td><td>RF OUT + shield</td><td>coax to PC-100 pin 1</td></tr>
        <tr><td>J3-1</td><td>+12 V</td><td>radio +12 V (coils, preamp)</td></tr>
        <tr><td>J3-2</td><td>GND</td><td>chassis</td></tr>
        <tr><td>J3-3</td><td>K1 coil low end</td><td>rocker ATT throw</td></tr>
        <tr><td>J3-4</td><td>K2 coil low end</td><td>rocker PRE throw</td></tr>
        <tr><td>J3-5</td><td>ATT sense</td><td>Nano D5 (grey)</td></tr>
        <tr><td>J3-6</td><td>PRE sense</td><td>Nano D6 (red)</td></tr>
      </table>
      <h2>Operation</h2>
      <table>
        <tr><th>Rocker</th><th>K1</th><th>K2</th><th>Signal path</th><th>D5 / D6</th></tr>
        <tr><td>NOR</td><td>off</td><td>off</td><td>bypass &ndash; bypass</td><td>HIGH / HIGH</td></tr>
        <tr><td>ATT</td><td>on</td><td>off</td><td>&pi;-pad (&minus;16 dB) &ndash; bypass</td><td>LOW / HIGH</td></tr>
        <tr><td>PRE</td><td>off</td><td>on</td><td>bypass &ndash; preamp (+10 dB)</td><td>HIGH / LOW</td></tr>
      </table>
    </div>
    <div>
      <h2>Parts list (whole board)</h2>
      <table>
        <tr><th>Ref</th><th>Part</th><th>Note</th></tr>
        <tr><td>K1, K2</td><td>Finder 30.22.7.012 DPDT 12 V</td><td>720 &Omega; coil, AgNi+Au, 5 mm pitch</td></tr>
        <tr><td>D1, D2</td><td>1N4148</td><td>flyback across K1 / K2 (band to +12 V)</td></tr>
        <tr><td>D3, D4</td><td>1N4148</td><td>sense: anode to J3-5 / J3-6, band to the coil low end</td></tr>
        <tr><td>R10, R12</td><td>68 &Omega;</td><td>&pi;-pad shunt (20 dB version: 62 &Omega;)</td></tr>
        <tr><td>R11</td><td>150 &Omega;</td><td>&pi;-pad series (20 dB version: 240 &Omega;) &mdash; 15.9 dB, Z<sub>in</sub> 49.3 &Omega;</td></tr>
        <tr><td>Q1 + preamp</td><td>BFG135 + parts of page 3</td><td>R1 1.5 k, R2 390, RF 220, RE1 10, RE2 22, R6 47, L1 100 &micro;H, L2 22 &micro;H, C1&ndash;C6, CE, ferrite bead</td></tr>
        <tr><td>Q2 (option)</td><td>BC557 + R13 4.7 k</td><td>switches the preamp supply with PRE</td></tr>
        <tr><td>C7</td><td>100 nF + 10 &micro;F</td><td>+12 V decoupling at J3</td></tr>
      </table>
      <h2>Layout</h2>
      <ul>
        <li>Double-sided, bottom = solid ground plane with many vias. RF tracks short and wide.</li>
        <li>Signal flow in a line: J1 &rarr; K1 &rarr; K2 &rarr; J2; pad between the K1 poles, preamp between the K2 poles.</li>
        <li>Preamp input and output apart (no loop); BFG135 tab on a copper area; ferrite bead right at the collector.</li>
        <li>Coils, sense diodes and supply in a corner near J3, away from the RF path.</li>
        <li>Coax shields to the ground plane right at J1 / J2.</li>
      </ul>
    </div>
  </div>
</div>
'''

s = open(page_file, encoding='utf-8').read()
NL = chr(10)
marker = '<div class="page pb">' + NL + '  <h1>RX preamp'
if marker not in s:
    s = s.replace('<div class="page">' + NL + '  <h1>RX preamp', marker, 1)
start = s.find('<div class="page" id="attpre">')
if start >= 0:                                  # remove the pages made by an earlier run
    s = s[:start] + s[s.index(marker):]
s = s.replace(marker, page1 + marker, 1)
if '.pb {' not in s:
    s = s.replace('  @media screen', '  .pb { break-before: page; }' + NL + '  @media screen', 1)
s = s.replace('<title>BFG135 RX preamp</title>', '<title>ATT PRE board</title>')
open(page_file, 'w', encoding='utf-8', newline=NL).write(s)
print('ok')
