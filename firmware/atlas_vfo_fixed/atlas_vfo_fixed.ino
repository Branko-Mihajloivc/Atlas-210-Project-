// YU5KBM - Atlas 210X Si5351 VFO
// Fixed version (2026-09) of atlas_vfo_a0_band (2023-06-01).
//
// Changes against the 2023 sketch:
//  - I2C timeout (Wire.setWireTimeout) so a glitch on SDA/SCL can no longer hang the VFO forever;
//    after a timeout the Si5351 is simply programmed again.
//  - Encoder ISR only counts steps; all frequency maths runs in loop() (no more ISR/loop race on vfo).
//  - Band switch (A3): hysteresis + must be stable for 100 ms before the band changes.
//    Each band remembers its last frequency.
//  - Frequency saved in EEPROM is really restored at power on (was overwritten on the first loop).
//  - EEPROM save 30 s after the last tuning step, without the 2 s blank screen.
//  - RIT is removed on TX and put back on RX (CLK0 is re-programmed on every TX/RX change).
//    Toggling RIT re-programs CLK0 immediately.
//  - Setup menu (hold button at power on) can now be left: BFO / FREQ CORRECTION, hold 2 s = save + exit.
//  - S-meter bar stays inside the 16 columns; debug output throttled to 2 lines/s.
//  - S-meter reads the bridge differentially: A0 = PC-100 pin 16 (reference), A1 = pin 15 (signal),
//    32x averaged, fast attack / slow decay, calibrated with the smeterCalMv/smeterCalDb table.
//
// 2026-10:
//  - Si5351 spur fix: CLK0 (LO) on PLLA with an even integer output divider (the PLL is tuned),
//    CLK1 (BFO) on its own PLLB with an even integer divider. The divider is chosen so the VCO stays
//    away from whole multiples / simple fractions of the crystal (PLL spur points). Before, both ran
//    from PLLA at a fixed 800 MHz (= 32.0 x crystal) with fractional dividers -> narrow birdies.
//    Same divider while tuning = only the PLL registers are written (less I2C noise per step).
//  - Own quadrature decoder for the MBL600 (counts every edge, one step per detent, re-aligns when
//    the knob rests). The Rotary library lost one click on every change of direction.
//  - CW: the S3 CW position is sensed on A7 (CW_SENSE_FITTED). In CW the carrier (CLK1) moves
//    CW_OFFSET_HZ into the crystal filter passband, like the original PC-500 did on +CW. CLK0 stays,
//    so the carrier comes out 800 Hz above the dial on USB bands and below on LSB bands (as the Atlas).
//  - Menu batch 2: S3 in CAL turns on CLK2 as a marker 1 kHz into the passband of the dial frequency
//    (heard as a 1 kHz tone, follows the knob), injected at PC-800C IN through 47 k. CLK2 runs on
//    PLLB (fractional multisynth), output high-Z when off. CAT: Kenwood TS-480 subset on the serial
//    port (FA FB IF MD ID PS AI FR FT SM TX RX); TX raises D13 (to the VariBeam Nano PTT input,
//    which keys only after 300 ms and has its own TX timeout). Menu: CAT on/off, CAT baud, CAL level.
//  - CAL batch 3: marker injected at the ATT/PRE board input (shielded 1 k / 5 ohm / 4.7 k box). Menu
//    CAL MARKER (always OFF at power on): fixed marker like a crystal calibrator on the start /
//    middle / end point of the band nearest to the dial (peaks as it is tuned through). In CAL the S meter line shows dBm. Menu AUTO CAL: CLK2 2.5 MHz counted on D5
//    (Timer1 T1) between GPSDO 1PPS edges on D6, correction applied, checked once more.
//    S3 in CAL: MENU short press = marker on / off ("MRK" on the LCD), off again when S3 leaves CAL.
//    Menu: hold MENU 2 s = leave without saving (all values back as they were).

#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <avr/eeprom.h>

#define DEBUG_SERIAL 1                      // 0 = no serial output

// Layout must stay identical to the 2023 sketch so the stored EEPROM data is still valid
// Menu settings added 2026-10 (own EEPROM area at SETTINGS2_ADDR, so the 2023 block stays valid)
#define SETTINGS2_ADDR   64
#define SETTINGS2_MAGIC  0xA2               // change when the layout changes -> defaults are loaded
struct settings2_t
{
    uint8_t magic;
    uint16_t oppOffsetHz;                   // MODE OPP carrier shift (Atlas 5523.3 kHz crystal = 3300)
    uint16_t cwOffsetHz;                    // CW carrier shift into the passband
    uint32_t defaultStep;                   // tuning step at power on
    uint8_t accel;                          // tuning acceleration 0 = off, 1 = low, 2 = high
    // batch 2 (2026-10-26), valid when magic2 == SETTINGS2_MAGIC2
    uint8_t catOn;                          // 1 = CAT on the serial port (debug output off)
    uint8_t catBaud;                        // index into catBauds[]
    uint8_t calDrive;                       // CLK2 drive 0..3 = 2 / 4 / 6 / 8 mA
    uint8_t magic2;
}
settings2;
#define SETTINGS2_MAGIC2 0xB1
const uint32_t catBauds[] = {4800, 9600, 19200, 38400};
#define DEBUG_BAUD   19200
#define CAT_PTT_PIN  13                     // HIGH = CAT TX -> 10 k -> VariBeam Nano D8
#define AUTOCAL_HZ       2500000UL          // CLK2 for AUTO CAL, counted on D5 (T1, max ~6 MHz)
#define AUTOCAL_SECONDS  10                 // gate: 10 PPS seconds = 25 000 000 counts, 1 count = 40 ppb
#define AUTOCAL_TIMEOUT_LOOPS 3000000UL     // no PPS edge for ~2-4 s (polling loop ~1 us)

struct settings_t
{
    int32_t bfo_offset;                     // BFO in 0.01 Hz (552000000 = 5.520000 MHz)
    uint32_t vfo;                           // last frequency in Hz
    bool superhet;
    int fcorrection;                        // Si5351 correction in units of 10 ppb (int is only 16 bit)
}
settings;

// LCD custom characters (8 slots):
//   0..4 = S-meter digits 1 / 3 / 5 / 7 / 9: small 3x6 digit, or the same digit cut out of a lit
//          block when the signal reaches that field (slot rewritten only when the state changes)
//   5 = bar (both lines, full step), 6 = half bar (left line), 7 = step marker (setup menu)
// bars: thin lines over the same 7 rows as the LCD digits
byte bar5[8] = {B01010, B01010, B01010, B01010, B01010, B01010, B01010, B00000};
byte barHalf[8] = {B01000, B01000, B01000, B01000, B01000, B01000, B01000, B00000};
// LCD (HD44780) digits 1 3 5 7 9 with an underline in row 8: shown once the signal reaches that S-unit
byte ulDigit[5][8] = {
    {B00100, B01100, B00100, B00100, B00100, B00100, B01110, B11111},   // 1
    {B11111, B00010, B00100, B00010, B00001, B10001, B01110, B11111},   // 3
    {B11111, B10000, B11110, B00001, B00001, B10001, B01110, B11111},   // 5
    {B11111, B00001, B00010, B00100, B01000, B01000, B01000, B11111},   // 7
    {B01110, B10001, B10001, B01111, B00001, B00010, B01100, B11111}};  // 9
const uint8_t smallDigit[5][6] = {
    {B010, B110, B010, B010, B010, B111},   // 1
    {B111, B001, B011, B001, B001, B111},   // 3
    {B111, B100, B111, B001, B001, B111},   // 5
    {B111, B001, B001, B010, B010, B010},   // 7
    {B111, B101, B101, B111, B001, B111}};  // 9
int8_t digitLit[5] = {-1, -1, -1, -1, -1};  // what is in CGRAM slots 0..4 now (-1 = not loaded)
// Step marker glyph (setup menu)
byte bar1[8] = {B10000, B11000, B11100, B11110, B11110, B11100, B11000, B10000};

