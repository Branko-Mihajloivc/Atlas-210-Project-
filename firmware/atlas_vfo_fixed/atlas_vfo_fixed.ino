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

#include <Rotary.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <avr/eeprom.h>

#define DEBUG_SERIAL 1                      // 0 = no serial output

// Layout must stay identical to the 2023 sketch so the stored EEPROM data is still valid
struct settings_t
{
    int32_t bfo_offset;                     // BFO in 0.01 Hz (552000000 = 5.520000 MHz)
    uint32_t vfo;                           // last frequency in Hz
    bool superhet;
    int fcorrection;                        // Si5351 correction in units of 10 ppb (int is only 16 bit)
}
settings;

// S meter glyphs: both lines = full step, left line only = half step
byte bar5[8] = {B00000, B11011, B11011, B11011, B11011, B11011, B11011, B00000};
byte barHalf[8] = {B00000, B11000, B11000, B11000, B11000, B11000, B11000, B00000};
// Step marker glyph (setup menu)
byte bar1[8] = {B10000, B11000, B11100, B11110, B11110, B11100, B11000, B10000};

#define BAND_SELECT_IN A3                   // 5 way rotary switch for band select (resistor ladder)
#define ENCODER_A    3                      // Encoder pin A
#define ENCODER_B    2                      // Encoder pin B
#define ENCODER_BTN  6                      // Step button
#define TX_BTN       A2                     // LOW = TX
#define SMETER_IN    A1                     // PC-100 pin 15: S meter amp output (rises with signal)
#define SMETER_REF_IN A0                    // PC-100 pin 16: S meter bridge reference (ZERO pot R222)
// Both through 10k in series + 100nF to GND at the radio end; 1k between pin 15 and 16 replaces the
// removed meter. Never connect to the meter / RL101 side (+13 V on TX).

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
Rotary r = Rotary(ENCODER_A, ENCODER_B);

volatile int8_t encoderSteps = 0;           // written by the encoder ISR, taken by loop()

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

uint16_t bandRaw = 0;
uint16_t smeterMv = 0;                      // pin 15
uint16_t smeterRefMv = 0;                   // pin 16
int16_t smeterDiffMv = 0;                   // pin 15 - pin 16, as measured
int16_t smeterShownMv = 0;                  // with meter ballistics (fast attack, slow decay)
int8_t smeterLastBars = -1;                 // what is on the LCD now; -1 = redraw
int8_t smeterLastS = -1;
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
const uint8_t NUM_BANDS = sizeof band / sizeof band[0];

// Band switch thresholds on A3 (ladder gives about 0 / 255 / 512 / 768 / 1023)
const uint16_t bandThreshold[NUM_BANDS - 1] = {180, 400, 650, 900};

uint8_t currentBand = 0;
uint8_t bandCandidate = 0;
uint8_t bandStableCount = 0;

