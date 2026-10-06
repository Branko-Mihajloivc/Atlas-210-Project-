/*Copyright (c)2019, Dennis Zabawa (KG4RUL)

  Permission is hereby granted, free of charge, by  to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*************************************************************************
  Control and status display for a SOTABEAMS VariBeam ("VB") SSB/CW DSP filter,
  installed in an Atlas 210X (YU5KBM).

  The front panel encoder is read by this Nano, which emulates encoder steps and
  button presses to the VB and shows the filter on a 128x64 SH1106 OLED.

  2026-09 rework:
  - Emulated gray code states are held EMULATION_STATE_MS (before: microseconds,
    so the VB missed steps and drifted away from the displayed values).
  - Encoder detents are counted, fast turning no longer loses steps.
  - Official Adafruit SH110X library, no OLED reset pin (the old OLED_RESET was
    pin 4 = encoder switch pin).
  - Resync at power on: the VB is driven to its minimum (known state), then
    stepped up to the saved values. Hold the button RESYNC_HOLD_MS to resync
    at any time.
  - Last CF / BW / mode saved in EEPROM and restored at power on.
  - Display shows passband edges and a trapezoid passband graph.
  - Screen after BLANK_AFTER_MS without use: OFF, DIM or ALWAYS ON (OLEDs make RF
    noise even when nothing changes). Turn the knob or press the button to wake it.

  Button: click = CF / BW mode, double click = next preset (SSB / SSB NARROW / CW /
  DIGI), triple click = screen mode (OFF / DIM / ALWAYS ON, saved), hold 2 s = resync.

  VB filter (from the SOTABEAMS notes):
  Center frequency 200-3500 Hz: steps 25 Hz up to 2000, 50 Hz above.
  Bandwidth 200-3500 Hz: steps 20 Hz up to 400, 50 Hz up to 700, 100 Hz above.
  Passband edges are CF -/+ BW/2, limited to 200-3500 Hz.
  One step = one cycle through all 4 gray code states. At power on the VB is at
  CF 1500, BW 2400, mode CF. Each VB button press toggles CF / BW mode.
*/
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <EEPROM.h>

/******************** Settings ****************/
#define EMULATION_STATE_MS 2      /*ms each emulated gray code state is held*/
#define CLICK_MS 60               /*length of an emulated button press*/
#define RESYNC_HOLD_MS 2000       /*hold the button this long to resync*/
#define BLANK_AFTER_MS 10000UL    /*screen OFF / DIM after this long without use*/
#define DOUBLE_CLICK_MS 350       /*second press within this time = double click*/
#define CONTRAST_NORMAL 0x80      /*OLED contrast in use*/
#define CONTRAST_DIM 0x00         /*OLED contrast in DIM mode*/
#define SKIRT_PX 6                /*width of the sloped filter skirts in the graph*/

/*Presets (double click cycles through them). Values must be VB step values:
  CF 25 Hz steps up to 2000 / 50 Hz above; BW 20 Hz up to 400, 50 Hz up to 700, 100 Hz above*/
struct Preset { const char *name; int cf; int bw; };
const Preset presets[] = {
  {"SSB",        1500, 2400},
  {"SSB NARROW", 1400, 1800},
  {"CW",          700,  500},
  {"DIGI",       1500, 3000},
};
const uint8_t NUM_PRESETS = sizeof(presets) / sizeof(presets[0]);
uint8_t presetIndex = NUM_PRESETS - 1;   /*first double click selects presets[0]*/
#define SAVE_AFTER_MS 5000UL      /*save to EEPROM this long after the last change*/
#define EEPROM_MAGIC 0x5B         /*marks valid saved settings*/

/******************** Pins ****************/
#define VBencoderPinA 2           /*front panel encoder A (interrupt)*/
#define VBencoderPinB 3           /*front panel encoder B (interrupt)*/
#define VBencoderSwPin 4          /*front panel encoder switch*/
#define emulatedEncoderPinA 5     /*to VB encoder input A*/
#define emulatedEncoderPinB 6     /*to VB encoder input B*/
#define emulatedEncoderSwPin 7    /*to VB button input*/

/*The library assumes a column offset of 2 (SH1106 with 132 columns of RAM). This module
  wraps the two rightmost pixels around to the left edge with that offset, so it needs 0.
  If the left edge of the picture is ever cut off by 2 pixels, set this back to 2.*/