#define BAND_SELECT_IN A3                   // 5 way rotary switch for band select (resistor ladder)
#define ENCODER_A    3                      // Encoder pin A
#define ENCODER_B    2                      // Encoder pin B
#define BTN_IN       A6                     // STEP / MENU buttons + MODE rocker (resistor ladder)
// A6: 10k pull-up to 5 V at the Nano. STEP = 0 ohm to GND, MENU = 2.2k, MODE rocker (OPP) = 22k.
// Readings: STEP 0, MENU ~184 (~170 with MODE on), MODE alone ~700, nothing 1023.
#define A6_STEP_MAX    100
#define A6_MENU_MAX    480
#define A6_MODE_MAX    860
#define MENU_HOLD_MS   1000                 // hold MENU this long to open the setup menu
#define OPP_OFFSET_HZ  3300                 // MODE OPP: carrier on the other side of the filter (like the 5523.3 kHz crystal)
#define TX_BTN       A2                     // LOW = TX
#define SMETER_DIGIT_STYLE 2               // 0 = LCD digits, 1 = small digits inverted when reached, 2 = LCD digits underlined when reached
#define AGC_SENSE    4                      // AGC rocker pole 2: LOW = fast
#define ATT_SENSE    5                      // ATT relay coil (1N4148): LOW = ATT on
#define PRE_SENSE    6                      // PRE relay coil (1N4148): LOW = PRE on
#define SMETER_IN    A1                     // PC-100 pin 15: S meter amp output (rises with signal)
#define SMETER_REF_IN A0                    // PC-100 pin 16: S meter bridge reference (ZERO pot R222)
// Both through 10k in series + 100nF to GND at the radio end; 1k between pin 15 and 16 replaces the
// removed meter. Never connect to the meter / RL101 side (+13 V on TX).

// S3 position on A7: +CW -> 1N4148 -> 22k -> A7, CAL line -> 1N4148 -> 47k -> A7, 10k A7 -> GND.
// CW about 3.8 V, CAL about 2.1 V, REC / TRANS 0 V. Set CW_SENSE_FITTED 0 while the network is not
// wired: an open A7 floats and would switch CW on and off at random.
#define CW_SENSE_FITTED  1                // A7 network wired 2026-10-23
#define S3_SENSE_IN      A7
#define S3_CW_MIN_RAW    620                // above = CW (3.8 V = 778)
#define S3_CAL_MIN_RAW   300                // above = CAL (2.1 V = 430)
#define CW_OFFSET_HZ     800                // carrier shift in CW, into the filter passband (5520.3-5523.0 kHz)

// MBL600 encoder: edges per detent (4 = one full quadrature cycle per click, 2 = half cycle)
#define ENC_EDGES_PER_STEP 4
#define ENC_REST_MS        60               // no edge for this long = knob rests in a detent

// Si5351 VCO limits used for the integer divider choice
#define VCO_MAX_HZ       900000000ULL
#define VCO_MIN_HZ       600000000ULL

// S meter calibration table: "sig=" value (mV of A1 - A0) -> dB above S0 (dB = dBm + 127, S9 = -73 dBm = 54 dB).
// Measured 2026-09-29 on 14.100 MHz, dial 14.099.0 (1 kHz tone), basic tinySA carrier into the antenna
// jack, DSP bypassed. Receiver after: PC-200/PC-300 tantalums replaced, DC relay cleaned, R309 (5k
// multi-turn, AGC threshold) set for 4.53 V on PC-200 pin 19 with no signal (5.25 V at -73 dBm),
// R222 (S-meter zero) set for sig ~5 mV with no signal.
//   no signal          5 mV
//   -89 dBm (S6)      13 mV   (levels below -76 dBm via tinySA -73/-63 dBm + decade attenuator,
//   -83 dBm (S7)      50 mV    10/20 dB + 6 dB stages; the "30 dB" stage reads ~5 dB short - not used)
//   -79 dBm (S8)     107 mV
//   -76 dBm          209 mV
//   -73 dBm (S9)     356 mV
//   -68 dBm (S9+5)   643 mV
//   -63 dBm (S9+10)  714 mV
//   -53 dBm (S9+20)  777 mV
//   -43 dBm (S9+30)  826 mV
//   -33 dBm (S9+40)  857 mV
// The S meter does not move below about -90 dBm (AGC idle). Do not turn R222 (PC-200) or
// R309 (PC-300D) after this. Keep mV rising.
const int16_t smeterCalMv[] = {8, 13, 50, 107, 209, 356, 643, 714, 777, 826, 857};
const int16_t smeterCalDb[] = {0, 38, 44,  48,  51,  54,  59,  64,  74,  84,  94};
const uint8_t SMETER_CAL_POINTS = sizeof smeterCalMv / sizeof smeterCalMv[0];
#define SMETER_SAMPLES   32                 // readings averaged per channel (finer than 1 ADC step)
#define SMETER_DECAY     4                  // falls 1/4 of the way per 100 ms, rises at once (like a meter)

#define SAVE_DELAY_MS      30000UL          // save to EEPROM this long after the last tuning step
#define SMETER_PERIOD_MS   100UL
#define DEBUG_PERIOD_MS    500UL
#define BAND_SAMPLE_MS     20UL
#define BAND_STABLE_READS  5                // 5 x 20 ms = band must be stable for 100 ms
#define BAND_HYST          25               // ADC counts of hysteresis around each threshold
#define WIRE_TIMEOUT_US    3000UL

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

Si5351 si5351;

volatile int8_t encoderSteps = 0;           // written by the encoder ISR, taken by loop()
volatile int8_t encAcc = 0;                 // edges counted since the last step
volatile uint8_t encState = 0;              // last (B << 1) | A
volatile unsigned long encLastEdge = 0;

bool cwMode = false;                        // S3 in CW: carrier shifted into the passband
bool modeOpp = false;                       // MODE rocker: opposite sideband
bool agcFast = false, attOn = false, preOn = false;
bool flagsDirty = true;                     // row 2 cols 0-3 (ATT / PRE) must be redrawn
enum A6State : uint8_t { A6_NONE, A6_STEP, A6_MENU, A6_MODE };
A6State a6Stable = A6_NONE;
unsigned long menuDownSince = 0;
bool calMode = false;                       // S3 in CAL: CLK2 marker on
bool clk2On = false;
bool calMarker = false;                     // menu CAL MARKER, not saved: OFF at every power on
bool clk2PinOff = false;                     // debug 'M': CLK2 divider runs, output pin off (leak test)
uint64_t lastClk2 = 0;
uint64_t bfoPllFreq = 0;                    // PLLB VCO in 0.01 Hz (CLK2 shares it)
bool catTx = false;                         // CAT asked for TX (D13 HIGH)
char catBuf[16];
uint8_t catLen = 0;
uint16_t s3Raw = 0;
uint16_t loDivider = 0;                     // CLK0 output divider in use (even integer)

uint32_t vfo = 10000000UL;                  // displayed frequency in Hz
uint32_t bfo = 552000000UL;                 // BFO in 0.01 Hz
uint32_t radix = 100;                       // tuning step in Hz
uint32_t ritstep = 1;
int32_t rit_offset = 0;

bool changed_f = false;
bool rit = false;
bool txon = false;
bool setupmenu = false;

unsigned long lastTuneMillis = 0;
unsigned long lastBandSample = 0;
unsigned long lastSmeter = 0;
unsigned long lastDebug = 0;
bool debugStream = false;                   // continuous status lines ('D' over serial)

uint16_t bandRaw = 0;
uint16_t smeterMv = 0;                      // pin 15
uint16_t smeterRefMv = 0;                   // pin 16
int16_t smeterDiffMv = 0;                   // pin 15 - pin 16, as measured
int16_t smeterShownMv = 0;                  // with meter ballistics (fast attack, slow decay)
int8_t smeterLastBars = -1;                 // what is on the LCD now; -1 = redraw
int8_t smeterLastOver = -2;
uint16_t i2cTimeouts = 0;

/*
MULTI-CLICK: One Button, Multiple Events
Oct 12, 2009
Run checkButton() to retrieve a button event:
Click
Double-Click
Hold
Long Hold
*/

// Button timing variables
unsigned int debounce = 20;       // ms debounce period to prevent flickering when pressing or releasing the button
unsigned int DCgap = 250;         // max ms between clicks for a double click event
unsigned int holdTime = 2000;     // ms hold period: how long to wait for press+hold event
unsigned int longHoldTime = 5000; // ms long hold period: how long to wait for press+hold event

// Other button variables
boolean buttonVal = HIGH;          // value read from button
boolean buttonLast = HIGH;         // buffered value of the button's previous state
boolean DCwaiting = false;         // whether we're waiting for a double click (down)
boolean DConUp = false;            // whether to register a double click on next release, or whether to wait and click
boolean singleOK = true;           // whether it's OK to do a single click
unsigned long downTime = 0;        // time the button was pressed down
unsigned long upTime = 0;          // time the button was released
boolean ignoreUp = false;          // whether to ignore the button release because the click+hold was triggered
boolean waitForUp = false;         // when held, whether to wait for the up event
boolean holdEventPast = false;     // whether or not the hold event happened already
boolean longHoldEventPast = false; // whether or not the long hold event happened already

// Bands. Frequencies in Hz.
typedef struct
{
    const char *name;
    uint32_t minFreq;
    uint32_t maxFreq;
    uint32_t lastFreq;      // start frequency, then the last frequency used on this band
    bool ifPositive;        // CLK0 = RF + IF (true) or RF - IF (false)
} Band;

