# Atlas 210X — build notes and measurements

Test gear: FTdx10 as frequency reference, tinySA + step attenuator as signal source.

## VFO Nano (Si5351)
- Sketch `firmware/atlas_vfo_fixed`, board "ATmega328P (Old Bootloader)".
- I²C timeout (freeze fix), encoder ISR only counts steps, band switch hysteresis, EEPROM restore,
  no blank screen at start, RIT removed on TX, setup menu with exit.
- A `!` on the LCD (line 1, col 14) shows that at least one I²C timeout happened since power on.
- Frequency calibration: Si5351 crystal ~101 ppm high → CORR = +101000 ppb.
  Dial 14.099.0 gives a ~1 kHz tone from a carrier on 14.100.000.
- BFO = 5.520000 MHz. Opposite sideband suppressed, 300 Hz – 2.7 kHz flat.
- CLK1 (BFO) drive 8 mA tested: no change → left at 2 mA.
- CH340 Nano clones: driver 3.5.2019.1 works; a newer driver failed with "cannot set com-state".
- When the radio is switched off the Nano back-powers the LCD/Si5351 from USB — unplug USB first.

## S-meter
- Differential reading: A1 = PC-100 pin 15, A0 = PC-100 pin 16, 1 k across pins 15/16 in place of
  the removed meter, 10 k series + 100 nF at the radio end on both lines.
- Calibrated from S6 (~−90 dBm) to S9+40 with the tinySA; table in the sketch.
- LCD: 9 fields = S1…S9, two lines per field (3 dB each), then `+10`…`+40` as text.
  The S-meter line is only rewritten when it changes (less digital noise).
- **Never connect the Nano to the meter / RL101 side** — it carries +13 V on TX.

## Receiver
- Sensitivity: signal heard at −115 dBm.
- Antenna jack → GND 1–2 Ω on all bands (band switch, LPF and antenna relay OK).
- DC switching relay RL101 (PC-100) was dirty → cleaned. Big gain improvement.
- Old dipped tantalums replaced on PC-200 and PC-300D (C311 47 µF AGC hold → electrolytic).
- PC-200-D (built as the C design: R220 100 Ω + D207 1N4148, no R203 pot). R222 (S-meter zero)
  → 1 k multi-turn.
- PC-300D: R309 (AGC threshold) → 5 k multi-turn. Factory procedure: no signal → R222 for zero,
  −73 dBm → R309 for S9, repeat (they interact).
- AGC is audio derived (PC-300D). PC-200 pin 19: 4.53 V no signal, 5.25 V at −73 dBm
  (factory 4.53 → 5.16 V).
- PC-300D part numbers and values differ from the PC-300C schematic — identify parts by value and
  position. The Atlas AGC bulletin mod was not applied for that reason.
- AGC release is slow (S-meter falls back over several seconds) — this is the PC-300D hold time,
  not the Nano.
- The VariBeam DSP adds ~25 dB of audio; it is wired after the AGC pickoff: PC-300 pin 22 → VariBeam → AF gain → pin 12.
- AF/RF gain and MIC/ALC are now two concentric dual pots (values to confirm, see the wiring sheet).
- 20 m weaker: the chassis-mounted 20 m image filter had a loose paper coil form turning with the
  slug → glue the form, tune for minimum image. 4–6 dB less on 20 m is normal for the Atlas.
- PC-100: loose R101 trimmer (carrier balance, 100 Ω) with hand effect → replace, check TX carrier
  null, peak L103 (5520 kHz).
- "+10 V" in the Atlas voltage charts is the +8.5 V line (78L06 + 1N5221). 8.3 V is OK.
- The display cable radiates (a hand over the cable stops the noise) → series resistors in the LCD
  lines, see the wiring sheet.

## VariBeam DSP control Nano
- Sketch `firmware/VB_LocalDisplay_fixed`, board "ATmega328P (Old Bootloader)".
- Fixes: 2 ms per emulated encoder state (the VariBeam missed steps), counted detents, OLED reset
  pin clashed with the encoder switch, SH1106 column offset 0.
- Splash screen + resync at power on, CF/BW/mode in EEPROM, passband edges + trapezoid graph.
- Button: 1× CF/BW, 2× preset (SSB 1500/2400, SSB NARROW 1400/1800, CW 700/500, DIGI 1500/3000),
  3× screen when idle (OFF / DIM / ALWAYS ON), hold 2 s = resync.
