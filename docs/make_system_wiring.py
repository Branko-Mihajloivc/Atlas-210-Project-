"""Builds system_wiring.html: one A3 landscape page with every block and connection of the
Atlas 210X modifications. Details of each block are on wiring_A4 (section numbers in brackets)."""
import os

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'system_wiring.html')
E = []


def t(x, y, s, size=12, anchor='start', bold=False, italic=False):
    b = ' font-weight="700"' if bold else ''
    i = ' font-style="italic"' if italic else ''
    E.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}"{b}{i}>{s}</text>')


def box(x, y, w, h, title, lines=(), dashed=False, title_size=13):
    d = ' stroke-dasharray="8 5"' if dashed else ''
    fill = 'none' if dashed else '#f6f6f6'
    E.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="{fill}" stroke="#111" stroke-width="{1 if dashed else 1.4}"{d}/>')
    t(x + 8, y + 18, title, title_size, bold=True)
    for i, ln in enumerate(lines):
        t(x + 8, y + 36 + i * 15, ln, 11)


def wire(pts, label=None, lx=None, ly=None, anchor='middle', w=1.4, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    E.append('<polyline points="' + ' '.join(f'{a},{b}' for a, b in pts) + f'" fill="none" stroke="#111" stroke-width="{w}"{d}/>')
    if label:
        if lx is None:
            (x1, y1), (x2, y2) = pts[0], pts[1]
            lx, ly = (x1 + x2) / 2, min(y1, y2) - 5
        t(lx, ly, label, 10.5, anchor)


def to_board(pts, ty, jx, label=None, lx=None, ly=None, dash=None):
    """continue a wire from its last point to the board edge (x=1180) at height ty, jogging at jx"""
    x, y = pts[-1]
    wire(pts + [(jx, y), (jx, ty), (1180, ty)], label, lx, ly, dash=dash)


def dot(x, y):
    E.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="#111"/>')


W, H = 1640, 1110

# ---------------- groups ----------------
box(20, 60, 330, 1030, 'FRONT PANEL', dashed=True)
box(1160, 60, 460, 1030, 'ATLAS 210X BOARDS / CHASSIS', dashed=True)

# ---------------- front panel blocks ----------------
box(40, 90, 290, 78, 'LCD RG1602A 16x2  [1]', ['12 wires, twisted groups, 470 Ω at the Nano', 'contrast pot on the Nano board (V0 yellow)'])
box(40, 185, 290, 62, 'Tuning encoder MBL600', ['A / B, 100 detents, own decoder'])
box(40, 262, 290, 78, 'STEP + MENU buttons, MODE rocker  [3]', ['STEP 0 Ω, MENU 2.2 k, MODE (OPP) 22 k', 'A6 ladder, 100 nF at the buttons'])
box(40, 356, 290, 78, 'ATT / NORM / PRE rocker  [4]', ['SPDT ON-OFF-ON, common = GND', 'throws to the relay coil low ends'])
box(40, 450, 290, 78, 'AGC fast / slow rocker  [5]', ['DPDT: pole 1 +12 V to the 2N7000,', 'pole 2 GND -> D4 (no divider)'])
box(40, 546, 290, 78, 'AF / RF gain (concentric)  [7]', ['AF 10 k: VariBeam out -> PC-300 pin 12', 'RF 5 k: R5 2.2 k, R7 220 Ω -> PC-100 pin 14'])
box(40, 640, 290, 78, 'MIC / ALC gain (concentric)  [7]', ['MIC 20 k (20 k across), R13 10 k -> +CW', 'ALC 100 k (11 k across)'])
box(40, 734, 290, 92, 'VariBeam control  (own Nano)', ['encoder + push: CF / BW, presets, screen', '0.96" SSD1306 OLED (I2C 0x3C)', 'Nano: D2/D3/D4 knob, D5/D6/D7 -> VB'])
box(40, 842, 290, 62, 'Mic jack (front)', ['Shure 404C, PTT on the KEY line'])
box(40, 920, 290, 62, 'ATU-100 buttons', ['TUNE / AUTO / BYPASS -> ATU-100 only'])
box(40, 998, 290, 78, 'Function switch S3', ['OFF-CAL-REC-TRANS-CW (original)', 'CW = TX + +CW, CAL = cal line'])

# ---------------- VFO Nano ----------------
NX, NY, NW, NH = 520, 90, 170, 760
E.append(f'<rect x="{NX}" y="{NY}" width="{NW}" height="{NH}" rx="8" fill="#ececec" stroke="#111" stroke-width="2"/>')
t(NX + NW / 2, NY - 10, 'VFO NANO', 15, 'middle', True)
t(NX + NW / 2, NY + NH + 18, 'sketch atlas_vfo_fixed', 10.5, 'middle', italic=True)
left_pins = {'D7-D12': 129, 'D2 / D3': 216, 'A6': 301, 'D5 / D6': 395, 'D4': 489, '+5V / GND': 600}
right_pins = {'A4 / A5 (I2C)': 130, 'A0 / A1': 330, 'A3': 400, 'A2': 470, 'D13': 520, 'A7': 590, 'D0 / D1 / RST': 760}
for n, y in left_pins.items():
    t(NX + 8, y + 4, n, 11.5, bold=True)
for n, y in right_pins.items():
    t(NX + NW - 8, y + 4, n, 11.5, 'end', True)

# front panel -> Nano
wire([(330, 129), (NX, 129)], 'LCD 12 wires (5 twisted groups + V0)')
wire([(330, 216), (NX, 216)], 'encoder A / B')
wire([(330, 301), (NX, 301)], 'A6 + GND (twisted, green)')
wire([(330, 489), (NX, 489)], 'D4 orange (AGC pole 2)')
# ATT/PRE: rocker -> relay coils (inside), Nano D5/D6 sense from the coils
to_board([(330, 395), (420, 395), (420, 975)], 975, 1170, 'ATT / PRE coil low ends', 375, 390)
to_board([(330, 460), (440, 460), (440, 1010)], 690, 1050, '+12 V switched (AGC) -> 2N7000', 600, 1005)
to_board([(NX, 395), (470, 395), (470, 950)], 950, 1170, 'D5 grey / D6 red (via 1N4148 at the coils)', 600, 945)
# mic / ALC / AF / RF pots to boards
to_board([(330, 570), (400, 570), (400, 1055)], 1055, 1170, 'AF / RF / MIC / ALC pot wiring (original wires) [7]', 600, 1050)

# ---------------- Si5351 ----------------
box(760, 90, 280, 120, 'Si5351 module (25 MHz)', ['PLLA -> CLK0 LO, even divider (spur-free choice)', 'PLLB -> CLK1 BFO 5.520 MHz (+800 Hz CW,', '          +3.3 kHz MODE OPP)', 'CLK2 calibrator / generator (off)'])
wire([(NX + NW, 130), (760, 130)], 'I2C SDA / SCL')
box(760, 230, 130, 46, 'LPF CLK0', [])
box(910, 230, 130, 46, 'LPF CLK1', [])
wire([(825, 210), (825, 230)])
wire([(975, 210), (975, 230)])
to_board([(825, 276), (825, 300)], 290, 1150, 'LO (CLK0) coax', 880, 314)
to_board([(975, 276), (975, 286)], 360, 1140, 'BFO (CLK1) coax', 1015, 300)
to_board([(1040, 170)], 890, 1170, 'CLK2 thin coax [9]', 1100, 165, dash='6 3')

# ---------------- Nano right side -> boards ----------------
to_board([(NX + NW, 330)], 270, 1130, 'A0 / A1 S-meter: 10 k + 100 nF each, 1 k across 15-16', 900, 325)
to_board([(NX + NW, 400)], 425, 1120, 'A3 band switch ladder', 900, 395)
to_board([(NX + NW, 470)], 478, 1110, 'A2 white, 1N4148 at the Nano (band to KEY line)', 900, 465)
wire([(NX + NW, 520), (740, 520), (740, 540)])
box(700, 540, 130, 46, 'NPN BC547', ['4.7 k base, 47 k B-E'])
to_board([(830, 563)], 494, 1100, 'D13 CAT PTT (later)', 940, 558)
wire([(NX + NW, 590), (720, 590), (720, 640)])
box(660, 640, 230, 62, 'A7 network at S3  [6]', ['+CW 1N4148 22 k, CAL 1N4148 47 k,', '10 k to GND (one blue wire to A7)'])
to_board([(890, 670)], 820, 1090, '+CW (C6 / R14) and CAL line', 990, 665)

# ---------------- J4 ----------------
box(760, 730, 330, 150, 'J4 rear socket (old EXT OSC)  [8] [10]', ['1 RST  (1 µF in series, now 10 nF: reset = power switch)', '2 TX green   3 RX white   4 GND black', '5 audio in  6 audio GND (floating)  7 audio out', '8 PC PTT -> relay coil (keys + mutes mic)', '9 spare', 'USB-TTL adapter outside: CAT, uploads, PTT (RTS)'])
wire([(NX + NW, 760), (760, 760)], 'serial (twisted 4)', 725, 755)
box(1000, 900, 140, 46, 'T1 / T2 600:600', [])
wire([(1070, 880), (1070, 900)])
to_board([(1140, 912)], 640, 1080)
to_board([(1140, 930)], 565, 1070)
box(800, 900, 160, 46, 'PTT relay DPDT', ['coil +12 V, flyback'])
wire([(880, 880), (880, 900)])
to_board([(960, 923), (980, 923), (980, 990)], 510, 1060, 'KEY line + mic hot (NC)', 1020, 985)

# ---------------- boards (right) ----------------
bx = 1180
box(bx, 235, 420, 85, 'PC-100 (1st mixer / IF / relay)', ['LO in from CLK0  |  pins 15 / 16 S-meter bridge', 'pin 14 RF gain  |  pin 20 KEY line (LOW = TX)', 'pin 18 WHT/BLK +CW'])
box(bx, 330, 420, 60, 'BFO / carrier input', ['original PC-500 output point (PC-500 removed)'])
box(bx, 395, 420, 60, 'S4 band switch', ['resistor ladder on the band wafer -> A3'])
box(bx, 460, 420, 60, 'KEY line', ['A2 diode, D13 NPN, J4 PTT relay, mic PTT, S3'])
box(bx, 525, 420, 80, 'PC-200', ['pin 17 mic in  <- J4 audio in (47 k / 470 Ω)', 'pin 16 mic gain top  <- R13 10 k from +CW', 'pin 19 AGC  |  CA3086 mic / S-meter amp'])
box(bx, 615, 420, 95, 'PC-300D', ['pin 22 AF out -> VariBeam DSP + J4 audio out', 'pin 12 AF in <- AF gain wiper', 'AGC hold node <- 2N7000 + R fast (AGC fast)', 'R309 AGC threshold 5 k multi-turn'])
box(bx, 720, 420, 60, 'VariBeam DSP board', ['pin 22 -> DSP -> AF gain pot -> pin 12'])
box(bx, 790, 420, 60, 'S3 function switch / C6-R14', ['+CW 0 V REC/SSB, +13 V CW; brown = GND'])
box(bx, 860, 420, 60, 'PC-820 cal injection point', ['CLK2 coax lands here (to locate)'])
box(bx, 930, 420, 95, 'RX line after the antenna relay', ['ATT relay (10 dB π-pad) + PRE relay', 'Finder 30.22 DPDT, both poles RF', 'coil high end +12 V, low end to the rocker', 'never in the TX path'])
box(bx, 1035, 420, 45, 'Pots wiring / mic jack / ATU-100 (separate)', [])

# VariBeam link (front control <- -> DSP board)
wire([(330, 780), (360, 780), (360, 1080), (1150, 1080), (1150, 750), (1180, 750)], 'VariBeam Nano D5/D6/D7 -> DSP board (emulated encoder)', 750, 1075)

# legend
t(20, 30, 'Atlas 210X — system wiring of the modifications (YT3CT, 2026-10)', 18, bold=True)
t(20, 48, 'Block level. Component details: wiring_A4 section numbers in [ ]. Diode band = cathode. The Nano never sees more than 5 V: diodes / dividers at the source end.', 11)

svg = f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" font-family="Segoe UI, Arial, sans-serif">' + ''.join(E) + '</svg>'
html = '''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>Atlas 210X system wiring</title>
<style>
@page { size: A3 landscape; margin: 8mm; }
body { margin: 0; background: #fff; color: #111; font: 9pt/1.3 "Segoe UI", Arial, sans-serif; }
.page { width: 404mm; margin: 0 auto; }
svg { width: 100%; height: auto; display: block; }
@media screen { body { background: #ccc; } .page { background: #fff; padding: 8mm; margin: 6mm auto; } }
</style></head><body><div class="page">
''' + svg + '''
</div></body></html>'''
open(out, 'w', encoding='utf-8', newline='\n').write(html)
print('written')
