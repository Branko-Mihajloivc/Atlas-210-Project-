"""Builds docs/controls_wiring.html: one landscape page, VFO Nano buttons / rockers / relays / sense wires."""
import os
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'controls_wiring.html')
E = []


def line(x1, y1, x2, y2, w=1.4, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    E.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#111" stroke-width="{w}"{d}/>')


def poly(pts, w=1.4):
    E.append('<polyline points="' + ' '.join(f'{x},{y}' for x, y in pts) + f'" fill="none" stroke="#111" stroke-width="{w}"/>')


def text(x, y, t, size=12, anchor='start', bold=False):
    b = ' font-weight="700"' if bold else ''
    E.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}"{b}>{t}</text>')


def dot(x, y):
    E.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="#111"/>')


def res_h(x1, x2, y, label, ly=-8):
    """horizontal resistor between x1 and x2 (zigzag in the middle 36 px)"""
    m = (x1 + x2) / 2
    a, b = m - 18, m + 18
    line(x1, y, a, y)
    pts = [(a, y)]
    for i in range(6):
        pts.append((a + 3 + i * 6, y + (-6 if i % 2 == 0 else 6)))
    pts.append((b, y))
    poly(pts)
    line(b, y, x2, y)
    text(m, y + ly, label, 11, 'middle')


def res_v(x, y1, y2, label, lx=10):
    m = (y1 + y2) / 2
    a, b = m - 18, m + 18
    line(x, y1, x, a)
    pts = [(x, a)]
    for i in range(6):
        pts.append((x + (-6 if i % 2 == 0 else 6), a + 3 + i * 6))
    pts.append((x, b))
    poly(pts)
    line(x, b, x, y2)
    text(x + lx, m + 4, label, 11)


def diode_h(x1, x2, y, band_right, label=None):
    """horizontal diode in the middle of x1..x2; band_right = cathode at the right"""
    m = (x1 + x2) / 2
    a, b = m - 8, m + 8
    line(x1, y, a, y)
    line(b, y, x2, y)
    if band_right:
        E.append(f'<polygon points="{a},{y-7} {a},{y+7} {b},{y}" fill="none" stroke="#111" stroke-width="1.4"/>')
        line(b, y - 7, b, y + 7, 2)
    else:
        E.append(f'<polygon points="{b},{y-7} {b},{y+7} {a},{y}" fill="none" stroke="#111" stroke-width="1.4"/>')
        line(a, y - 7, a, y + 7, 2)
    if label:
        text(m, y - 11, label, 10, 'middle')


def diode_v_up(x, y1, y2):
    """vertical diode between y1 (top) and y2, cathode (band) at the top: flyback across a coil"""
    m = (y1 + y2) / 2
    a, b = m - 8, m + 8
    line(x, y1, x, a)
    line(x, b, x, y2)
    E.append(f'<polygon points="{x-7},{b} {x+7},{b} {x},{a}" fill="none" stroke="#111" stroke-width="1.4"/>')
    line(x - 7, a, x + 7, a, 2)


def gnd(x, y):
    line(x, y, x, y + 8)
    line(x - 10, y + 8, x + 10, y + 8)
    line(x - 6, y + 12, x + 6, y + 12)
    line(x - 2, y + 16, x + 2, y + 16)


def cap_v(x, y1, y2, label):
    m = (y1 + y2) / 2
    line(x, y1, x, m - 3)
    line(x - 9, m - 3, x + 9, m - 3, 1.8)
    line(x - 9, m + 3, x + 9, m + 3, 1.8)
    line(x, m + 3, x, y2)
    text(x + 12, m + 4, label, 11)


def button_v(x, y1, y2, label):
    """push button (normally open) in a vertical wire"""
    m = (y1 + y2) / 2
    line(x, y1, x, m - 8)
    line(x, m + 8, x, y2)
    E.append(f'<circle cx="{x}" cy="{m-8}" r="2" fill="none" stroke="#111"/>')
    E.append(f'<circle cx="{x}" cy="{m+8}" r="2" fill="none" stroke="#111"/>')
    line(x - 10, m - 12, x - 10, m + 12, 1.2)
    line(x - 10, m, x - 16, m, 1.2)
    text(x, y1 - 6, label, 11, 'middle', True)