Band band[] = {
    {"80", 3300000UL, 4200000UL, 3700000UL, true},
    {"40", 6800000UL, 7400000UL, 7100000UL, true},
    {"20", 13800000UL, 14650000UL, 14100000UL, false},
    {"15", 20800000UL, 21650000UL, 21250000UL, false},
    {"10", 28200000UL, 29600000UL, 28900000UL, false}};

// CAL marker points in kHz (start / middle / end of each band, same order as band[])
const uint16_t calPointsKhz[5][3] = {
    {3500, 3650, 3800},
    {7000, 7100, 7200},
    {14000, 14175, 14350},
    {21000, 21225, 21450},
    {28500, 29000, 29500}};
const uint8_t NUM_BANDS = sizeof band / sizeof band[0];

// Band switch thresholds on A3 (ladder gives about 0 / 255 / 512 / 768 / 1023)
const uint16_t bandThreshold[NUM_BANDS - 1] = {180, 400, 650, 900};

uint8_t currentBand = 0;
uint8_t bandCandidate = 0;
uint8_t bandStableCount = 0;

/**************************************/
/* A6: STEP / MENU buttons, MODE      */
/**************************************/
uint16_t readA6()
{
    analogRead(BTN_IN);                     // dummy read after switching the ADC channel
    return analogRead(BTN_IN);
}

A6State classifyA6(uint16_t raw)
{
    if (raw < A6_STEP_MAX) return A6_STEP;
    if (raw < A6_MENU_MAX) return A6_MENU;
    if (raw < A6_MODE_MAX) return A6_MODE;
    return A6_NONE;
}

// Sampled every 5 ms, must read the same twice in a row (no capacitor on the ladder yet)
void updateA6()
{
    static unsigned long last = 0;
    static A6State cand = A6_NONE;
    if (millis() - last < 5)
        return;
    last = millis();
    A6State c = classifyA6(readA6());
    if (c != cand)
    {
        cand = c;
        return;
    }
    a6Stable = c;
    // MODE is latching: its state is only taken while no button is pressed
    bool opp = modeOpp;
    if (c == A6_MODE) opp = true;
    else if (c == A6_NONE) opp = false;
    if (opp != modeOpp)
    {
        modeOpp = opp;
        changed_f = true;
    }
}