#define OLED_COLUMN_OFFSET 0
class AtlasOLED : public Adafruit_SH1106G
{
public:
  AtlasOLED() : Adafruit_SH1106G(128, 64, &Wire, -1) {}
  void setColumnOffset(uint8_t o) { _page_start_offset = o; }
};
AtlasOLED display;

enum VBencoderRotationDirs { CW, CCW };
enum modes { CF, BW };
enum screenModes { SCREEN_OFF, SCREEN_DIM, SCREEN_ON };   /*after BLANK_AFTER_MS idle*/
screenModes screenMode = SCREEN_OFF;

/******************** Encoder ISR state ****************/
volatile boolean VBencoderAFlag = false;
volatile boolean VBencoderBFlag = false;
volatile int8_t VBpendingSteps = 0;   /*+CW / -CCW detents not yet sent to the VB*/

/******************** Filter state ****************/
int dspCurrentBW = 2400;
int dspCurrentCenterFreq = 1500;
modes currentMode = CF;

unsigned long lastActivity = 0;
unsigned long lastChange = 0;
bool displayOn = true;
bool unsavedChange = false;

/***********************************************************/
/* Emulate one VB encoder step (four gray code states)     */
/***********************************************************/
void vbStepEmulation(VBencoderRotationDirs encoderDir)
{
  if (encoderDir == CW)
  {
    digitalWrite(emulatedEncoderPinA, HIGH);
    delay(EMULATION_STATE_MS);
    digitalWrite(emulatedEncoderPinB, LOW);
    delay(EMULATION_STATE_MS);
    digitalWrite(emulatedEncoderPinA, LOW);
    delay(EMULATION_STATE_MS);
    digitalWrite(emulatedEncoderPinB, HIGH);
    delay(EMULATION_STATE_MS);
  }
  else
  {
    digitalWrite(emulatedEncoderPinB, HIGH);
    delay(EMULATION_STATE_MS);
    digitalWrite(emulatedEncoderPinA, LOW);
    delay(EMULATION_STATE_MS);
    digitalWrite(emulatedEncoderPinB, LOW);
    delay(EMULATION_STATE_MS);
    digitalWrite(emulatedEncoderPinA, HIGH);
    delay(EMULATION_STATE_MS);
  }
}

/* Emulate one VB button press: toggles CF / BW mode on the VB */
void vbClickEmulation()
{
  digitalWrite(emulatedEncoderSwPin, LOW);
  delay(CLICK_MS);
  digitalWrite(emulatedEncoderSwPin, HIGH);
  delay(CLICK_MS);
  currentMode = (currentMode == CF) ? BW : CF;
}

/***********************************************************/
/* Front panel encoder ISRs - count detents                */
/***********************************************************/
void serviceVBencoderPinA()
{
  byte reading = PIND & B00001100;           /*pin A (D2) and pin B (D3)*/
  if (reading == B00001100 && VBencoderAFlag)
  {
    VBencoderBFlag = false;
    VBencoderAFlag = false;
    if (VBpendingSteps < 100) VBpendingSteps++;
  }
  else if (reading == B00000100)
  {
    VBencoderBFlag = true;
  }
}

void serviceVBencoderPinB()
{
  byte reading = PIND & B00001100;
  if (reading == B00001100 && VBencoderBFlag)
  {
    VBencoderBFlag = false;
    VBencoderAFlag = false;
    if (VBpendingSteps > -100) VBpendingSteps--;
  }
  else if (reading == B00001000)
  {
    VBencoderAFlag = true;
  }
}

int8_t takePendingSteps()
{
  noInterrupts();
  int8_t s = VBpendingSteps;
  VBpendingSteps = 0;
  interrupts();
  return s;
}

/***********************************************************/
/* Track the VB values - same step sizes as the VB         */
/***********************************************************/
void updateDspBW(VBencoderRotationDirs dir)
{
  if (dir == CCW)
  {
    if (dspCurrentBW > 700) dspCurrentBW -= 100;
    else if (dspCurrentBW > 400) dspCurrentBW -= 50;
    else if (dspCurrentBW > 200) dspCurrentBW -= 20;
  }
  else
  {
    if (dspCurrentBW < 400) dspCurrentBW += 20;
    else if (dspCurrentBW < 700) dspCurrentBW += 50;
    else if (dspCurrentBW < 3500) dspCurrentBW += 100;
  }
}

void updateDspCenterFreq(VBencoderRotationDirs dir)
{
  if (dir == CCW)
  {
    if (dspCurrentCenterFreq > 2000) dspCurrentCenterFreq -= 50;
    else if (dspCurrentCenterFreq > 200) dspCurrentCenterFreq -= 25;
  }
  else
  {
    if (dspCurrentCenterFreq < 2000) dspCurrentCenterFreq += 25;
    else if (dspCurrentCenterFreq < 3500) dspCurrentCenterFreq += 50;
  }
}