def coil(x, y1, y2, label):
    line(x, y1, x, y1 + 6)
    E.append(f'<rect x="{x-12}" y="{y1+6}" width="24" height="{y2-y1-12}" fill="none" stroke="#111" stroke-width="1.4"/>')
    line(x, y2 - 6, x, y2)
    text(x - 18, (y1 + y2) / 2 + 4, label, 11, 'end')


# ---------------- Nano ----------------
E.append('<rect x="40" y="50" width="120" height="720" rx="6" fill="#f3f3f3" stroke="#111" stroke-width="1.6"/>')
text(100, 40, 'VFO Nano', 15, 'middle', True)
pins = {'+5V': 90, 'A6': 150, 'GND': 230, 'D5': 345, 'D6': 445, 'D4': 545, 'A2': 655, 'A7': 730}
for name, y in pins.items():
    text(150, y + 4, name, 13, 'end', True)
    line(160, y, 170, y, 1.4)

# front panel box
E.append('<rect x="800" y="55" width="290" height="560" rx="8" fill="none" stroke="#777" stroke-width="1.2" stroke-dasharray="7 5"/>')
text(945, 48, 'FRONT PANEL', 13, 'middle', True)

# ---------------- 1. buttons on A6 ----------------
text(300, 72, '1. Buttons STEP / MODE / MENU (one wire A6 + GND to the front)', 12, 'start', True)
line(170, 90, 240, 90)
res_v(240, 90, 150, '10 k pull-up (at Nano)')
dot(240, 150)
line(170, 150, 1000, 150)
line(170, 230, 1060, 230)
dot(240, 230) if False else None
# STEP: direct
dot(850, 150)
button_v(850, 150, 230, 'STEP')
dot(850, 230)
# MODE: 2.2k
dot(920, 150)
button_v(920, 150, 185, 'MODE')
res_v(920, 185, 230, '2.2 k', 8)
dot(920, 230)
# MENU: 4.7k
dot(990, 150)
button_v(990, 150, 185, 'MENU')
res_v(990, 185, 230, '4.7 k', 8)
dot(990, 230)
# 100 nF
line(1000, 150, 1050, 150)
cap_v(1050, 150, 230, '100 nF')
text(560, 142, 'A6 wire', 11, 'middle')
text(560, 222, 'GND wire', 11, 'middle')

# ---------------- 2. ATT / NORM / PRE ----------------
text(300, 285, '2. ATT / NORM / PRE  (relays inside, rocker at the front)', 12, 'start', True)
# ATT relay at x=600, coil from +12 (y=300) to node y=345
text(600, 300, '+12 V', 11, 'middle')
coil(600, 305, 335, 'ATT relay coil')
line(600, 335, 600, 345)
dot(600, 345)
diode_v_up(640, 305, 345)
line(600, 305, 640, 305)
line(600, 345, 640, 345)
text(648, 330, 'flyback', 10)
# PRE relay at x=420, coil from +12 (y=400) to node y=445
text(420, 400, '+12 V', 11, 'middle')
coil(420, 405, 435, 'PRE relay coil')
line(420, 435, 420, 445)
dot(420, 445)
diode_v_up(460, 405, 445)
line(420, 405, 460, 405)
line(420, 445, 460, 445)
text(468, 430, 'flyback', 10)
# sense diodes (band towards the coil node)
line(170, 345, 500, 345)
diode_h(500, 560, 345, True, '1N4148')
line(560, 345, 600, 345)
line(170, 445, 330, 445)
diode_h(330, 390, 445, True, '1N4148')
line(390, 445, 420, 445)
# rocker ON-OFF-ON at the front
line(600, 345, 930, 345)
line(460, 445, 930, 445) if False else line(420, 445, 420, 445)
line(460, 445, 930, 445)
E.append('<circle cx="930" cy="345" r="3" fill="none" stroke="#111"/>')
E.append('<circle cx="930" cy="445" r="3" fill="none" stroke="#111"/>')
E.append('<circle cx="990" cy="395" r="3" fill="none" stroke="#111"/>')
line(990, 395, 936, 372, 1.4)
line(990, 395, 1050, 395)
gnd(1050, 395)
text(990, 432, 'ON-OFF-ON', 10, 'middle')
text(900, 338, 'ATT', 11, 'end', True)
text(900, 460, 'PRE', 11, 'end', True)
text(1000, 385, 'common', 10)
text(1000, 418, '(centre = NORM)', 10)
text(240, 337, 'D5 sense wire', 11, 'middle')
text(240, 437, 'D6 sense wire', 11, 'middle')