/**************************************/
/* Button multi-click handling        */
/**************************************/
int checkButton()
{
    int event = 0;
    // Read the state of the button
    updateA6();
    buttonVal = (a6Stable == A6_STEP) ? LOW : HIGH;
    // Button pressed down
    if (buttonVal == LOW && buttonLast == HIGH && (millis() - upTime) > debounce) {
        downTime = millis();
        ignoreUp = false;
        waitForUp = false;
        singleOK = true;
        holdEventPast = false;
        longHoldEventPast = false;
        if ((millis() - upTime) < DCgap && DConUp == false && DCwaiting == true) DConUp = true;
        else DConUp = false;
        DCwaiting = false;
    }
    // Button released
    else if (buttonVal == HIGH && buttonLast == LOW && (millis() - downTime) > debounce) {
        if (not ignoreUp) {
            upTime = millis();
            if (DConUp == false) DCwaiting = true;
            else {
                event = 2;
                DConUp = false;
                DCwaiting = false;
                singleOK = false;
            }
        }
    }
    // Test for normal click event: DCgap expired
    if (buttonVal == HIGH && (millis() - upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true) {
        event = 1;
        DCwaiting = false;
    }
    // Test for hold
    if (buttonVal == LOW && (millis() - downTime) >= holdTime) {
        // Trigger "normal" hold
        if (not holdEventPast) {
            event = 3;
            waitForUp = true;
            ignoreUp = true;
            DConUp = false;
            DCwaiting = false;
            holdEventPast = true;
        }
        // Trigger "long" hold
        if ((millis() - downTime) >= longHoldTime) {
            if (not longHoldEventPast) {
                event = 4;
                longHoldEventPast = true;
            }
        }
    }
    buttonLast = buttonVal;
    return event;
}

/**************************************/
/* Band switch                        */
/**************************************/
uint8_t rawToBand(int raw)
{
    uint8_t b = 0;
    while (b < NUM_BANDS - 1 && raw >= (int)bandThreshold[b])
        b++;
    return b;
}

uint16_t readBandRaw()
{
    analogRead(BAND_SELECT_IN);             // dummy read, lets the ADC settle after switching channel
    return analogRead(BAND_SELECT_IN);
}

// Used once at power on: wait until the switch gives the same band 5 times in a row
uint8_t readBandBlocking()
{
    uint8_t last = rawToBand(readBandRaw());
    uint8_t same = 0;
    for (uint8_t i = 0; i < 50 && same < BAND_STABLE_READS; i++)
    {
        delay(BAND_SAMPLE_MS);
        uint8_t b = rawToBand(readBandRaw());
        if (b == last) same++;
        else { last = b; same = 0; }
    }
    return last;
}

// Called from loop(): changes band only when the new position is clear of the thresholds
// (hysteresis) and stable for BAND_STABLE_READS samples
void updateBand()
{
    if (millis() - lastBandSample < BAND_SAMPLE_MS)
        return;
    lastBandSample = millis();

    int raw = readBandRaw();
    bandRaw = raw;
    uint8_t lo = rawToBand(max(raw - BAND_HYST, 0));
    uint8_t hi = rawToBand(min(raw + BAND_HYST, 1023));
    uint8_t b = (lo == hi) ? lo : currentBand;     // inside a hysteresis zone: keep the current band

    if (b == currentBand)
    {
        bandStableCount = 0;
        return;
    }
    if (b != bandCandidate)
    {
        bandCandidate = b;
        bandStableCount = 1;
        return;
    }
    if (++bandStableCount < BAND_STABLE_READS)
        return;

    bandStableCount = 0;
    band[currentBand].lastFreq = vfo;
    currentBand = b;
    vfo = band[currentBand].lastFreq;
    rit_offset = 0;
    changed_f = true;
}

/**************************************/
/* Interrupt service routine for      */
/* encoder - only counts the steps    */
/**************************************/
// Quadrature decoder: every valid edge is +1 / -1, ENC_EDGES_PER_STEP edges = one step.
// Index = (old state << 2) | new state, state = (B << 1) | A. CW (as with the Rotary library
// before) is the sequence 11 -> 01 -> 00 -> 10 -> 11.
const int8_t encTable[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

uint8_t readEncoderPins()
{
    uint8_t p = PIND;                       // D2 = B (PD2), D3 = A (PD3)
    return (((p >> 2) & 1) << 1) | ((p >> 3) & 1);
}

ISR(PCINT2_vect) {
    uint8_t s = readEncoderPins();
    int8_t d = encTable[(encState << 2) | s];
    encState = s;
    if (d == 0)
        return;
    encLastEdge = millis();
    encAcc += d;
    if (encAcc >= ENC_EDGES_PER_STEP)
    {
        encAcc -= ENC_EDGES_PER_STEP;
        if (encoderSteps < 100) encoderSteps++;
    }
    else if (encAcc <= -ENC_EDGES_PER_STEP)
    {
        encAcc += ENC_EDGES_PER_STEP;
        if (encoderSteps > -100) encoderSteps--;
    }
}

// Knob resting in a detent: re-align the edge count to it. A leftover of half a step or more is a
// click that was not counted (detent not where the count expected it), so it is counted now.
void encoderRest()
{
    noInterrupts();
    if (encAcc != 0 && millis() - encLastEdge > ENC_REST_MS)
    {
        if (encAcc >= ENC_EDGES_PER_STEP / 2 && encoderSteps < 100) encoderSteps++;
        else if (encAcc <= -ENC_EDGES_PER_STEP / 2 && encoderSteps > -100) encoderSteps--;
        encAcc = 0;
    }
    interrupts();
}

int8_t takeEncoderSteps()
{
    encoderRest();
    noInterrupts();
    int8_t s = encoderSteps;
    encoderSteps = 0;
    interrupts();
    return s;
}

/**************************************/
/* S3 position (CW / CAL) on A7       */
/**************************************/
void updateS3()
{
#if CW_SENSE_FITTED
    static uint8_t cwCount = 0;
    static unsigned long lastS3 = 0;
    if (millis() - lastS3 < 20)
        return;
    lastS3 = millis();
    analogRead(S3_SENSE_IN);                // dummy read after switching the ADC channel
    s3Raw = analogRead(S3_SENSE_IN);
    bool cw = s3Raw >= S3_CW_MIN_RAW;
    bool cal = !cw && s3Raw >= S3_CAL_MIN_RAW;
    if (cal != calMode)
    {
        calMode = cal;
        if (!cal)
            calMarker = false;              // the marker never stays on outside CAL
        changed_f = true;                   // CLK2 on / off
    }
    // must read the same 3 times in a row before CW mode changes
    if (cw != cwMode)
    {
        if (++cwCount >= 3)
        {
            cwMode = cw;
            cwCount = 0;
            changed_f = true;
        }
    }
    else
        cwCount = 0;
#endif
}

/**************************************/
/* Apply encoder steps to VFO or RIT  */
/**************************************/
void tune(int8_t steps)
{
    if (steps == 0 || txon)                 // frequency is locked while transmitting
        return;

    if (rit)
    {
        rit_offset = constrain(rit_offset + (int32_t)steps * (int32_t)ritstep, -10000L, 10000L);
    }
    else
    {
        // Acceleration: clicks counted per 100 ms window; fast turning multiplies the step
        // (low: x10 above ~80 clicks/s; high: also x100 above ~250 clicks/s), max 10 kHz per click.
        static unsigned long winStart = 0;
        static uint8_t winCount = 0;
        if (millis() - winStart > 100)
        {
            winStart = millis();
            winCount = 0;
        }
        winCount = min(255, winCount + abs(steps));
        int32_t mult = 1;
        if (settings2.accel >= 1 && winCount > 8) mult = 10;
        if (settings2.accel >= 2 && winCount > 25) mult = 100;
        while (mult > 1 && (int32_t)radix * mult > 10000) mult /= 10;
        int32_t f = (int32_t)vfo + (int32_t)steps * (int32_t)radix * mult;
        f = constrain(f, (int32_t)band[currentBand].minFreq, (int32_t)band[currentBand].maxFreq);
        vfo = (uint32_t)f;
        // reset rit offset to zero if frequency changed in non rit mode
        rit_offset = 0;
        ritstep = 1;
    }
    lastTuneMillis = millis();
    changed_f = true;
}

/**************************************/
/* Program the Si5351                 */
/**************************************/
// PLL spurs: the Si5351 PLL makes strong spurs when its VCO is close to a whole multiple of the
// crystal or to a simple fraction of it (1/4, 1/3, 1/2, 2/3, 3/4). The spur offset at the output is
// about |fraction - p/q| * crystal / (q * divider). So every even output divider that keeps the VCO
// in range is scored by its distance from those points and the best one is used. The divider is
// kept while it is still good enough (hysteresis), so tuning normally only moves the PLL.
#define SPUR_KEEP_SCORE   150               // keep the divider while the score is at least this
int32_t corrPpb = 0;                        // crystal correction in ppb (set in setup / menu)

// Score in 1/10000: min over p/q (q <= 4) of |frac(VCO / xtal) - p/q| / q. Higher = cleaner.
uint16_t spurScore(uint64_t f100, uint16_t d)
{
    uint64_t ref100 = 2500000000ULL + (uint64_t)((int64_t)2500000000LL * corrPpb / 1000000000LL);
    uint64_t vco100 = f100 * d;
    uint32_t fr = (uint32_t)(((vco100 % ref100) * 10000ULL) / ref100);   // 0..9999
    static const uint8_t qs[7] = {1, 4, 3, 2, 3, 4, 1};
    static const uint16_t ps[7] = {0, 2500, 3333, 5000, 6667, 7500, 10000};
    uint16_t best = 10000;
    for (uint8_t i = 0; i < 7; i++)
    {
        uint16_t dist = (fr > ps[i]) ? fr - ps[i] : ps[i] - fr;
        uint16_t s = dist / qs[i];
        if (s < best)
            best = s;
    }
    return best;
}

// Even divider for output f100 (in 0.01 Hz): keep 'current' if it is still in range and clean
// enough, otherwise the cleanest one in range.
uint16_t chooseDivider(uint64_t f100, uint16_t current)
{
    uint32_t f = (uint32_t)(f100 / 100);
    uint16_t dMin = (uint16_t)((VCO_MIN_HZ + f - 1) / f);
    uint16_t dMax = (uint16_t)(VCO_MAX_HZ / f);
    if (dMin & 1) dMin++;
    if (current >= dMin && current <= dMax && !(current & 1) && spurScore(f100, current) >= SPUR_KEEP_SCORE)
        return current;
    uint16_t bestD = dMax & ~1, bestS = 0;
    for (uint16_t d = dMin; d <= dMax; d += 2)
    {
        uint16_t s = spurScore(f100, d);
        if (s > bestS) { bestS = s; bestD = d; }
    }
    return bestD;
}

uint16_t lastBfoDivider = 0;
uint32_t lastBfoSet = 0;

void updateSi5351()
{
    int32_t rf = (int32_t)vfo;
    if (rit && !txon)
        rf += rit_offset;

    // MODE OPP: carrier on the other side of the filter, LO follows so the dial stays right.
    // CW always uses NORM (Atlas manual: sideband selector in NORM for CW).
    uint64_t bfoRx = bfo + ((modeOpp && !cwMode) ? (uint64_t)settings2.oppOffsetHz * SI5351_FREQ_MULT : 0);
    uint64_t clk0 = (uint64_t)rf * SI5351_FREQ_MULT;
    if (band[currentBand].ifPositive)
        clk0 += bfoRx;
    else
        clk0 -= bfoRx;

    // LO: even integer divider on PLLA. Same divider = only the PLL registers are written
    // (fewer I2C writes per tuning step = less noise while tuning).
    uint16_t d = chooseDivider(clk0, loDivider);
    if (d != loDivider)
    {
        loDivider = d;
        si5351.set_freq_manual(clk0, clk0 * d, SI5351_CLK0);
        si5351.set_int(SI5351_CLK0, 1);
        si5351.pll_reset(SI5351_PLLA);
    }
    else
        si5351.set_pll(clk0 * d, SI5351_PLLA);

    // BFO / carrier: own PLLB, even integer divider. In CW the carrier moves into the passband.
    uint64_t clk1 = bfoRx + (cwMode ? (uint64_t)settings2.cwOffsetHz * SI5351_FREQ_MULT : 0);
    if (clk1 != lastBfoSet)
    {
        uint16_t dB = chooseDivider(clk1, 0);
        si5351.set_freq_manual(clk1, clk1 * dB, SI5351_CLK1);
        si5351.set_int(SI5351_CLK1, 1);
        if (dB != lastBfoDivider)
            si5351.pll_reset(SI5351_PLLB);
        lastBfoDivider = dB;
        lastBfoSet = clk1;
        bfoPllFreq = clk1 * dB;
        lastClk2 = 0;                       // CLK2 divider depends on PLLB
    }
    updateClk2();
}

// CAL marker: CLK2 fixed on the CAL point nearest to the dial, fractional multisynth on PLLB.
// Off (high-Z) outside CAL, with CAL MARKER off and on TX.
bool dialLsb()
{
    bool lsb = band[currentBand].ifPositive;     // Atlas: LSB on 80 / 40, USB on 20 / 15 / 10
    return modeOpp ? !lsb : lsb;
}

uint32_t nearestCalPoint()
{
    uint32_t best = 0, bestD = 0xFFFFFFFFUL;
    for (uint8_t i = 0; i < 3; i++)
    {
        uint32_t p = (uint32_t)calPointsKhz[currentBand][i] * 1000UL;
        uint32_t d = (vfo > p) ? vfo - p : p - vfo;
        if (d < bestD) { bestD = d; best = p; }
    }
    return best;
}

void updateClk2()
{
    if (calMode && calMarker && !txon && bfoPllFreq)
    {
        uint64_t f100 = (uint64_t)nearestCalPoint() * SI5351_FREQ_MULT;
        if (f100 != lastClk2 || !clk2On)
        {
            si5351.set_freq_manual(f100, bfoPllFreq, SI5351_CLK2);
            si5351.set_int(SI5351_CLK2, 0);
            si5351.drive_strength(SI5351_CLK2, (si5351_drive)settings2.calDrive);
            si5351.output_enable(SI5351_CLK2, clk2PinOff ? 0 : 1);
            lastClk2 = f100;
            clk2On = true;
        }
    }
    else if (clk2On)
    {
        si5351.output_enable(SI5351_CLK2, 0);
        clk2On = false;
    }
}

/*****************************/
/* display S meter           */
/*****************************/
// mV -> dB above S0, linear between calibration points, last slope continues above the table
int smeterDb(int sigMv)
{
    if (sigMv <= smeterCalMv[0])
        return smeterCalDb[0];
    uint8_t i = 1;
    while (i < SMETER_CAL_POINTS - 1 && sigMv > smeterCalMv[i])
        i++;
    long db = smeterCalDb[i - 1] + ((long)(sigMv - smeterCalMv[i - 1]) * (smeterCalDb[i] - smeterCalDb[i - 1]))
                                    / (smeterCalMv[i] - smeterCalMv[i - 1]);
    return (int)db;
}

// Load CGRAM slot k (0..4) with digit k small (lit = false) or inverted (lit = true)
void loadDigit(uint8_t k, bool lit)
{
    uint8_t g[8];
    g[0] = lit ? B11111 : 0;
    g[7] = lit ? B11111 : 0;
    for (uint8_t r = 0; r < 6; r++)
    {
        uint8_t row = smallDigit[k][r] << 1;          // 3 dots in the middle of 5
        g[r + 1] = lit ? (uint8_t)(~row & B11111) : row;
    }
    lcd.createChar(k, g);
    digitLit[k] = lit;
}

void display_smeter(int sigMv)
{
    // Line 2 cols 4-12: S1..S9. Odd S-units = small digit, inverted once reached; even S-units =
    // bar (two lines, one line = 3 dB). Cols 13-15: over S9 as text "+", "+10".."+40".
    int db = constrain(smeterDb(sigMv), 0, 94);
    int halves = (db < 54 ? db : 54) / 3;   // 0..18
    int over = (db > 54) ? db - 54 : 0;     // dB over S9
    int overShown = (over == 0) ? -1 : (over / 10) * 10;   // -1 = nothing, 0 = "+", 10.. = "+10"..
    // Only write the LCD when something visible changes: every LCD write is a burst of
    // digital edges on the data lines that can be heard in the receiver.
    if (halves == smeterLastBars && overShown == smeterLastOver)
        return;
    smeterLastBars = halves;
    smeterLastOver = overShown;
#if SMETER_DIGIT_STYLE == 1
    for (uint8_t k = 0; k < 5; k++)          // digit slots first (createChar moves the cursor)
    {
        bool lit = (halves - 4 * k) >= 2;
        if (digitLit[k] != (int8_t)lit)
            loadDigit(k, lit);
    }
#endif
    lcd.setCursor(4, 1);
    for (int i = 0; i < 9; i++)
    {
        int h = halves - 2 * i;
        if ((i & 1) == 0)
#if SMETER_DIGIT_STYLE == 1
            lcd.write(byte(i / 2));         // digit field (small or inverted)
#elif SMETER_DIGIT_STYLE == 2
        {
            if (h >= 2)
                lcd.write(byte(i / 2));     // underlined copy of the LCD digit
            else
                lcd.print((char)('1' + i));
        }
#else
            lcd.print((char)('1' + i));     // digit field: normal LCD digit 1 3 5 7 9
#endif
        else if (h >= 2)
            lcd.write(byte(5));             // both lines
        else if (h == 1)
            lcd.write(byte(6));             // left line only
        else
            lcd.print(' ');
    }
    if (overShown < 0)
        lcd.print(F("   "));
    else if (overShown == 0)
        lcd.print(F("+  "));
    else
    {
        lcd.print('+');
        lcd.print(overShown);
    }
}

// CAL: row 2 cols 4-15 show the level in dBm instead of the bar (S0 = -127 dBm, the meter does
// not move below about -90 dBm). Redrawn at most every 300 ms and only when the value changes.
void display_dbm(int sigMv)
{
    static int lastDbm = 0;
    static unsigned long lastDraw = 0;
    int db = constrain(smeterDb(sigMv), 0, 114);
    int dbm = (db < 37) ? -90 : db - 127;
    bool force = smeterLastBars >= -1;      // bar was on the LCD, or a redraw was asked for
    if (!force && (dbm == lastDbm || millis() - lastDraw < 300))
        return;
    lastDbm = dbm;
    lastDraw = millis();
    smeterLastBars = -3;                    // the bar must be drawn again after CAL
    lcd.setCursor(4, 1);
    lcd.print(db < 37 ? F(" <") : F("  "));
    lcd.print(dbm);
    lcd.print(F(" dBm   "));
}

// Row 2 cols 0-3: ATT / PRE / blank
void display_flags2()
{
    flagsDirty = false;
    lcd.setCursor(0, 1);
    lcd.print(attOn ? F("ATT ") : (preOn ? F("PRE ") : F("NOR ")));
}

// D4 / D5 / D6 read every 50 ms; a change redraws only what it affects
void updateSense()
{
    static unsigned long last = 0;
    if (millis() - last < 50)
        return;
    last = millis();
    bool f = !digitalRead(AGC_SENSE), a = !digitalRead(ATT_SENSE), p = !digitalRead(PRE_SENSE);
    if (f != agcFast)
    {
        agcFast = f;
        changed_f = true;                   // row 1 col 15
    }
    if (a != attOn || p != preOn)
    {
        attOn = a;
        preOn = p;
        flagsDirty = true;
    }
    if (flagsDirty)
        display_flags2();
}

// Average of SMETER_SAMPLES readings in mV (5 V reference)
uint16_t readAvgMv(uint8_t pin)
{
    analogRead(pin);                        // dummy read after switching the ADC channel
    uint32_t sum = 0;
    for (uint8_t i = 0; i < SMETER_SAMPLES; i++)
        sum += analogRead(pin);
    return (sum * 5000UL) / (1023UL * SMETER_SAMPLES);
}

void updateSmeter()
{
    if (millis() - lastSmeter < SMETER_PERIOD_MS)
        return;
    lastSmeter = millis();
    smeterMv = readAvgMv(SMETER_IN);
    smeterRefMv = readAvgMv(SMETER_REF_IN);
    smeterDiffMv = (int16_t)smeterMv - (int16_t)smeterRefMv;
    if (smeterDiffMv > smeterShownMv)
        smeterShownMv = smeterDiffMv;
    else
        smeterShownMv -= (smeterShownMv - smeterDiffMv + SMETER_DECAY - 1) / SMETER_DECAY;
    if (calMode)
        display_dbm(smeterShownMv);
    else
        display_smeter(smeterShownMv);
}

void display_rit()
{
    lcd.setCursor(0, 0);
    lcd.print(F("RIT:            "));
    lcd.setCursor(6, 0);
    lcd.print(rit_offset);
    lcd.print(F("Hz"));

    lcd.setCursor(15, 0);
    switch (ritstep)
    {
        case 1:
            lcd.print('1');
            break;
        case 100:
            lcd.print('2');
            break;
        case 2500:
            lcd.print('3');
            break;
    }
    show_i2c_flag();
}

// '!' in column 14 of line 1 after any I2C timeout since power on
// (the Si5351 was re-programmed automatically, but it shows that the fault happened)
void show_i2c_flag()
{
    lcd.setCursor(14, 0);
    lcd.print(i2cTimeouts ? '!' : ' ');
}

void print3(uint16_t v)
{
    if (v < 100)
        lcd.print('0');
    if (v < 10)
        lcd.print('0');
    lcd.print(v);
}

/**************************************/
/* Displays the frequency             */
/**************************************/
void display_frequency()
{
    // Row 1: cols 0-2 mode (USB / LSB / CW / TX), 4-13 frequency, 14 '!', 15 AGC F / S
    uint32_t f = vfo;
    lcd.setCursor(0, 0);
    if (cwMode)
        lcd.print(F("CW  "));
    else if (txon)
        lcd.print(F("TX  "));
    else if (calMode)
        lcd.print(calMarker ? F("MRK ") : F("CAL "));
    else
        lcd.print(dialLsb() ? F("LSB ") : F("USB "));
    uint16_t mhz = f / 1000000UL;
    if (mhz < 10)
        lcd.print(' ');
    lcd.print(mhz);
    lcd.print('.');
    print3((f / 1000UL) % 1000);
    lcd.print('.');
    print3(f % 1000UL);
    lcd.print(i2cTimeouts ? '!' : ' ');     // col 14
    lcd.print(agcFast ? 'F' : 'S');         // col 15
}

void show_main()
{
    if (rit && !txon)
        display_rit();
    else
        display_frequency();
}

void saveSettings()
{
    settings.vfo = vfo;
    eeprom_update_block((const void*)&settings, (void*)0, sizeof(settings));   // only writes changed bytes
}

void vfosteps()
{
    switch (radix)
    {
        case 1:       radix = 10;      break;
        case 10:      radix = 100;     break;
        case 100:     radix = 1000;    break;
        case 1000:    radix = 10000;   break;
        case 10000:   radix = 100000;  break;
        case 100000:  radix = 1000000; break;
        default:      radix = 1;       break;
    }
    if (!setupmenu)
    {
        // show cursor under the digit that is being tuned: "VFO: 14.100.000"
        uint8_t col;
        switch (radix)
        {
            case 1:       col = 13; break;
            case 10:      col = 12; break;
            case 100:     col = 11; break;
            case 1000:    col = 9;  break;
            case 10000:   col = 8;  break;
            case 100000:  col = 7;  break;
            default:      col = 5;  break;
        }
        lcd.setCursor(col, 0);
        lcd.cursor();
        delay(500);
        lcd.noCursor();
    }
}

/**************************************/
/* Setup menu (STEP held at power on, */
/* or MENU held 1 s)                  */
/*  encoder      : change value       */
/*  STEP click   : change step size   */
/*  MENU click / STEP double: next    */
/*  hold STEP 2 s: save and exit      */
/*  hold MENU 2 s: exit, no saving    */
/**************************************/
#define MENU_ITEMS 11
const uint32_t stepList[] = {1, 10, 100, 1000, 10000};

/**************************************/
/* AUTO CAL with a GPSDO 1PPS         */
/**************************************/
// CLK2 (AUTOCAL_HZ) -> CAL box 1 k -> D5 = Timer1 external clock (T1), GPSDO 1PPS -> 1 k -> D6.
// Rocker on NOR, so the ATT / PRE sense diodes do not clamp D5 / D6. Counting runs with interrupts
// off (no millis, no encoder) and polls D6, so an edge is caught within one loop (~1 us = 2-3 counts).
// Returns 0 = OK, 1 = no PPS, 2 = no / wrong CLK2 count.
uint8_t countClk2(uint32_t *count)
{
    uint8_t oldA = TCCR1A, oldB = TCCR1B, oldMask = TIMSK1;
    TIMSK1 = 0;
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    TIFR1 = _BV(TOV1);
    TCCR1B = _BV(CS12) | _BV(CS11) | _BV(CS10);   // clock on T1 rising edge
    uint8_t sreg = SREG;
    cli();
    uint32_t ovf = 0, start = 0, idle = 0;
    uint8_t edges = 0, status = 0;
    bool last = PIND & _BV(6);
    while (true)
    {
        if (TIFR1 & _BV(TOV1))
        {
            TIFR1 = _BV(TOV1);
            ovf++;
        }
        bool now = PIND & _BV(6);
        if (now && !last)                   // PPS rising edge
        {
            uint16_t t = TCNT1;
            uint32_t o = ovf;
            if ((TIFR1 & _BV(TOV1)) && t < 0x8000)
                o++;                        // overflow just happened, not counted yet
            uint32_t stamp = (o << 16) | t;
            if (edges == 0)
                start = stamp;
            else if (edges == AUTOCAL_SECONDS)
            {
                *count = stamp - start;
                break;
            }
            edges++;
            idle = 0;
        }
        last = now;
        if (++idle > AUTOCAL_TIMEOUT_LOOPS)
        {
            status = 1;
            break;
        }
    }
    SREG = sreg;
    TCCR1B = 0;
    TCCR1A = oldA;
    TCCR1B = oldB;
    TIMSK1 = oldMask;
    uint32_t expected = AUTOCAL_HZ * AUTOCAL_SECONDS;
    if (status == 0 && (*count < expected - expected / 2000 || *count > expected + expected / 2000))
        status = 2;                         // more than 500 ppm off = not CLK2
    return status;
}

void setCorrection(int32_t corr)
{
    si5351.set_correction(corr, SI5351_PLL_INPUT_XO);
    corrPpb = corr;
    lastBfoSet = 0;                         // both PLLs must be calculated again
    loDivider = 0;
    updateSi5351();
}

void startAutoCalClk2()
{
    si5351.set_freq_manual((uint64_t)AUTOCAL_HZ * SI5351_FREQ_MULT, bfoPllFreq, SI5351_CLK2);
    si5351.set_int(SI5351_CLK2, 0);
    si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
    si5351.output_enable(SI5351_CLK2, 1);
    clk2On = true;                          // so updateClk2 switches it off afterwards
    lastClk2 = 0;
}

// One measurement: error in ppb (count against AUTOCAL_HZ), or status 1 / 2
uint8_t autoCalPass(int32_t *errPpb)
{
    lcd.setCursor(0, 1);
    lcd.print(F("COUNTING 10 s   "));
    uint32_t count = 0;
    uint8_t st = countClk2(&count);
    if (st == 0)
    {
        int64_t expected = (int64_t)AUTOCAL_HZ * AUTOCAL_SECONDS;
        *errPpb = (int32_t)(((int64_t)count - expected) * 1000000000LL / expected);
    }
    return st;
}

// Measure, correct, measure again. STEP click = keep, knob = undo. Returns the correction to use.
int32_t autoCal(int32_t corr)
{
    int32_t oldCorr = corr, err1 = 0, err2 = 0;
    lcd.clear();
    lcd.print(F("AUTO CAL  NOR!"));
    startAutoCalClk2();
    delay(200);
    uint8_t st = autoCalPass(&err1);
    if (st == 0)
    {
        corr = constrain(corr + err1, -300000L, 300000L);   // crystal high by err -> correction + err
        setCorrection(corr);
        startAutoCalClk2();
        delay(200);
        st = autoCalPass(&err2);
    }
    si5351.output_enable(SI5351_CLK2, 0);
    lastClk2 = 0;
    lcd.clear();
    if (st != 0)
    {
        lcd.print(st == 1 ? F("NO PPS (D6)") : F("NO CLK2 (D5)"));
        lcd.setCursor(0, 1);
        lcd.print(F("CORR unchanged"));
        if (corr != oldCorr)
            setCorrection(oldCorr);
        delay(3000);
        return oldCorr;
    }
    lcd.print(F("E:"));
    lcd.print(err1);
    lcd.print(F(" R:"));
    lcd.print(err2);
    lcd.setCursor(0, 1);
    lcd.print(F("click=OK turn=NO"));
    takeEncoderSteps();
    while (true)
    {
        if (checkButton() == 1)
            return corr;
        if (takeEncoderSteps())
        {
            setCorrection(oldCorr);
            lcd.clear();
            lcd.print(F("CORR unchanged"));
            delay(1500);
            return oldCorr;
        }
    }
}

void saveSettings2()
{
    settings2.magic = SETTINGS2_MAGIC;
    eeprom_update_block((const void*)&settings2, (void*)SETTINGS2_ADDR, sizeof(settings2));
}

void runSetupMenu()
{
    setupmenu = true;
    lcd.clear();
    lcd.print(F("VFO SETUP MODE"));
    lcd.setCursor(0, 1);
    lcd.print(F("release button"));
    while (readA6() < A6_MENU_MAX)          // wait until STEP / MENU is released
        ;
    delay(500);
    buttonLast = HIGH;
    upTime = millis();

    int32_t bfoHz = bfo / 100;
    int32_t corr = (int32_t)settings.fcorrection * 10L;   // ppb
    uint8_t item = 0;
    bool redraw = true;
    bool menuWasDown = false;
    unsigned long menuPressedAt = 0;
    bool cancel = false;
    settings2_t oldSettings2 = settings2;   // for leaving without saving
    uint32_t oldBfo = bfo;
    int32_t oldCorr = corr;
    bool oldMarker = calMarker;
    radix = 100;

    while (true)
    {
        int8_t s = takeEncoderSteps();
        if (s)
        {
            switch (item)
            {
                case 0:
                    bfoHz = constrain(bfoHz + (int32_t)s * (int32_t)radix, 1000000L, 20000000L);
                    bfo = (uint32_t)bfoHz * 100UL;
                    break;
                case 1:
                    corr = constrain(corr + (int32_t)s * (int32_t)radix, -300000L, 300000L);
                    si5351.set_correction(corr, SI5351_PLL_INPUT_XO);
                    corrPpb = corr;
                    lastBfoSet = 0;         // both PLLs must be calculated again
                    loDivider = 0;
                    break;
                case 2:
                    settings2.oppOffsetHz = constrain((int32_t)settings2.oppOffsetHz + (int32_t)s * (int32_t)min(radix, 1000UL), 1000L, 6000L);
                    break;
                case 3:
                    settings2.cwOffsetHz = constrain((int32_t)settings2.cwOffsetHz + (int32_t)s * (int32_t)min(radix, 100UL), 300L, 1500L);
                    break;
                case 4:
                {
                    int8_t i = 0;
                    while (i < 4 && stepList[i] != settings2.defaultStep) i++;
                    i = constrain(i + (s > 0 ? 1 : -1), 0, 4);
                    settings2.defaultStep = stepList[i];
                    break;
                }
                case 5:
                    settings2.accel = constrain((int8_t)settings2.accel + (s > 0 ? 1 : -1), 0, 2);
                    break;
                case 6:
                    settings2.catOn = !settings2.catOn;
                    break;
                case 7:
                    settings2.catBaud = constrain((int8_t)settings2.catBaud + (s > 0 ? 1 : -1), 0, 3);
                    break;
                case 8:
                    settings2.calDrive = constrain((int8_t)settings2.calDrive + (s > 0 ? 1 : -1), 0, 3);
                    lastClk2 = 0;           // apply the new level if the marker is on
                    break;
                case 9:
                    calMarker = !calMarker;
                    break;
            }
            updateSi5351();                 // hear the change live
            redraw = true;
        }

        int b = checkButton();
        if (b == 1 && item == 10) { corr = autoCal(corr); redraw = true; }
        else if (b == 1) { vfosteps(); redraw = true; }
        if (b == 2) { item = (item + 1) % MENU_ITEMS; redraw = true; }
        if (b == 3) break;
        bool menuDown = (a6Stable == A6_MENU);
        if (menuDown && !menuWasDown) { item = (item + 1) % MENU_ITEMS; redraw = true; menuPressedAt = millis(); }
        menuWasDown = menuDown;
        if (menuDown && millis() - menuPressedAt >= 2000) { cancel = true; break; }

        if (redraw)
        {
            redraw = false;
            lcd.clear();
            switch (item)
            {
                case 0: lcd.print(F("BFO:")); lcd.print(bfoHz); lcd.print(F("Hz")); break;
                case 1: lcd.print(F("CORR:")); lcd.print(corr); lcd.print(F("ppb")); break;
                case 2: lcd.print(F("OPP OFFS:")); lcd.print(settings2.oppOffsetHz); lcd.print(F("Hz")); break;
                case 3: lcd.print(F("CW OFFS:")); lcd.print(settings2.cwOffsetHz); lcd.print(F("Hz")); break;
                case 4: lcd.print(F("START STEP:")); lcd.print(settings2.defaultStep); break;
                case 5:
                    lcd.print(F("ACCEL: "));
                    lcd.print(settings2.accel == 0 ? F("OFF") : (settings2.accel == 1 ? F("LOW") : F("HIGH")));
                    break;
                case 6: lcd.print(F("CAT: ")); lcd.print(settings2.catOn ? F("ON") : F("OFF")); break;
                case 7: lcd.print(F("CAT BAUD:")); lcd.print(catBauds[settings2.catBaud]); break;
                case 8: lcd.print(F("CAL LEVEL:")); lcd.print(2 + 2 * settings2.calDrive); lcd.print(F("mA")); break;
                case 9: lcd.print(F("CAL MARKER: ")); lcd.print(calMarker ? F("ON") : F("OFF")); break;
                case 10: lcd.print(F("AUTO CAL")); break;
            }
            lcd.setCursor(0, 1);
            if (item <= 3)
            {
                lcd.write(byte(7));
                lcd.print(F("STEP:"));
                lcd.print(item == 2 ? min(radix, 1000UL) : (item == 3 ? min(radix, 100UL) : radix));
            }
            else if (item == 10)
                lcd.print(F("STEP=start"));
            else
                lcd.print(F("turn knob"));
            lcd.setCursor(item >= 9 ? 11 : 12, 1);
            lcd.print(item + 1);
            lcd.print('/');
            lcd.print(MENU_ITEMS);
        }
    }

    lcd.clear();
    if (cancel)
    {
        settings2 = oldSettings2;
        bfo = oldBfo;
        calMarker = oldMarker;
        setCorrection(oldCorr);             // also programs both PLLs with the old BFO
        lcd.print(F("NOT SAVED"));
        while (readA6() < A6_MENU_MAX)      // wait for MENU release (no menu again from loop)
            ;
        a6Stable = A6_NONE;                 // no stale MENU (would count as a short press in CAL)
    }
    else
    {
        settings.bfo_offset = (int32_t)bfo;
        settings.fcorrection = (int)(corr / 10L);
        saveSettings();
        saveSettings2();
        lcd.print(F("SAVED"));
    }
    delay(1000);
    lcd.clear();
    radix = settings2.defaultStep;
    setupmenu = false;
    startSerial();                          // CAT on / off or baud may have changed
}

/**************************************/
/* CAT: Kenwood TS-480 subset         */
/**************************************/
void startSerial()
{
    Serial.end();
    Serial.begin(settings2.catOn ? catBauds[settings2.catBaud] : DEBUG_BAUD);
    catLen = 0;
    if (!settings2.catOn)
        setCatPtt(false);
}

void setCatPtt(bool on)
{
    if (calMode)                            // never key in the CAL position
        on = false;
    catTx = on;
    digitalWrite(CAT_PTT_PIN, on ? HIGH : LOW);
}

uint8_t catMode()                           // 1 = LSB, 2 = USB, 3 = CW
{
    if (cwMode) return 3;
    return dialLsb() ? 1 : 2;
}

void catFreq11(uint32_t f)
{
    char d[11];
    for (int8_t i = 10; i >= 0; i--)
    {
        d[i] = '0' + f % 10;
        f /= 10;
    }
    Serial.write(d, 11);
}

// S meter as TS-480 0..30 (S9 = 15, S9+60 = 30)
uint8_t catSmeter()
{
    int db = constrain(smeterDb(smeterShownMv), 0, 114);
    return db <= 54 ? (uint8_t)(db * 15 / 54) : (uint8_t)min(30, 15 + (db - 54) / 4);
}

void catCommand()
{
    catBuf[catLen] = 0;
    char a = catBuf[0], b = catBuf[1];
    bool query = (catLen == 2);
    if (a == 'F' && (b == 'A' || b == 'B'))
    {
        if (query)
        {
            Serial.print('F'); Serial.print(b); catFreq11(vfo); Serial.print(';');
        }
        else if (b == 'A' && !txon)
        {
            uint32_t f = strtoul(catBuf + 2, NULL, 10);
            if (f >= band[currentBand].minFreq && f <= band[currentBand].maxFreq)
            {
                vfo = f;                    // only inside the band the switch is on
                rit_offset = 0;
                lastTuneMillis = millis();
                changed_f = true;
            }
        }
    }
    else if (a == 'I' && b == 'F')
    {
        Serial.print(F("IF"));
        catFreq11(vfo);
        Serial.print(F("00000+000000000"));
        Serial.print((txon || catTx) ? '1' : '0');
        Serial.print(catMode());
        Serial.print(F("000000 ;"));
    }
    else if (a == 'M' && b == 'D')
    {
        if (query) { Serial.print(F("MD")); Serial.print(catMode()); Serial.print(';'); }
        // mode set ignored: the sideband is the band / MODE rocker, CW is S3
    }
    else if (a == 'T' && b == 'X')
        setCatPtt(true);
    else if (a == 'R' && b == 'X')
        setCatPtt(false);
    else if (a == 'I' && b == 'D')
        Serial.print(F("ID020;"));
    else if (a == 'P' && b == 'S')
    {
        if (query) Serial.print(F("PS1;"));
    }
    else if (a == 'A' && b == 'I')
    {
        if (query) Serial.print(F("AI0;"));
    }
    else if ((a == 'F' && (b == 'R' || b == 'T')))
    {
        if (query) { Serial.print('F'); Serial.print(b); Serial.print(F("0;")); }
    }
    else if (a == 'S' && b == 'M')
    {
        uint8_t v = catSmeter();
        Serial.print(F("SM00"));
        if (v < 10) Serial.print('0');
        Serial.print(v);
        Serial.print(';');
    }
    else
        Serial.print(F("?;"));
}

void serviceCat()
{
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == ';')
        {
            if (catLen >= 2)
                catCommand();
            catLen = 0;
        }
        else if (c >= ' ' && catLen < sizeof(catBuf) - 1)
            catBuf[catLen++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
    }
}

void setup()
{
    digitalWrite(CAT_PTT_PIN, LOW);          // no CAT TX at power on
    pinMode(CAT_PTT_PIN, OUTPUT);
    Serial.begin(DEBUG_BAUD);
    pinMode(BAND_SELECT_IN, INPUT);
    pinMode(BTN_IN, INPUT);
    pinMode(TX_BTN, INPUT_PULLUP);
    pinMode(AGC_SENSE, INPUT_PULLUP);
    pinMode(ATT_SENSE, INPUT_PULLUP);
    pinMode(PRE_SENSE, INPUT_PULLUP);

    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);
#if SMETER_DIGIT_STYLE == 2
    for (uint8_t k = 0; k < 5; k++)
        lcd.createChar(k, ulDigit[k]);
#endif
    lcd.createChar(5, bar5);
    lcd.createChar(6, barHalf);
    lcd.createChar(7, bar1);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("YT3CT ATLAS 210X"));
    lcd.setCursor(4, 1);
    lcd.print(F("loading"));
    delay(1000);

    currentBand = readBandBlocking();
    bandCandidate = currentBand;

    // read from eeprom; defaults if it is blank or does not look like our data
    eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
    if (settings.bfo_offset < 100000000L || settings.bfo_offset > 2000000000L ||
        settings.fcorrection < -30000 || settings.fcorrection > 30000)
    {
        // Defaults measured 2026-09-28 against the FTdx10 / TinySA (Si5351 crystal is ~101 ppm high):
        // BFO 5.520000 MHz, correction +101000 ppb. Change in the setup menu if the Si5351 is replaced.
        settings.bfo_offset = 552000000L;
        settings.superhet = true;
        settings.fcorrection = 10100;        // x10 ppb = +101000 ppb
        settings.vfo = band[currentBand].lastFreq;
    }
    bfo = settings.bfo_offset;
    eeprom_read_block((void*)&settings2, (void*)SETTINGS2_ADDR, sizeof(settings2));
    if (settings2.magic != SETTINGS2_MAGIC)
    {
        settings2.oppOffsetHz = OPP_OFFSET_HZ;
        settings2.cwOffsetHz = CW_OFFSET_HZ;
        settings2.defaultStep = 10;
        settings2.accel = 1;
        settings2.magic2 = 0;
    }
    if (settings2.magic2 != SETTINGS2_MAGIC2)    // batch 2 fields: defaults, batch 1 values kept
    {
        settings2.catOn = 0;
        settings2.catBaud = 1;               // 9600
        settings2.calDrive = 0;              // 2 mA
        settings2.magic2 = SETTINGS2_MAGIC2;
        saveSettings2();
    }
    radix = settings2.defaultStep;

    // restore the saved frequency if it belongs to the band the switch is on
    if (settings.vfo >= band[currentBand].minFreq && settings.vfo <= band[currentBand].maxFreq)
        band[currentBand].lastFreq = settings.vfo;
    vfo = band[currentBand].lastFreq;

    // I2C with timeout: a glitch can no longer hang the sketch forever
    Wire.begin();
    Wire.setWireTimeout(WIRE_TIMEOUT_US, true);

    //initialize the Si5351
    bool i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0); //If you're using a 27Mhz crystal, put in 27000000 instead of 0
    Wire.setWireTimeout(WIRE_TIMEOUT_US, true);
    if (!i2c_found)
    {
        lcd.clear();
        lcd.print(F("Si5351 NOT FOUND"));
        delay(2000);
    }
    si5351.set_correction((int32_t)settings.fcorrection * 10L, SI5351_PLL_INPUT_XO);
    corrPpb = (int32_t)settings.fcorrection * 10L;
    si5351.set_ms_source(SI5351_CLK0, SI5351_PLLA);   // LO
    si5351.set_ms_source(SI5351_CLK1, SI5351_PLLB);   // BFO / carrier, own PLL
    si5351.set_ms_source(SI5351_CLK2, SI5351_PLLB);   // CAL marker shares the BFO PLL
    si5351.set_clock_disable(SI5351_CLK2, SI5351_CLK_DISABLE_HI_Z);   // off = high-Z (D5 tap)
    si5351.output_enable(SI5351_CLK2, 0);
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA); //you can set this to 2MA, 4MA, 6MA or 8MA
    si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA); //be careful though - measure into 50ohms
                                                          //(8 mA tested 2026-09-28: no change in audio/AGC level)

    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    encState = readEncoderPins();
    PCICR |= (1 << PCIE2);           // Enable pin change interrupt for the encoder
    PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
    sei();

    if (classifyA6(readA6()) == A6_STEP)    // STEP held at power on = setup menu
        runSetupMenu();

    txon = !digitalRead(TX_BTN);
    if (settings2.catOn)
        startSerial();
    lcd.clear();
    smeterLastBars = -1;             // force the S meter line to be drawn
    flagsDirty = true;
    changed_f = true;                // program CLK0/CLK1 and draw the display on the first loop

