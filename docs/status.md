# Atlas 210X Si5351 VFO - status 2026-09-28

## Done
- Sketch `atlas_vfo_fixed\atlas_vfo_fixed.ino` (uploaded to the Nano, COM17, "ATmega328P (Old Bootloader)")
  - I2C timeout (freeze fix), encoder ISR only counts steps, band switch hysteresis,
    EEPROM restore works, no 2 s blank screen, RIT removed on TX, setup menu with exit.
  - S-meter: differential A1 (PC-100 pin 15) - A0 (PC-100 pin 16), 1k between pin 15 and 16
    (replaces the removed meter), 10k series + 100nF at radio end on both lines.
  - LCD S-meter line only written when it changes (less digital noise).
- CH340 driver rolled back to 3.5.2019.1 - IDE upload works again.
- Frequency calibration: Si5351 crystal ~101 ppm high -> CORR = +101000 ppb (stored, also default).
  14.099.0 gives 1008 Hz tone with TinySA on 14.100.000 (matches FTdx10).
- BFO = 5520000 Hz (stored, default). Opposite sideband suppressed, 300 Hz - 2.7 kHz even.
- Receiver hears -115 dBm (TinySA).
- Replaced: old caps in the receiver, C311 (47 uF AGC hold, PC-300D) tantalum -> electrolytic.
- CLK1 (BFO) drive 8 mA tested: no change -> left at 2 mA.

## Findings
- AGC is audio derived (PC-300). DSP (SOTABEAMS VariBeam) in front of it adds ~25 dB and
  changes the S-meter completely. DSP is bypassed / floating for now.
- Without DSP the AGC only starts at about -80 dBm (S8). Cause: too little audio into the AGC
  amp (Q301B on PC-300D) - to be investigated.
- PC-300D has a 2.5k trimmer pot (in place of R309 2.2k, D301 cathode to GND). It shifts the
  S-meter / AGC resting level, NOT the threshold. Set so rest is ~86-87 mV (sig). Pot is too
  coarse - consider a multi-turn trimmer.
- PC-300D trimmer cap C317 (= C327 5-30 pF on PC-300C) on the 2N3819 FET osc switch:
  carrier balance / isolation. Do not touch without measuring TX carrier suppression.
- "+10 V" in the Atlas voltage charts is the +8.5 V line (78L06 + 1N5221 = 8.4 V). 8.3 V is OK.
- Display cable radiates (hand over cable stops the noise).
- USB drops when the radio is switched off (Nano back-powers LCD/Si5351) - unplug USB first.

## S-meter calibration (needs redoing!)
The table in the sketch was measured BEFORE the C311 swap and the 2.5k trimmer change
(no signal 84 mV, -79 dBm 118, -73 185, -68 315, -63 591, -53 803, -43 860).
With the new trimmer setting (rest ~86-87) the bottom of the table is too steep -> display
flickers S0-S2 with no signal. Redo -85, -79, -73, -68, -63, -53, -43 dBm at 14.100.000,
dial 14.099.0 (1 kHz tone), antenna off, and add a dead zone of ~5 mV above rest.

## PC-300D AGC trimmer
- The 2.5k pot was measured at **533 ohm** at the last setting (rest ~86 mV) - check whether that
  was in circuit (parallel paths read low). Original PC-300C value in that place: R309 = 2.2k.
- Low value loads the rectified audio before the AGC amp -> likely cause of the late AGC.
- Being replaced with a multi-turn trimmer. First try ~2.2k, check AGC start (-97/-91 dBm),
  then pick the setting by the weak-signal ear test (-110 dBm clearest above noise).

## From the Engineering Supplement V3.71 (210\Atlas210XEngSupV371.pdf) - read 2026-09-29
- R309 (PC-300D 2.5k pot) IS the AGC threshold adjust (like 350XL). Replace with 5k multi-turn.
  Factory S-meter procedure: no signal -> R222 (PC-200) for zero; -73 dBm (50 uV) -> R309 for S9;
  repeat (they interact). For the Nano (1k instead of ~200 ohm / 500 uA meter):
  target sig ~0-10 mV at rest, ~300 mV at -73 dBm. Then recalibrate the Nano table. (p. 31-33)