- After uploading, power cycle the radio so the VariBeam and the Nano start in sync.
- The display is being changed to a 0.96" SSD1306 → the sketch needs the SSD1306 library
  instead of SH110X.

## Front plate (3D print)
- `3d/Atlas 210 - final.stl`, 239 × 88 × 9 mm (`Atlas 210 - original.stl` for comparison).
- MBL600 encoder (flange Ø61, body Ø43, cutout Ø44, 3 × M3 studs on PCD 50.8 at 120°):
  recess Ø61.8 × 1, through hole Ø44.4, 3 × Ø3.8 at (0, +25.4), (−22, −12.7), (+22, −12.7).
- 2 × 0.96" SSD1306 (PCB 27.3 × 27.8, glass 26.8 × 19.3): window 23 × 12, pocket 27.8 × 28.4,
  frame with 2 mm walls.
- 16×2 LCD (RG1602A): pocket as designed (PCB flush with the top edge), window 65 × 15.

## Planned
- TX test into a dummy load: A0/A1 voltages and I²C timeouts on TX, carrier suppression.
- Re-check birdies with the corrected frequency calibration.
- Low-pass filters on Si5351 CLK0 and CLK1 against spurs.
- Button ladder on A6 (ATT / MODE / AGC / spare), RX attenuator, AGC fast/slow — see the wiring sheet.
- USB extension to the back panel so both Nanos can be updated without opening the radio.

## Work 2026-10-18/19
- New 3D-printed front plate fitted.
- LCD rewired with twisted groups (see the wiring sheet, section 1): E and RS each twisted with a ground,
  +5 V / GND / V0 / backlight as one twisted group, data lines in pairs; 3 grounds on one GND pad.
  470 ohm series resistors at the Nano end. Fault found and fixed: V0 and backlight wire on the same pad.
- Copper tape (insulated both sides, grounded at one end) planned where the bundle passes the Si5351 LPF.
- Front panel pots as fitted: RF gain 5 k, AF gain 10 k, MIC gain 20 k, ALC 100 k (originals 10 k).
  Compensation (wiring sheet section 7): R5 2.2 k + R7 220 ohm; 20 k across MIC pot; 11 k across ALC pot.
- R13 (10 k, mic gain top) reconnected to the +CW line (C6 / R14 junction, WHT/BLK to ACC): 0 V in REC and
  SSB TX, +13 V in CW.
- CW gives only a blip on key-down: the original carrier oscillator board shifted the carrier into the crystal
  filter passband on +CW. With the Si5351 BFO fixed at 5.520 MHz the carrier is blocked by the filter.
  Fix planned in the sketch: CW sense on A7 (22 k / 10 k divider from +CW), BFO moved into the passband in CW
  (~5.5215 MHz, to tune), LO shifted by the same amount.
- Low SSB modulation reported (only OK at max mic gain): first fit the 11 k across the ALC pot, then check
  the mic type (needs a high-impedance mic) and the mic amp voltages on TX.

## Next (Nano session)
1. CW: A7 sense + BFO/LO shift in the sketch, then CW test into a dummy load.
2. LCD layout: glyph test, then the new layout (see docs/lcd_mockup.html).
3. Noise test after the LCD rewiring (antenna off, tune, hand-over-cable test).
4. Fit the pot compensation resistors, re-check modulation on air.
5. (done 2026-10) VariBeam sketch switched to SSD1306, new OLED working.
6. RX preamp idea: compare band noise antenna vs dummy load first.

## Ideas list (agreed 2026-10-20, not started)
Software (VFO Nano):
- VFO A/B + split (control scheme to decide).
- Tuning acceleration (10 Hz base step, bigger steps when turning fast).
- CAT control over USB (Kenwood-style protocol) - top wish. PTT over CAT needs a Nano-driven key line transistor.
- Si5351 CLK2 as test signal / calibrator; GPSDO for calibration (e.g. 1PPS-gated count of CLK2 for auto CORR).
- TX timeout: only if the Nano ever controls TX (CAT PTT / break-in).
Hardware:
- TX power / SWR display (PC-1100 bridge forward/reflected via dividers).
- Heat sink temperature + fan control.
- Supply voltage display - possibly on the VariBeam Nano / OLED (free analog pins there).
- Reverse polarity protection + fuse on the 12 V input.
Bigger projects:
- Semi-break-in CW with sidetone.
- Data-mode interface (line in/out + PTT via ACC) together with CAT.
Hardware mods (radio):
- Mic preamp for low-impedance dynamic mics (likely cure for the low modulation).
- AGC attack/release mod (Atlas bulletin, Engineering Supplement) - faster AGC recovery.
- Bass-response caps on PC-300 (Engineering Supplement) - fuller RX audio.
- IF tap (buffered 5.52 MHz) for an SDR panadapter.
- Socket the CA3086 on PC-200 (usual S-meter / mic amp failure part).
- Replace the loose R101 carrier balance trimmer (PC-100), re-null the carrier.
- PA check: bias, heat sink compound.
- Fan on the heat sink (needed for FT8 / high duty cycle).
Suggested order after the current list: CAT + PTT (+ TX timeout), power/SWR display, mic preamp, AGC mod.
J4 (EXT OSC, 9-pin noval, rear) reuse - planned:
- Clear all internal wires; where a lug joins 2+ wires, splice them (keep +CW to R13 / C6-R14, +13 V, CW GND paths).
  Afterwards check REC / SSB TX / CW and that all 9 pins are open.
