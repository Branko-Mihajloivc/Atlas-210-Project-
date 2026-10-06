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
- The VariBeam DSP adds ~25 dB of audio; it belongs after the AGC pickoff (PC-300 pin 22 → AF gain).
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