/* One step in the current mode: VB and tracked value */
void stepVB(VBencoderRotationDirs dir)
{
  if (currentMode == CF)
    updateDspCenterFreq(dir);
  else
    updateDspBW(dir);
  vbStepEmulation(dir);
}

/***********************************************************/
/* EEPROM                                                  */
/***********************************************************/
void saveSettings()
{
  EEPROM.update(0, EEPROM_MAGIC);
  EEPROM.put(1, dspCurrentCenterFreq);
  EEPROM.put(3, dspCurrentBW);
  EEPROM.update(5, (uint8_t)currentMode);
  EEPROM.update(6, (uint8_t)screenMode);
  unsavedChange = false;
}

void loadScreenMode()
{
  uint8_t s = EEPROM.read(6);
  screenMode = (EEPROM.read(0) == EEPROM_MAGIC && s <= SCREEN_ON) ? (screenModes)s : SCREEN_OFF;
}

/* Saved target values; defaults (VB power on values) if nothing valid is stored */
void loadTargets(int &cf, int &bw, modes &mode)
{
  cf = 1500;
  bw = 2400;
  mode = CF;
  if (EEPROM.read(0) != EEPROM_MAGIC)
    return;
  int c, b;
  EEPROM.get(1, c);
  EEPROM.get(3, b);
  uint8_t m = EEPROM.read(5);
  if (c >= 200 && c <= 3500 && b >= 200 && b <= 3500 && m <= 1)
  {
    cf = c;
    bw = b;
    mode = (modes)m;
  }
}

/***********************************************************/
/* Display                                                 */
/***********************************************************/
void passbandEdges(int &lo, int &hi)
{
  lo = dspCurrentCenterFreq - dspCurrentBW / 2;
  hi = dspCurrentCenterFreq + dspCurrentBW / 2;
  if (lo < 200) lo = 200;
  if (hi > 3500) hi = 3500;
}

int freqToX(int f)                     /*0..3500 Hz -> x 0..127*/
{
  return (int)((long)f * 127L / 3500L);
}

void displayStatus()
{
  int lo, hi;
  passbandEdges(lo, hi);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(currentMode == CF ? '>' : ' ');
  display.print(F("CF "));
  display.print(dspCurrentCenterFreq);
  display.setCursor(0, 17);
  display.print(currentMode == BW ? '>' : ' ');
  display.print(F("BW "));
  display.print(dspCurrentBW);

  /*passband edges, centered*/
  display.setTextSize(1);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d-%d Hz", lo, hi);
  display.setCursor((128 - 6 * (int)strlen(buf)) / 2, 36);
  display.print(buf);

  /*passband graph: trapezoid like a real filter curve (flat top lo..hi, sloped
    skirts), 0-3500 Hz scale, ticks every 500 Hz, labels at 1k 2k 3k*/
  int xl = freqToX(lo), xh = freqToX(hi);
  int bl = max(xl - SKIRT_PX, 0), bh = min(xh + SKIRT_PX, 127);
  const int yTop = 45, yBot = 53;
  display.fillRect(xl, yTop, xh - xl + 1, yBot - yTop + 1, SH110X_WHITE);
  display.fillTriangle(xl, yTop, xl, yBot, bl, yBot, SH110X_WHITE);
  display.fillTriangle(xh, yTop, xh, yBot, bh, yBot, SH110X_WHITE);
  display.drawFastHLine(0, 54, 128, SH110X_WHITE);
  for (int f = 500; f <= 3000; f += 500)    /*no ticks at the screen edges (0 / 3500 Hz)*/
    display.drawFastVLine(freqToX(f), 54, (f % 1000 == 0) ? 3 : 2, SH110X_WHITE);
  display.setCursor(freqToX(1000) - 5, 57);
  display.print(F("1k"));
  display.setCursor(freqToX(2000) - 5, 57);
  display.print(F("2k"));
  display.setCursor(freqToX(3000) - 5, 57);
  display.print(F("3k"));

  display.display();
}

void wakeDisplay()
{
  lastActivity = millis();
  if (!displayOn)
  {
    display.setContrast(CONTRAST_NORMAL);
    display.oled_command(SH110X_DISPLAYON);
    displayOn = true;
  }
}