#if DEBUG_SERIAL
    Serial.print(F("Start band="));
    Serial.print(band[currentBand].name);
    Serial.print(F(" vfo="));
    Serial.print(vfo);
    Serial.print(F(" bfo="));
    Serial.print(bfo);
    Serial.print(F(" si5351="));
    Serial.println(i2c_found ? F("OK") : F("NOT FOUND"));
#endif
}

void loop()
{
    // TX/RX change: re-program CLK0 so RIT is removed on TX and restored on RX
    bool tx = !digitalRead(TX_BTN);
    if (tx != txon)
    {
        txon = tx;
        changed_f = true;
    }

    updateBand();
    updateS3();
    updateSense();
    tune(takeEncoderSteps());

    // I2C timeout happened: program the Si5351 again
    if (Wire.getWireTimeoutFlag())
    {
        Wire.clearWireTimeoutFlag();
        i2cTimeouts++;
        lastBfoSet = 0;              // program both outputs again
        loDivider = 0;
        changed_f = true;
    }

    if (changed_f)
    {
        changed_f = false;
        updateSi5351();
        show_main();
    }

    updateSmeter();

    // MENU held MENU_HOLD_MS = setup menu, at any time
    if (a6Stable == A6_MENU)
    {
        if (menuDownSince == 0)
            menuDownSince = millis();
        else if (millis() - menuDownSince >= MENU_HOLD_MS && !txon)
        {
            menuDownSince = 0;
            runSetupMenu();
            lcd.clear();
            smeterLastBars = -1;
            flagsDirty = true;
            changed_f = true;
        }
    }
    else
    {
        // S3 in CAL: a short MENU press (released to no button) turns the marker on / off
        if (menuDownSince != 0 && calMode && (a6Stable == A6_NONE || a6Stable == A6_MODE) &&
            millis() - menuDownSince >= 50)
        {
            calMarker = !calMarker;
            changed_f = true;
        }
        menuDownSince = 0;
    }

    int b = checkButton();
    if (b == 1) clickEvent();
    if (b == 2) doubleClickEvent();
    if (b == 4) longHoldEvent();

    // save to eeprom SAVE_DELAY_MS after the last frequency change
    if (vfo != settings.vfo && millis() - lastTuneMillis > SAVE_DELAY_MS)
        saveSettings();

    if (catTx && calMode)
        setCatPtt(false);

    if (settings2.catOn)
        serviceCat();
#if DEBUG_SERIAL
    // Serial is quiet unless asked: the status line runs out through J4 and the USB cable and is
    // heard as ticking on 15 / 10 m. '?' = one status line, 'D' = continuous lines on / off.
    else
    {
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '?')
            printStatus();
        else if (c == 'D' || c == 'd')
            debugStream = !debugStream;
        else if (c == 'M' || c == 'm')
        {
            clk2PinOff = !clk2PinOff;
            if (clk2On)
                si5351.output_enable(SI5351_CLK2, clk2PinOff ? 0 : 1);
            Serial.println(clk2PinOff ? F("CLK2 pin OFF") : F("CLK2 pin ON"));
        }
    }
    if (debugStream && millis() - lastDebug >= DEBUG_PERIOD_MS)
    {
        lastDebug = millis();
        printStatus();
    }
    }