# ---------------- 3. AGC ----------------
text(300, 500, '3. AGC fast / slow  (rocker at the front)', 12, 'start', True)
text(1060, 548, '+12 V', 11, 'end')
line(1000, 545, 1060, 545) if False else None
E.append('<circle cx="1000" cy="545" r="3" fill="none" stroke="#111"/>')
E.append('<circle cx="940" cy="545" r="3" fill="none" stroke="#111"/>')
line(1000, 545, 1020, 545)
line(943, 545, 995, 530, 1.4)
text(970, 528, 'SPST', 10, 'middle')
line(937, 545, 700, 545)
dot(700, 545)
# to 2N7000 board
line(700, 545, 700, 575)
E.append('<rect x="560" y="575" width="230" height="44" rx="4" fill="none" stroke="#111" stroke-width="1.2"/>')
text(675, 593, '2N7000 at PC-300D: gate via 100 Ω,', 10, 'middle')
text(675, 609, '100 k gate–GND, drain via R fast to hold node', 10, 'middle')
# divider to D4 (at the source end)
line(700, 545, 360, 545)
res_h(300, 360, 545, '22 k')
dot(300, 545)
line(170, 545, 300, 545)
res_v(300, 545, 590, '10 k', 10)
gnd(300, 590)
text(240, 537, 'D4 sense wire', 11, 'middle')

# ---------------- 4. A2 TX ----------------
line(170, 655, 270, 655)
diode_h(270, 330, 655, True, '1N4148')
line(330, 655, 420, 655)
text(426, 659, 'KEY line (PC-100 pin 20 / mic PTT) — LOW on TX', 11)
text(760, 659, '4. TX sense', 12, 'start', True)

# ---------------- 5. A7 S3 ----------------
text(760, 734, '5. S3 CW / CAL sense', 12, 'start', True)
line(170, 730, 260, 730)
dot(260, 730)
res_v(260, 730, 770, '10 k', 10)
gnd(260, 770)
line(260, 730, 300, 730)
dot(300, 730)
line(300, 730, 300, 712)
line(300, 730, 300, 748)
res_h(300, 380, 712, '22 k')
diode_h(380, 440, 712, False)
line(440, 712, 470, 712)
text(476, 716, '+CW (C6 / R14 junction)', 11)
res_h(300, 380, 748, '47 k', 18)
diode_h(380, 440, 748, False)
line(440, 748, 470, 748)
text(476, 752, 'S3 CAL line', 11)