- Factory AGC starts responding from about -112 dBm (p. 34) - ours starts ~-80 dBm -> R309 far off.
- PC-200: later boards have an undocumented 2.5k pot in place of R203 (3.9k) on the foil side
  (MC1350 gain, up to 13 dB, changes S-meter) - recommended at MAX resistance. (p. 55)
- PC-200D: R220 + D207 removed, 1N270 from pin 20 to Q202 pin 10 (band to pin 20). (p. 32, 54)
- CA3086 on PC-200 is the usual S-meter culprit - socket it.
- Test: 10 dB S/N sensitivity with PC-200 pin 19 (AGC) disconnected vs connected. (p. 53)
- PC-300D part numbers differ from the PC-300C schematic (e.g. C317 on C = C311 on D,
  C320 on C = C315 on D). Identify parts by value/position, not number. (p. 56)
- Dirty contacts: ohmmeter antenna jack -> GND on every band must be < 2 ohm; DeOxit band switch,
  burnish antenna relay (PC-1100C) and DC switching relay. (p. 6-7)
- AGC attack/release mod (Atlas bulletin, standard on LE) and bass-response caps on PC-300. (p. 53, 56)
- Receiver spurs come mainly from carrier osc x VFO harmonics -> add low-pass filters on the
  Si5351 CLK0 and CLK1 outputs. (p. 116-117)
- DSP belongs on PC-300 pin 22 (A.F. gain), i.e. after the AGC pickoff. (p. 96)

## Done 2026-09-29
- Antenna jack -> GND: 1-2 ohm on all bands (band switch / LPF / antenna relay OK).
- PC-200-D (built as C design: R220 100R + D207 1N4148, no R203 pot). Earlier work found:
  C204 100uF new, R202 2.2k moved to foil side, C221 replaced with 0.002 film. Replaced the
  dipped tantalums (22uF, 2.2uF, 6.8uF->10uF). R222 (S-meter zero) -> 1k multi-turn.
- PC-300D: R309 -> 5k multi-turn. More tantalums replaced. Earlier work on it: C322 0.47uF Mylar,
  new electrolytics. NOTE: PC-300D part numbers/values differ from the PC-300C schematic
  (e.g. R314 is 100k, not the 3k3 of the Atlas AGC bulletin) -> AGC bulletin mod NOT applied.
- DC switching relay RL101 (PC-100) was dirty -> cleaned. Big gain improvement.
- AGC now: PC-200 pin 19 = 4.53 V no signal (R309), 5.25 V at -73 dBm (factory 4.53 -> 5.16).
- S-meter re-calibrated with the basic tinySA (+ decade attenuator 10/20/6 dB stages;
  its "30 dB" stage reads ~5 dB short). S-meter starts at ~-90 dBm (S6). Table in the sketch.
- LCD S-meter: 9 fields = S1..S9, two lines per field (3 dB each), "+", "+10".."+40" as text.
- tinySA Ultra DAMAGED by accidental TX: self test signal level fail, -63.4 instead of -35 dBm,
  LNA makes it 26 dB worse, attenuator 7 dB steps. Parts ordered (AliExpress): U22 AS179-92LF,
  U28 PE4312C-Z, LNA BGA2817/ZK10D. Replace U22 first, then self test.
  NEVER key TX with a test instrument on the antenna jack.

## VariBeam DSP control Nano (2026-09-30) - DONE
- New sketch: `VB_LocalDisplay_fixed\VB_LocalDisplay_fixed.ino` (original: Desktop\VB_LocalDisplay.ino).
  Nano on COM9, "ATmega328P (Old Bootloader)". Libraries: Adafruit GFX + Adafruit SH110X (Library Manager).