#endif
}

void printStatus()
{
    Serial.print(F("band="));
    Serial.print(band[currentBand].name);
    Serial.print(F(" A3="));
    Serial.print(bandRaw);
    Serial.print(F(" A0mV="));
    Serial.print(smeterRefMv);
    Serial.print(F(" A1mV="));
    Serial.print(smeterMv);
    Serial.print(F(" sig="));
    Serial.print(smeterDiffMv);
    Serial.print(F(" dB="));
    Serial.print(smeterDb(smeterShownMv));
    Serial.print(F(" vfo="));
    Serial.print(vfo);
    Serial.print(F(" rit="));
    Serial.print(rit ? rit_offset : 0);
    Serial.print(F(" tx="));
    Serial.print(txon);
    Serial.print(F(" i2cTimeouts="));
    Serial.print(i2cTimeouts);
    Serial.print(F(" A7="));
    Serial.print(s3Raw);
    Serial.print(cwMode ? F(" CW") : (calMode ? F(" CAL") : F("")));
    Serial.print(F(" A6="));
    Serial.print(readA6());
    Serial.print(modeOpp ? F(" OPP") : F(" NORM"));
    Serial.print(agcFast ? F(" AGC=F") : F(" AGC=S"));
    if (attOn) Serial.print(F(" ATT"));
    if (preOn) Serial.print(F(" PRE"));
    Serial.print(F(" loDiv="));
    Serial.print(loDivider);
    Serial.print(F(" score="));
    Serial.print(spurScore((uint64_t)((band[currentBand].ifPositive ? vfo + bfo / 100 : vfo - bfo / 100)) * 100ULL, loDivider));
    Serial.print(F(" bfoDiv="));
    Serial.println(lastBfoDivider);
}

void clickEvent()
{
    if (!rit)
        vfosteps();
    else
    {
        //steps for RIT
        switch (ritstep)
        {
            case 1:
                ritstep = 100;
                break;
            case 100:
                ritstep = 2500;
                break;
            default:
                ritstep = 1;
                break;
        }
        display_rit();
    }
}

void doubleClickEvent()
{
    rit = !rit;
    changed_f = true;                // re-program CLK0 with / without the RIT offset
}

void longHoldEvent()
{
    saveSettings();
}