/* Called when idle for BLANK_AFTER_MS: OFF, DIM or leave ON, per screenMode */
void idleDisplay()
{
  if (screenMode == SCREEN_OFF)
    display.oled_command(SH110X_DISPLAYOFF);
  else if (screenMode == SCREEN_DIM)
    display.setContrast(CONTRAST_DIM);
  if (screenMode != SCREEN_ON)
    displayOn = false;                 /*next activity restores it*/
}

/* Step the VB (in the current mode) until the tracked value equals target */
void stepValueTo(int target)
{
  for (int i = 0; i < 200; i++)
  {
    int v = (currentMode == CF) ? dspCurrentCenterFreq : dspCurrentBW;
    if (v == target)
      break;
    stepVB(v < target ? CW : CCW);
  }
}

/* Double click: next preset - set CF and BW, end in CF mode */
void nextPreset()
{
  presetIndex = (presetIndex + 1) % NUM_PRESETS;
  const Preset &p = presets[presetIndex];

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - 12 * (int)strlen(p.name)) / 2, 20);
  display.print(p.name);
  display.display();

  if (currentMode != CF)
    vbClickEmulation();
  stepValueTo(p.cf);
  vbClickEmulation();                  /*CF -> BW*/
  stepValueTo(p.bw);
  vbClickEmulation();                  /*BW -> CF*/
  takePendingSteps();

  delay(500);
  unsavedChange = true;
  lastChange = millis();
  displayStatus();
}

/* Triple click: cycle OFF -> DIM -> ALWAYS ON, show it for a moment, save */
void cycleScreenMode()
{
  screenMode = (screenMode == SCREEN_OFF) ? SCREEN_DIM : (screenMode == SCREEN_DIM) ? SCREEN_ON : SCREEN_OFF;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(22, 10);
  display.print(F("SCREEN WHEN IDLE"));
  display.setTextSize(2);
  display.setCursor(screenMode == SCREEN_ON ? 4 : 40, 30);
  display.print(screenMode == SCREEN_OFF ? F("OFF") : screenMode == SCREEN_DIM ? F("DIM") : F("ALWAYS ON"));
  display.display();
  delay(1200);
  saveSettings();
  displayStatus();
}

void splashScreen()
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(4, 4);
  display.print(F("ATLAS 210X"));
  display.setTextSize(1);
  display.setCursor(22, 26);
  display.print(F("VariBeam  DSP"));
  display.setCursor(28, 38);
  display.print(F("YU5KBM-YT3CT"));
  display.display();
}