- Proposed pins: 1 GND, 2 Nano TX (D1), 3 Nano RX (D0), 4 RST via 100 nF from DTR (rear programming),
  5 PTT (key line via transistor/opto), 6 audio out, 7 audio in (attenuated), 8 audio GND, 9 spare.
  Audio via 600:600 isolation transformers, shielded audio wires inside.

## Birdies 20 m, antenna disconnected (2026-10-20)
14.051 tone S7 · 14.067 garble + chirps/ticks S5-6 · 14.082 tone S2 · 14.125 weak tone · 14.134 hum S0 ·
14.139 hum/data S0 · 14.151 low tone S0 · 14.166 fast "cricket" S0 · 14.173 · 14.182 tone S0 · 14.188 hash S0 ·
14.194 tone S9 · 14.198 gurgling/data S8 · 14.206 tone S2 · 14.233 tone S2 · 14.241 tone S0 ·
14.256-14.261 tone -> garble -> hash -> FSK-like splatter. Most are inaudible with the antenna connected.
Many are ~15.6 kHz apart (14.051/.067/.082, 14.151/.166/.182/.198, 14.241/.256) -> a comb from one digital
clock (15.625 kHz = 16 MHz/1024 or 12 MHz/768). To find by elimination (see test plan).
Test plan (50 ohm dummy load on the antenna jack, listen to the S9 one at 14.194 and the 14.198 "data"):
1. Unplug the USB/TTL cable from the radio (PC USB noise / ground loop).
2. VariBeam Nano + OLED: disconnect its supply (screen OFF mode is not enough).
3. VFO Nano: build with DEBUG_SERIAL 0 (the 19200 status line toggles all the time).
4. Tune +/-1 kHz across a birdie: a real carrier moves normally; a Si5351 mixing product moves faster, slower or the wrong way.
5. Power supply: try a linear supply or a battery if a switch-mode supply is used.
BFO test result (2026-10-20): 14.194 tone changes pitch with the BFO (BFO +200 Hz -> gone, -200 Hz -> higher)
-> NOT a real outside signal, it is generated inside the radio (a real signal keeps its pitch, the LO follows the BFO).
Sniffer finds nothing radiated -> most likely Si5351 spurs: CLK0 (LO) and CLK1 (BFO) both run from PLLA at a fixed
800 MHz with fractional output dividers (sketch: set_pll(SI5351_PLL_FIXED, SI5351_PLLA)). Fix to try in the sketch:
BFO on its own PLL (PLLB) with an integer divider, LO with an even integer output divider and the PLL tuned instead.
NEXT VFO NANO UPLOAD (agreed 2026-10-20): 1) Si5351 spur fix (BFO on PLLB integer, LO even integer divider),
2) CW fix (CW sense + ~800 Hz BFO shift), then re-check the 20 m birdie list.
Calibrator idea: Si5351 CLK2 via thin coax to where the PC-820 100 kHz cal board output fed the receiver,
heavily attenuated; CLK2 enabled only in S3 CAL position (sense the CAL line) or from the menu - off otherwise.
LCD rewiring check (2026-10-20): super probe hears the LCD data lines only when almost touching the wire
(no "before" measurement, but the earlier hand-over-cable noise was clearly audible in the receiver).
Hand test after rewiring: no change in noise with the hand over the LCD cable; PC-100 cans, transformers and pot
barely react; the previously very touch-sensitive coil now reacts minimally. Only touching an exposed PC-100
transformer leg gives a clear change (expected: direct contact with an RF node).
