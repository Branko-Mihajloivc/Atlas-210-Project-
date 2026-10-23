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
NEXT VFO NANO UPLOAD (agreed 2026-10-20): 1) Si5351 spur fix (BFO on PLLB integer, LO even integer divider), 1b) encoder direction-change fix,
2) CW fix (CW sense + ~800 Hz BFO shift), then re-check the 20 m birdie list.
Calibrator idea: Si5351 CLK2 via thin coax to where the PC-820 100 kHz cal board output fed the receiver,
heavily attenuated; CLK2 enabled only in S3 CAL position (sense the CAL line) or from the menu - off otherwise.
LCD rewiring check (2026-10-20): super probe hears the LCD data lines only when almost touching the wire
(no "before" measurement, but the earlier hand-over-cable noise was clearly audible in the receiver).
Hand test after rewiring: no change in noise with the hand over the LCD cable; PC-100 cans, transformers and pot
barely react; the previously very touch-sensitive coil now reacts minimally. Only touching an exposed PC-100
transformer leg gives a clear change (expected: direct contact with an RF node).
Calibrator (CLK2): KEEP on the plan for now - may be used as a built-in signal generator. Decide later.
GPSDO: only needed occasionally for calibration, not permanently installed. Options: GPSDO set to an
in-band frequency (e.g. 14.100.000) into the antenna jack (attenuated) and set CORR in the menu by ear / tone;
or later a 1PPS input (e.g. J4 spare pin 9) for automatic CORR.
J4 wiring (in progress 2026-10-20): VFO Nano only - TX/RX each twisted with GND, RST via 100 nF (cap at the Nano end)
to J4 pin 4, PTT pair, audio in shielded cable separate. VariBeam Nano keeps its header pins for the TTL cable.
VFO encoder (MBL600): loses one click on every direction change. Cause: Rotary library counts full cycles and
emits at a fixed A/B state that does not match the MBL600 detent rest position. Fix (next VFO upload): own
quadrature decoder - count all edges, one step per detent, re-align the count when the knob rests in a detent.
Pot compensation (wiring sheet section 7) DONE 2026-10-20: RF gain R5/R7, 20 k across MIC gain, 11 k across ALC.
To check on air: modulation level at normal mic gain (was only OK at max), RF gain range.
CLK2 as RX check source: use S5-S7 (about -97 / -91 / -85 dBm) - well above internal leakage, repeatable for
health checks, S-meter check and band comparison. Leakage test: CLK2 on, injection wire disconnected -> S-meter
should show nothing / well below S1. Real MDS / sensitivity: external tinySA + step attenuator.
J4 check: pin 1 (+CW in the original drawing) was never wired in this radio (clean contact, no solder).
J4 found: 1 unused, 2-3 jumper (old VFO coax path, remove), 4 GND, 5 orange +8.5 V, 6 cut white/orange stub,
7 yellow T line, 8 red +13 V via fuse, 9 to check. Old wires to be removed (check other ends / splice).
J4 new plan: 1 RST (grey, 10 nF / 103 in series at the Nano end), 2 Nano TX D1 (green), 3 Nano RX D0 (white), 4 GND (black),
5 / 7 audio (direction to decide), 6 audio GND PC side - floating, open to chassis, 8 PTT (proposed), 9 spare.
External adapter: adapter RX -> pin 2, adapter TX -> pin 3, GND -> pin 4, DTR -> pin 1, adapter +5 V not used.
J4 final: 5 audio in (PC->TX), 6 audio GND PC side floating, 7 audio out (RX->PC), 8 PTT.
J4 pin 8 PTT: white wire (Atlas KEY line colour) to the KEY line (mic PTT / PC-100 pin 20), NOT the T line (+13 V TX supply output).
ATT/PRE relays: Finder 30.22.7.012 (DPDT, 12 V, 720 ohm / 16 mA, AgNi + 5 um Au contacts) - suitable, 2 needed.
Next VFO upload PREPARED 2026-10-21 (commit 1069026, compiled, not uploaded): spur fix, MBL600 decoder, CW carrier shift. CW_SENSE_FITTED = 0 until the A7 network is wired. Previous sketch kept in git history.
A2 TX sense seems unconnected (to check). Plan: KEY line -> 1N4148 (band to key line) -> A2, INPUT_PULLUP, LOW = TX.
FT8 plan (2026-10-21, user runs MSHV):
- RX audio to the PC (J4 pin 7) taken BEFORE the VariBeam (PC-300 pin 22) -> full RX bandwidth, independent of DSP / volume.
- PTT step A (now): PC grounds J4 pin 8 (KEY line) - USB-TTL adapter RTS via a small NPN/opto in the plug, or a
  sound-card interface with PTT out. Use RTS for PTT, never DTR (DTR resets the Nano via the RST cap).