/* Progress bar at the bottom of the splash / resync screen, 0-100 % */
void showProgress(const __FlashStringHelper *text, int percent)
{
  display.fillRect(0, 48, 128, 16, SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print(text);
  display.drawRect(0, 58, 128, 6, SH110X_WHITE);
  display.fillRect(1, 59, (long)126 * percent / 100, 4, SH110X_WHITE);
  display.display();
}

/***********************************************************/
/* Resync: drive the VB to its minimum (known state), then */
/* step up to the target values. Assumes the VB is in CF   */
/* mode when called (true after VB power on and after any  */
/* resync, because currentMode tracks every emulated click)*/
/***********************************************************/
void resyncVB(int targetCF, int targetBW, modes targetMode)
{
  /*get the VB into CF mode*/
  if (currentMode != CF)
    vbClickEmulation();

  /*CF: 3500 -> 200 needs 102 steps, send 110 to be sure*/
  showProgress(F("sync CF"), 0);
  for (int i = 0; i < 110; i++)
    vbStepEmulation(CCW);
  dspCurrentCenterFreq = 200;
  for (int i = 0; i < 200 && dspCurrentCenterFreq < targetCF; i++)
    stepVB(CW);
  showProgress(F("sync BW"), 50);

  /*BW: 3500 -> 200 needs 44 steps, send 50*/
  vbClickEmulation();                  /*CF -> BW mode*/
  for (int i = 0; i < 50; i++)
    vbStepEmulation(CCW);
  dspCurrentBW = 200;
  for (int i = 0; i < 100 && dspCurrentBW < targetBW; i++)
    stepVB(CW);

  if (targetMode == CF)
    vbClickEmulation();                /*back to CF mode*/
  showProgress(F("ready"), 100);

  takePendingSteps();                  /*ignore knob movement during the resync*/
  unsavedChange = false;
}

/***********************************************************/
/* Front panel button: click = CF/BW, double click =       */
/* screen mode, hold 2 s = resync                          */
/***********************************************************/
void checkButton()
{
  static bool pressed = false;
  static bool longDone = false;
  static unsigned long downTime = 0;
  static uint8_t clicks = 0;             /*short presses in the current sequence*/
  static unsigned long clickTime = 0;

  /*a click sequence ends when no further press follows within DOUBLE_CLICK_MS*/
  if (clicks > 0 && !pressed && millis() - clickTime > DOUBLE_CLICK_MS)
  {
    uint8_t n = clicks;
    clicks = 0;
    if (n == 1)                          /*click: CF / BW*/
    {
      vbClickEmulation();
      unsavedChange = true;
      lastChange = millis();
      displayStatus();
    }
    else if (n == 2)                     /*double click: next preset*/
      nextPreset();
    else                                 /*triple click: screen mode*/
      cycleScreenMode();
    wakeDisplay();
  }

  bool isDown = (digitalRead(VBencoderSwPin) == LOW);
  if (isDown && !pressed)
  {
    delay(20);                           /*debounce*/
    if (digitalRead(VBencoderSwPin) != LOW)
      return;
    pressed = true;
    longDone = false;
    downTime = millis();
    wakeDisplay();
  }
  else if (isDown && pressed && !longDone && millis() - downTime >= RESYNC_HOLD_MS)
  {
    longDone = true;
    clicks = 0;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(16, 16);
    display.print(F("RESYNC"));
    display.display();
    modes keepMode = currentMode;
    resyncVB(dspCurrentCenterFreq, dspCurrentBW, keepMode);
    saveSettings();
    displayStatus();
  }
  else if (!isDown && pressed)
  {
    delay(20);                           /*debounce*/
    pressed = false;
    if (!longDone)                       /*short press: count it, act when the sequence ends*/
    {
      if (clicks < 3)
        clicks++;
      clickTime = millis();
    }
  }
}

/***********************************************************/
void checkVbEncoder()
{
  int8_t steps = takePendingSteps();
  if (steps == 0)
    return;
  wakeDisplay();
  while (steps != 0)
  {
    stepVB(steps > 0 ? CW : CCW);
    steps += (steps > 0) ? -1 : 1;
  }
  unsavedChange = true;
  lastChange = millis();
  displayStatus();
}

/***********************************************************/
void setup()
{
  digitalWrite(emulatedEncoderPinA, LOW);   /*emulated encoder rest state*/
  digitalWrite(emulatedEncoderPinB, HIGH);
  digitalWrite(emulatedEncoderSwPin, HIGH);
  pinMode(emulatedEncoderPinA, OUTPUT);
  pinMode(emulatedEncoderPinB, OUTPUT);
  pinMode(emulatedEncoderSwPin, OUTPUT);
  pinMode(VBencoderSwPin, INPUT_PULLUP);
  pinMode(VBencoderPinA, INPUT_PULLUP);
  pinMode(VBencoderPinB, INPUT_PULLUP);

  /*Diagnostics on the serial port (115200): which I2C devices answer*/
  Serial.begin(115200);
  Serial.println(F("VariBeam control start, I2C scan:"));
  Wire.begin();
  Wire.setWireTimeout(3000, true);
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++)
  {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0)
    {
      Serial.print(F("  device at 0x"));
      Serial.println(a, HEX);
      found++;
    }
  }
  Serial.println(found ? F("scan done") : F("  NO I2C DEVICE FOUND - check OLED / wiring"));

  display.begin(0x3C, true);                /*I2C address 0x3C, 128x64*/
  display.setColumnOffset(OLED_COLUMN_OFFSET);
  display.setContrast(CONTRAST_NORMAL);
  display.setTextColor(SH110X_WHITE);
  loadScreenMode();
  splashScreen();

  delay(500);                               /*let the VB finish its own start up*/
  int cf, bw;
  modes mode;
  loadTargets(cf, bw, mode);
  currentMode = CF;                         /*VB is in CF mode after power on*/
  resyncVB(cf, bw, mode);
  delay(400);

  attachInterrupt(digitalPinToInterrupt(VBencoderPinA), serviceVBencoderPinA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VBencoderPinB), serviceVBencoderPinB, CHANGE);
  takePendingSteps();

  displayStatus();
  lastActivity = millis();
}

/***********************************************************/
void loop()
{
  checkVbEncoder();
  checkButton();

  if (unsavedChange && millis() - lastChange >= SAVE_AFTER_MS)
    saveSettings();

  if (displayOn && screenMode != SCREEN_ON && millis() - lastActivity >= BLANK_AFTER_MS)
    idleDisplay();
}