svg = '<svg viewBox="0 0 1100 800" xmlns="http://www.w3.org/2000/svg" font-family="Segoe UI, Arial, sans-serif">' + ''.join(E) + '</svg>'
html = '''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>Atlas 210X controls wiring</title>
<style>
@page { size: A4 landscape; margin: 8mm; }
body { margin: 0; background: #fff; color: #111; font: 9pt/1.3 "Segoe UI", Arial, sans-serif; }
.page { width: 281mm; margin: 0 auto; }
h1 { font-size: 13pt; margin: 0 0 1mm; }
.sub { color: #555; font-size: 8.5pt; margin-bottom: 1mm; }
svg { width: 222mm; height: auto; display: block; margin: 0 auto; }
ul { margin: 1mm 0 0 4mm; padding: 0; font-size: 8.5pt; }
.pb { break-before: page; }
table { border-collapse: collapse; width: 100%; font-size: 9.5pt; margin-top: 2mm; }
th, td { border: .6px solid #999; padding: 1.2mm 2mm; text-align: left; vertical-align: top; }
th { background: #eee; }
tr.grp td { background: #f5f5f5; font-weight: 700; }
@media screen { body { background: #ccc; } .page { background: #fff; padding: 8mm; margin: 6mm auto; } }
</style></head><body><div class="page">
<h1>Atlas 210X &mdash; VFO Nano buttons, rockers, relays and sense wires</h1>
<div class="sub">YT3CT &middot; 2026-10 &middot; proposal &middot; diode band = cathode. Parts at the source end, plain wires to the Nano (5 V or less).</div>
''' + svg + '''
<ul>
<li>From the Nano to the front / relays: <b>A6, GND, D4, D5, D6</b>. A2 goes to the KEY line, A7 to the S3 network. The +12 V comes from the radio, never from the Nano.</li>
<li>D5 / D6 / A2: Nano internal pull-up, LOW = on. D6 becomes PRE sense only after STEP has moved to A6. Relay contacts (RF in / out of the &pi;-pad and preamp) are not shown.</li>
<li>A7: CW &asymp; 3.8 V, CAL &asymp; 2.1 V, REC / TRANS 0 V. Set CW_SENSE_FITTED 1 in the sketch once this network is wired.</li>
</ul>
</div>
<div class="page pb">
<h1>Wires leaving the VFO box</h1>
<div class="sub">Everything to run while the VFO box is open. Parts (diodes, resistors) at the far end unless noted.</div>
<table>
<tr><th>#</th><th>Nano pin</th><th>Wire</th><th>Far end</th><th>Parts / notes</th></tr>
<tr class="grp"><td colspan="5">Rear socket J4 &mdash; twisted 4-wire set</td></tr>
<tr><td>1</td><td>RST</td><td>grey</td><td>J4 pin 1</td><td>10 nF (103) in series, <b>at the Nano end</b></td></tr>
<tr><td>2</td><td>D1 (TX)</td><td>green</td><td>J4 pin 2</td><td>to adapter RX</td></tr>
<tr><td>3</td><td>D0 (RX)</td><td>white</td><td>J4 pin 3</td><td>from adapter TX</td></tr>
<tr><td>4</td><td>GND</td><td>black</td><td>J4 pin 4</td><td>chassis / Nano GND</td></tr>
<tr class="grp"><td colspan="5">Front panel buttons &mdash; twisted pair</td></tr>
<tr><td>5</td><td>A6</td><td>green</td><td>STEP / MODE / MENU</td><td>10 k pull-up A6&ndash;5 V at the Nano; 0 / 2.2 k / 4.7 k + 100 nF at the buttons</td></tr>
<tr><td>6</td><td>GND</td><td></td><td>buttons common</td><td>also the ATT/PRE rocker common</td></tr>
<tr class="grp"><td colspan="5">Sense inputs (Nano only listens, 5 V or less on the wire)</td></tr>
<tr><td>7</td><td>D5</td><td>grey</td><td>ATT relay coil low end</td><td>1N4148, band to the coil</td></tr>
<tr><td>8</td><td>D6</td><td>red</td><td>PRE relay coil low end</td><td>1N4148, band to the coil (after STEP moved to A6)</td></tr>
<tr><td>9</td><td>D4</td><td>orange</td><td>AGC switched +12 V</td><td>22 k series + 10 k to GND (divider at the far end)</td></tr>
<tr><td>10</td><td>A2</td><td>white</td><td>KEY line</td><td>1N4148 fitted <b>at the Nano end</b> (band to the KEY line), well insulated &mdash; the white wire carries +13 V</td></tr>
<tr><td>11</td><td>A7</td><td>blue</td><td>S3 network</td><td>network &rarr; <b>orange</b> to CAL (1N4148 + 47 k), <b>white/black</b> to +CW (1N4148 + 22 k), 10 k to GND</td></tr>
<tr class="grp"><td colspan="5">Output / supply</td></tr>
<tr><td>12</td><td>D13</td><td>white/red</td><td>KEY line / J4 pin 8 area</td><td>CAT PTT later: 4.7 k &rarr; NPN base (BC547), 47 k B&ndash;E, collector to KEY line, emitter GND (KEY line 2.1 mA). Leave open, label it</td></tr>
<tr><td>13</td><td>+5 V</td><td></td><td>front / relay area</td><td>small future circuits, &lt; 50 mA</td></tr>
<tr class="grp"><td colspan="5">RF</td></tr>
<tr><td>14</td><td>Si5351 CLK2</td><td>thin coax</td><td>old PC-820 cal injection point</td><td>leave the receiver end open for now; later 1&ndash;2 pF or 47 k in series</td></tr>
</table>
<ul>
<li>Not from the VFO box: J4 pins 5&ndash;8 (audio via transformers, PTT to the KEY line) and the rocker / relay +12 V wiring.</li>
<li>Route along the chassis, cross coax at 90&deg;. If a reading jumps on TX: 100 nF from that pin to GND at the Nano.</li>
</ul>
</div></body></html>'''
open(out, 'w', encoding='utf-8', newline='\n').write(html)
print('written')