/**************************************/
/* Button multi-click handling        */
/**************************************/
int checkButton()
{
    int event = 0;
    // Read the state of the button
    buttonVal = digitalRead(ENCODER_BTN);
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
ISR(PCINT2_vect) {
    unsigned char result = r.process();
    if (result == DIR_CW && encoderSteps < 100)
        encoderSteps++;
    else if (result == DIR_CCW && encoderSteps > -100)
        encoderSteps--;
}

int8_t takeEncoderSteps()
{
    noInterrupts();
    int8_t s = encoderSteps;
    encoderSteps = 0;
    interrupts();
    return s;
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
        int32_t f = (int32_t)vfo + (int32_t)steps * (int32_t)radix;
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
void updateSi5351()
{
    int32_t rf = (int32_t)vfo;
    if (rit && !txon)
        rf += rit_offset;

    uint64_t clk0 = (uint64_t)rf * SI5351_FREQ_MULT;
    if (band[currentBand].ifPositive)
        clk0 += bfo;
    else
        clk0 -= bfo;

    si5351.set_freq(clk0, SI5351_CLK0);
    si5351.set_freq(bfo, SI5351_CLK1);
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

void display_smeter(int sigMv)
{
    // Line 2: "S9>" + columns 3-11 = S1..S9 (two lines per column, one line = 3 dB)
    //         + columns 12-15 = over S9 as text: "+", "+10", "+20", "+30", "+40"
    int db = constrain(smeterDb(sigMv), 0, 94);
    int halves = (db < 54 ? db : 54) / 3;   // 0..18
    int sunits = (db >= 54) ? 9 : db / 6;
    int over = (db > 54) ? db - 54 : 0;     // dB over S9
    int overShown = (over == 0) ? -1 : (over / 10) * 10;   // -1 = nothing, 0 = "+", 10.. = "+10"..
    // Only write the LCD when something visible changes: every LCD write is a burst of
    // digital edges on the data lines that can be heard in the receiver.
    if (halves == smeterLastBars && sunits == smeterLastS && overShown == smeterLastOver)
        return;
    smeterLastBars = halves;
    smeterLastS = sunits;
    smeterLastOver = overShown;
    lcd.setCursor(0, 1);
    lcd.print('S');
    lcd.print(sunits);
    lcd.print('>');
    for (int i = 0; i < 9; i++)
    {
        int h = halves - 2 * i;
        if (h >= 2)
            lcd.write(byte(0));             // both lines
        else if (h == 1)
            lcd.write(byte(2));             // left line only
        else
            lcd.print(' ');
    }
    if (overShown < 0)
        lcd.print(F("    "));
    else if (overShown == 0)
        lcd.print(F("+   "));
    else
    {
        lcd.print('+');
        lcd.print(overShown);
        lcd.print(' ');
    }
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
    uint32_t f = vfo;
    lcd.setCursor(0, 0);
    lcd.print(F("VFO:"));
    uint16_t mhz = f / 1000000UL;
    if (mhz < 10)
        lcd.print(' ');
    lcd.print(mhz);
    lcd.print('.');
    print3((f / 1000UL) % 1000);
    lcd.print('.');
    print3(f % 1000UL);
    lcd.print(F("  "));                     // clear columns 14-15
    show_i2c_flag();
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
/* Setup menu (button held at power on)*/
/*  encoder      : change value       */
/*  click        : change step        */
/*  double click : next item          */
/*  hold 2 s     : save and exit      */
/**************************************/
void runSetupMenu()
{
    setupmenu = true;
    lcd.clear();
    lcd.print(F("VFO SETUP MODE"));
    lcd.setCursor(0, 1);
    lcd.print(F("release button"));
    while (!digitalRead(ENCODER_BTN))
        ;
    delay(500);
    buttonLast = HIGH;
    upTime = millis();

    int32_t bfoHz = bfo / 100;
    int32_t corr = (int32_t)settings.fcorrection * 10L;   // ppb
    uint8_t item = 0;
    bool redraw = true;
    radix = 100;

    while (true)
    {
        int8_t s = takeEncoderSteps();
        if (s)
        {
            if (item == 0)
            {
                bfoHz = constrain(bfoHz + (int32_t)s * (int32_t)radix, 1000000L, 20000000L);
                bfo = (uint32_t)bfoHz * 100UL;
            }
            else
            {
                corr = constrain(corr + (int32_t)s * (int32_t)radix, -300000L, 300000L);
                si5351.set_correction(corr, SI5351_PLL_INPUT_XO);
                si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
            }
            updateSi5351();                 // hear the change live
            redraw = true;
        }

        int b = checkButton();
        if (b == 1) { vfosteps(); redraw = true; }
        if (b == 2) { item = (item + 1) % 2; redraw = true; }
        if (b == 3) break;

        if (redraw)
        {
            redraw = false;
            lcd.clear();
            if (item == 0)
            {
                lcd.print(F("BFO:"));
                lcd.print(bfoHz);
                lcd.print(F("Hz"));
            }
            else
            {
                lcd.print(F("CORR:"));
                lcd.print(corr);
                lcd.print(F("ppb"));
            }
            lcd.setCursor(0, 1);
            lcd.write(byte(1));
            lcd.print(F("STEP:"));
            lcd.print(radix);
        }
    }

    settings.bfo_offset = (int32_t)bfo;
    settings.fcorrection = (int)(corr / 10L);
    saveSettings();
    lcd.clear();
    lcd.print(F("SAVED"));
    delay(1000);
    lcd.clear();
    radix = 100;
    setupmenu = false;
}

void setup()
{
    Serial.begin(19200);
    pinMode(BAND_SELECT_IN, INPUT);
    pinMode(ENCODER_BTN, INPUT_PULLUP);
    pinMode(TX_BTN, INPUT_PULLUP);

    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);
    lcd.createChar(0, bar5);
    lcd.createChar(1, bar1);
    lcd.createChar(2, barHalf);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("YU5KBM ATLAS 210"));
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
    si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA); //you can set this to 2MA, 4MA, 6MA or 8MA
    si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA); //be careful though - measure into 50ohms
                                                          //(8 mA tested 2026-09-28: no change in audio/AGC level)

    PCICR |= (1 << PCIE2);           // Enable pin change interrupt for the encoder
    PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
    sei();

    if (!digitalRead(ENCODER_BTN))
        runSetupMenu();

    txon = !digitalRead(TX_BTN);
    lcd.clear();
    smeterLastBars = -1;             // force the S meter line to be drawn
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
    tune(takeEncoderSteps());

    // I2C timeout happened: program the Si5351 again
    if (Wire.getWireTimeoutFlag())
    {
        Wire.clearWireTimeoutFlag();
        i2cTimeouts++;
        changed_f = true;
    }

    if (changed_f)
    {
        changed_f = false;
        updateSi5351();
        show_main();
    }

    updateSmeter();

    int b = checkButton();
    if (b == 1) clickEvent();
    if (b == 2) doubleClickEvent();
    if (b == 4) longHoldEvent();

    // save to eeprom SAVE_DELAY_MS after the last frequency change
    if (vfo != settings.vfo && millis() - lastTuneMillis > SAVE_DELAY_MS)
        saveSettings();

#if DEBUG_SERIAL
    if (millis() - lastDebug >= DEBUG_PERIOD_MS)
    {
        lastDebug = millis();
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
        Serial.println(i2cTimeouts);
    }
#endif
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