- PTT step B (later, with CAT): CAT "TX" command -> VFO Nano D13 -> 2N7000 pulls the KEY line low. A and B can coexist.
- Mic-disconnect relay on the same PTT so room noise is not transmitted with data.
D13 wire run now (2026-10-21) from the VFO Nano to the KEY line / J4 pin 8 area, labelled "D13 CAT PTT", unconnected until the 2N7000 is fitted.
CLK2 thin coax to be run now (VFO box open), receiver end open until the PC-820 injection point is located.
+5 V wire out: dropped (no room).
KEY line current measured 2.1 mA -> CAT PTT with any small NPN (BC547): D13 -> 4.7 k -> base, 47 k B-E, collector to KEY line, emitter GND.
Wired 2026-10-22: A6 green (10 k pull-up at Nano), A7 blue -> network (orange CAL, white/black +CW), A2 white (diode at Nano end), D4 orange, D5 grey, D6 red, D13 white/red, CLK2 coax. Buttons GND twisted with the green A6 wire; no +5 V wire.
Front controls decided 2026-10-23: STEP button (0 ohm), MENU button (4.7 k), MODE latching rocker NORM/OPP (22 k) all on A6 + GND; ATT/NORM/PRE and AGC rockers; 3 ATU buttons.
WIRING STATE 2026-10-23: VFO Nano side done (J4 4-wire set, A6+GND twisted, A7 blue, A2 white with diode at Nano, D4 orange, D5 grey, D6 red, D13 white/red, CLK2 coax). J4 now connected to the USB adapter (CAT / rear programming). Still to do at the far ends: buttons + MODE rocker, ATT/PRE relays + sense, AGC, A7 network, KEY line point (A2, D13 NPN, J4 pin 8 PTT), J4 audio, CLK2 injection. Next: first upload through J4.
J4 RST cap: 10 nF did NOT reset the Nano (its own 100 nF RST-to-CH340 cap forms a divider, only ~0.5 V dip). Changed to 1 uF (105). Serial via J4 confirmed working (status lines read on COM20, white = pin 2).
UPLOADED 2026-10-23 via J4 (COM20) with the power-switch method (radio off 10 s, on while arduino-cli retries): spur fix + MBL600 decoder + CW shift (CW_SENSE_FITTED 0). Serial confirms loDiv=98 on 20 m. To test: RX normal on all bands, dial accuracy, encoder reversal, 20 m birdie re-check.
Uploaded 2026-10-23 (e951f81): divider choice away from PLL spur points + fewer I2C writes. Serial: 14.0955 loDiv=82 score=313, bfoDiv=118. Earlier run (d=98): encoder reversal fixed, but tuning noise new and birdies 14.194/14.1945 S9, 14.106 S8 (new), 14.051 S8, 14.196 garble S7; 14.198 gone. Re-test pending.
Uploaded 12ce740: serial quiet unless asked ("?" = one status line, "D" = stream on/off). Verified: 0 bytes unasked, "?" answers. Ticking on 21/28 MHz was the 2/s status line running out through J4 + USB cable. Later: 470 ohm-1 k series in J4 TX/RX at the Nano end; CAT polling slow or auto-information mode.
Remaining 20 m birdies (14.051, 14.106, 14.1955; antenna off, barely audible with antenna): PARKED. Not Si5351 PLL fractional, not VariBeam (Nano + DSP off). Suspects: VFO Nano 16 MHz, its CH340 12 MHz, Si5351 25 MHz xtal, LCD oscillator. Find with tinySA + loop probe (after repair). Possible fixes: lift CH340 supply pin (J4 programming makes it unnecessary), tin shield over Si5351 / Nano, RC/ferrite on wires, 27 MHz crystal or 27 MHz Si5351 module (sketch: init with 27000000).
Birdie hunt 2026-10-23 (tinySA basic + 1 cm loop): spectrum 0-30 MHz near the VFO shows only BFO 5.52 MHz + harmonics (11, 16.5, 22, 27.6) and LO + harmonics; no 12 / 16 / 25 MHz combs. LO x BFO products only at order 17-19 near 14.194 (too weak). 20 m image (3.154 MHz) injected into the antenna jack: not heard -> front-end image rejection OK. 2.9-3.3 MHz loop scan: nothing above -101 dBm. Remaining birdies (antenna off only, barely audible with antenna): hunt stopped.
Buttons fitted 2026-10-23: STEP (direct) and MENU (2.2 k) on A6, no cap; MODE rocker not wired yet. Sketch cc092bd supports A6 (STEP as before, MENU hold 1 s = setup menu, MODE OPP = +3.3 kHz carrier, CW always NORM) - to upload.
Uploaded cc092bd 2026-10-23: STEP (click / double click RIT) and MENU (hold 1 s -> setup menu, save with STEP hold) tested OK. A2 TX sense not connected yet (RIT stays on during TX until then).
A2 TX sense connected 2026-10-23 and tested: tx=0 in RX, tx=1 keyed. On TX: A0/A1 ~1.75 V (safe), sig 1.
CW WORKS 2026-10-23: A7 network wired (REC 0, CAL 470, CW 828), CW_SENSE_FITTED 1 uploaded, R13 (10 k) jumpered from mic gain pot top to +CW -> about 50 W carrier into the dummy load, set with mic gain. Still to check: carrier frequency = dial + 800 Hz on 20/15/10 m (dial - 800 Hz on 80/40), e.g. with the FTdx10 listening off-air (never a test instrument on the antenna jack during TX).
Semi-break-in NEEDED (S3 order REC-TRANS-CW makes manual switching clumsy). Idea: VariBeam Nano (free pins) reads the key, grounds the KEY line (TX) + switches +13 V onto +CW via transistors, hang time after the last element, optional sidetone. VFO Nano follows automatically via the A7 +CW sense. S3 stays on REC. To design later.
CW receive: REC position, tune the station to ~800 Hz tone, switch to CW to send (carrier lands on their frequency); VariBeam CW preset 700/500 - check whether 700 or 800 Hz centre suits.
Switch types final 2026-10-23: ATT/NORM/PRE = SPDT ON-OFF-ON (first, most used; sense via coil diodes to D5/D6), AGC = DPDT (2nd pole GND -> D4, no divider), MODE = DPDT (one pole, A6 22 k). 100 nF (104) on A6 at the buttons. build_pdfs.py fixed: own Edge instance + wait (dark PDFs were sometimes stale).
Data audio plan 2026-10-23: OUT from PC-300 pin 22 (after AGC, before AF gain / VariBeam) via 1 uF + 10 k + 600:600 transformer to J4 7/6; IN from J4 5/6 via transformer + 47 k / 470 ohm into PC-200 pin 17 (mic input, MIC gain controls it). J4 pin 8 now drives a DPDT relay (+12 V coil, flyback): contact 1 keys the KEY line, contact 2 (NC) opens the mic hot wire -> mic muted on PC PTT. Wiring sheet section 10.
CW output ~50 W on 20 m vs spec >= 80 W (200 W input). Later checks: supply voltage at the radio key-down (13.6 V), current (manual: ~12 A at full carrier), ALC pot setting, mic gain max, wattmeter cross-check, compare bands. Short key-downs (PA heat).
ALC: more ALC = less power is correct (manual 3-17: CCW = no ALC, CW = max, normal ~12 o clock). Mic gain stops raising power at ~1/3 -> test later with ALC fully CCW: rises = ALC limited; stays ~50 W = PA / supply limit (check V and A key-down, ~12 A per manual).
Mic: Shure 404C, element measures ~1 k DC (likely high / medium impedance dynamic). Low level vs the crystal / ceramic mics the Atlas expects -> hard to modulate. After the ALC test: mic preamp (10-20 dB) in front of PC-200 pin 17.
FINAL TODO (when everything is done): update the full chassis drawing in the style of "Atlas 210x-215X Chassis Wiring - YT3CT mods.png" (E: 210 folder, red annotations on the redraw) with ALL final modifications - keep it off GitHub (based on the Atlas schematic).
MENU items wanted (all, step by step; flash ~10 KB free, RAM ~1 KB, EEPROM plenty - add a settings version number): OPP offset, CW offset, default step, tuning acceleration, TX timeout, CAT on/off + baud, debug stream, CLK2 on/off + frequency + drive, S-meter zero offset, Si5351 drive CLK0/CLK1, reset band memories, factory reset (keep CORR). First batch: OPP offset, CW offset, default step, acceleration.
LCD garbage on TX (CW, 50 W) 2026-10-23: caused by the R13 / +CW jumper wire routed in the bundle with the Nano wires. Rerouted alone along the front near the chassis -> display clean. (LCD re-init after TX->RX still a possible later safety net.) RX relay (RL101, PC-100) acting up again -> clean + DeoxIT.
MODE rocker wired 2026-10-23 (ON-OFF-ON, one side = OPP via 22 k), 104 near the Nano: A6 1023 NORM, 704 OPP; sideband flips. AGC rocker 2nd pole wired to D4 (sketch does not read D4 yet - next update: D4/D5/D6 pull-ups + status).
Button side DONE 2026-10-23: STEP, MENU (and RIT, menu save) work in NORM and OPP.
LCD layout uploaded and approved 2026-10-23: row 1 USB/LSB/TX/CW + frequency + ! + AGC F/S; row 2 ATT/PRE + S-meter with plain LCD digits 1 3 5 7 9 (underlined copy once that S-unit is reached, SMETER_DIGIT_STYLE 2), thin bars over 7 rows, +10..+40. TX S-meter reaction seen once (PA current via the meter circuit) then gone - no PA label.
Menu batch 1 uploaded and tested 2026-10-23 (43f29c2): NOR flag, 6 menu items (BFO, CORR, OPP offset, CW offset, start step, acceleration), MENU click = next item. Next menu batch with CAT / calibrator: TX timeout, CAT on/off + baud, debug stream, CLK2 on/off + freq + drive, S-meter zero, Si5351 drive, reset bands, factory reset.
Hardware note: BFW16A amplifiers after the Si5351 on both LO (CLK0) and BFO (CLK1), plus LPF / BPF on both. Order confirmed: Si5351 -> BFW16A amp -> LPF (good). Amp levels set to the same peak-to-peak voltage as the original VFO / carrier oscillator; LPF removes harmonics and makes the square wave near-sine. Can-mixer (MCL / SBL-1) swap considered: not worth it for the birdies (big job, bilateral RX/TX).
Power / SWR display: probably covered by the ATU-100 (own SWR / PWR display) - decide when it arrives. If built: PC-1100 bridge fwd / ref -> VariBeam Nano analog pins via dividers, OLED on TX, calibrate with wattmeter + 50 / 75 / 25 ohm loads (user has an old Nano SWR meter design with that calibration). Otherwise the VariBeam Nano pins go to supply voltage + heat sink temperature / fan.
CAL / GEN plan: S3 CAL -> CLK2 on (menu: TONE = carrier 1 kHz from the dial so a 1000 Hz tone is heard, or 100k = 100 kHz markers), shows CAL. Rear SMA generator idea: CLK2 -> 100 nF -> SMA (+ optional 50 ohm back-termination), ~8 kHz-150 MHz square, 0..+10 dBm (2/4/6/8 mA), accuracy = CORR (GPSDO calibrated); GEN menu: knob sets frequency, level selectable; CLK2 off otherwise. SMA is output only - never to a transmitter / antenna jack.
GPSDO is 10 MHz fixed (user believes). Calibration plan: GEN mode CLK2 = 10.000000 MHz on the rear SMA, FTdx10 tuned ~10 MHz hears GPSDO 10 MHz + CLK2 loosely coupled, adjust CORR to zero beat (< 1 beat/s ~ 0.1 ppm). 1PPS not needed. Generator on/off idea: hold MENU ~5 s in CAL position (to agree later).
ATT/PRE board placement: between PC-800 OUT and PC-100 pin 1 (after the preselector), chain IN -> ATT relay -> PRE relay -> OUT. Preamp: BFG135 broadband feedback amp ~10-11 dB (docs/preamp_bfg135.pdf): RF 220, RE1 10 + RE2 22 (bypassed), bias 1.5 k / 390 via 22 uH, collector 100 uH, Ic ~50 mA.
Attenuator decided: ~16 dB pi-pad 68 / 150 / 68 ohm (E12, 15.9 dB, Zin 49.3 ohm); 20 dB alternative 62 / 240 / 62.