- Fixes: 2 ms per emulated encoder state (VB missed steps), counted detents, OLED reset pin
  was pin 4 = encoder switch (removed), SH1106 column offset 0 (right edge wrapped to the left).
- Features: splash screen + resync at power on (VB driven to minimum, then to saved values),
  CF/BW/mode saved in EEPROM, passband edges + trapezoid graph, presets.
- Button: 1x CF/BW, 2x preset (SSB 1500/2400, SSB NARROW 1400/1800, CW 700/500, DIGI 1500/3000),
  3x screen when idle (OFF/DIM/ALWAYS ON), hold 2 s = resync.
- After uploading (Nano restarts alone) power cycle the radio so VB and Nano start in sync.
- Idea: bring a USB extension out to the back panel so both Nanos can be updated without opening.

## Also found 2026-09-30
- 20 m weaker: the chassis-mounted 20 m image filter (upstream of PC-810) had a loose paper coil
  form turning with the slug -> glue form, tune slug for minimum image (tinySA 3.060 MHz strong,
  dial 14.099.0), then check 14.100 at -79 dBm. 4-6 dB less on 20 m is normal for the Atlas.
- PC-100: loose trimmer pot (R101 carrier balance 100R) and hand effect -> replace R101 (then TX
  carrier null check), peak L103 (5520 kHz).

## 3D front plate (2026-10-06)
- Original: 3d/Atlas 210 - original.stl. Corrected and re-exported from CAD: 3d/Atlas 210 - final.stl
  (outer size unchanged 239x88x9).
- Project home (since 2026-10-06): Desktop\Atlas 210 Project = git repo
  https://github.com/Branko-Mihajloivc/Atlas-210-Project- (the E: folder is the old 2023 archive).
- MBL600 encoder (datasheet: flange D61, body D43, cutout D44, 3x M3 studs PCD 50.8 at 120 deg):
  recess D61.8 x 1, through hole D44.4, 3x D3.8 at (0,+25.4) (-22,-12.7) (+22,-12.7) from centre.
- 2x 0.96" SSD1306 (PCB 27.3 x 27.8, glass 26.8 x 19.3, pixels 22 x 11 starting 6.37 below PCB top):
  window 23 x 12, pocket 27.8 x 28.4, frame enlarged (2 mm walls).
- 16x2 LCD: pocket/frame as designed (PCB flush with plate top edge), window 65 x 14 -> 65 x 15
  (top edge level with the OLED windows).
- Displays chosen: VFO LCD -> RG1602A yellow-green (drop-in, 100R backlight resistor on board).
  VariBeam OLED was damaged during reassembly -> replacement 0.96" SSD1306 ordered
  (needs the sketch switched from SH110X to the SSD1306 library before upload!).

## Next
0. (done) Ohmmeter test, PC-200 check.
0b. Clean the antenna relay (PC-1100) too if signal level becomes intermittent again.
0c. On-air test OK (QSOs made, TX fine). Sketch now shows '!' (line 1, col 14) after any I2C
    timeout since power on - watch for it in normal use / TX.
0d. AGC release is slow (S-meter falls back over several seconds after a strong signal) - this is
    the Atlas AGC hold time on PC-300D (hold cap / discharge resistor), not the Nano. Optional:
    shorten release time (identify parts by value first).
1. Fit 5k multi-turn R309, set R222/R309 by the factory procedure, then recalibrate the S-meter table.
2. AGC / audio chain on PC-300D: why so little audio reaches the AGC amp.
3. DSP control Nano (VB_LocalDisplay.ino): add delay(2) after each digitalWrite in
   vbStepEmulation() so the VariBeam does not miss steps; move DSP after the AGC pickoff
   (PC-300 pin 22 -> AF gain pot -> pin 12).
4. TX test into dummy load (TinySA disconnected!): A0/A1 voltages and i2cTimeouts on TX,
   carrier suppression.
5. Birdies re-check with correct frequency calibration (e.g. one near 14.100.3).
6. Display cable: 220R-1k series resistors in LCD lines at the Nano and/or ferrite.
