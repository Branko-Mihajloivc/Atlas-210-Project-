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
*************************************************************************
  This is a sketch to control and monitor the status of a SOTABEAMS Variable
  Bandwidth Filter Module for SSB/CW "VariBeam" referred to here as "VB"

  https://www.sotabeams.co.uk/

  A single detented rotary encoder continuously varies the bandwidth or the
  centre frequency of the filter. All settings produce filters with flat
  passbands and steep skirts. suitable for digital modes and APRS, low pass
  filters or CW filters.

  This sketch tracks the encoder rotation with a pair of interrupt service
  routines, emulates these rotations to the VB and displays the status on a
  local display.  The encoder pushbutton status is polled.  See the code below
  for information on how the bandwidth and center frequency steps are
  determined.

  This sketch utilizes direct port addressing to read the digital pins of the nano

  PORTD maps to Arduino digital pins 0 to 7
    PIND - The Port D Input Pins Register - read only
*/
/******************** Local Display Parameters ****************/
#include <Adafruit_GFX.h> /*setup Interface to OLED display*/
#include <Adafruit_SH1106.h>
#define OLED_RESET 4
Adafruit_SH1106 display(OLED_RESET);
#if (SH1106_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SH1106.h!");
#endif
/******************* Hardware Encoder Parameters ***************/
enum VBencoderRotationDirs /*Possible rotation directions for VB encoder*/
{
  CW, CCW /*ClockWise, CounterClockWise*/
};
volatile VBencoderRotationDirs VBencoderDir; /*Last rotation direction of
  VB encoder*/
#define VBencoderPinA 2 /*VB encoder interrupt pin A is digital pin 2*/
#define VBencoderPinB 3 /*VB encoder interrupt pin B is digital pin 3*/
#define VBencoderSwPin 4 /*VB encoder switch pin is digital pin 4*/
volatile boolean VBencoderSwPressed = false; /*Toggles when VB switch
  pressed/released*/
#define VBencoderBitMask B00001100 /*Isolate pin A and Pin B VB encoder 
  port bits*/
volatile boolean VBencoderAFlag = false; /*If true expecting a rising edge
  on pinA to signal that VB encoder has arrived at a detent*/
volatile boolean VBencoderBFlag = false; /*If true expecting a rising edge
  on pinB to signal that VB encoder has arrived at a detent (opposite
  direction to when aFlag is set)*/
volatile byte VBencoderReading = 0; /*Value read from VB encoder interrupt
  pins*/
volatile boolean VBencoderChanged = false; /*True when VB encoder
  has rotated*/
/******************* Emulated Encoder Parameters ****************/
#define emulatedEncoderPinA 5 /*Emulated encoder pin A is digital pin 5*/
#define emulatedEncoderPinB 6 /*Emulated encoder pin B is digital pin 6*/
#define emulatedEncoderSwPin 7 /*Emulated encoder switch pin is digital pin 7*/
/***************** DSP Filter Parameters ************************/
/*Default Settings:
  Center Freq = 1500Hz
  bandwidth = 2400Hz
  Mode = Center

  The step size varies based on the current values.

  Center frequency / Hz:
  200-2000: step size 25
  2000-3500: step size 50

  Bandwidth / Hz:
  200-400: step size 20
  400-700: step size 50
  700-3500: step size 100

  So for example, the available values for center
  frequency are: 200, 225, 250 ... 1950, 1975, 2000,
  2050, 2100 ... 3400, 3500

  One step = one cycle through all 4 gray code states
  of the two encoder pins RC6+RC7.

  Both center frequency and bandwidth are limited to
  200-3500 Hz.

  The filter passband edges are also limited to
  200-3500 Hz. The upper and lower edges are calculated
  based on center frequency and bandwidth, then limited
  to 200-3500 Hz, before being used to generate the
  filter coefficients. So:

  - Center = 200 and bandwidth = 200 would result in a
  passband of 200-300Hz.

  - Center = 300 and bandwidth = 200 would result in a
  passband of 200-400Hz.

  There is no way of setting upper and lower edges
  directly, they have to be controlled via center
  frequency and bandwidth.
*/
volatile int dspCurrentBW = 2400; /*Default VB bandwidth on powerup
  - change if you use the on-board VB button to change this value*/
volatile int dspCurrentCenterFreq = 1500; /*Default VB center
  frequency on powerup - change if you use the on-board VB button to
  change this value*/
enum modes /*Possible modes for VB setting: CF = center frequency;
  BW = bandwidth*/
{
  CF, BW
};
String modeLabels[2] /*Strings to display VB setting mode on
  local display*/
{
  "CF", "BW"
};
volatile modes currentMode = CF; /*VB defaults to center frequency mode on
  powerup*/

/***********************************************************/
void vbStepEmulation(VBencoderRotationDirs encoderDir) /*Emulates
  four Gray Code state changes to the VB necessary for one step
  of either the CF or BW value*/
{
  if (encoderDir == CW) /*Which direction will we emulate*/
  {
    digitalWrite(emulatedEncoderPinA, HIGH);
    digitalWrite(emulatedEncoderPinB, LOW);
    digitalWrite(emulatedEncoderPinA, LOW);
    digitalWrite(emulatedEncoderPinB, HIGH);
  }
  else
  {
    digitalWrite(emulatedEncoderPinB, HIGH);
    digitalWrite(emulatedEncoderPinA, LOW);
    digitalWrite(emulatedEncoderPinB, LOW);
    digitalWrite(emulatedEncoderPinA, HIGH);
  }
} //END vbStepEmulation

/***********************************************************/
void serviceVBencoderPinA()
{
  cli(); /*Stop interrupts happening while we read pin values*/
  VBencoderReading = PIND & VBencoderBitMask; /*Read all eight pin values
  then strip away all but pinA and pinB's values*/
  if (VBencoderReading == B00001100 && VBencoderAFlag)
  { /*Check that we have both pins at detent (HIGH) and that we are
      expecting detent on this pin's rising edge*/
    VBencoderDir = CW; /*Flag VBencoder's rotation direction*/
    VBencoderBFlag = false; /*Reset flags for the next turn*/
    VBencoderAFlag = false;
    VBencoderChanged = true; /*Signal that the encoder has rotated*/
  }
  else
  {
    if (VBencoderReading == B00000100) /*Check for only A pin high*/
    {
      VBencoderBFlag = true; /*Signal that we're expecting pinB to
      signal the transition to detent from free rotation*/
    }
  }
  sei(); /*Restart interrupts*/
} //END serviceVBencoderPinA

/***********************************************************/
void serviceVBencoderPinB()
{
  cli(); /*Stop interrupts happening while we read pin values*/
  VBencoderReading = PIND & VBencoderBitMask; /*Rread all eight pin values
  then strip away all but pinA and pinB's values*/
  if (VBencoderReading == B00001100 && VBencoderBFlag)
  { /*Ccheck that we have both pins at detent (HIGH) and that we are
      expecting detent on this pin's rising edge*/
    VBencoderDir = CCW; /*Flag VB encoder's rotation direction*/
    VBencoderBFlag = false; /*Reset flags for the next turn*/
    VBencoderAFlag = false;
    VBencoderChanged = true; /*Signal that the encoder has rotated*/
  }
  else if (VBencoderReading == B00001000) /*Check for only B pin high*/
  {
    VBencoderAFlag = true; /*Signal that we're expecting pinA to
    signal the transition to detent from free rotation*/
  }
  sei(); /*Restart interrupts*/
} //END serviceVBencoderPinB

/***********************************************************/
void checkVBencoderSwitch()
{
  if (digitalRead(VBencoderSwPin) == LOW && (!(VBencoderSwPressed)))
    /*Check for VB encoder switch pin low and not previously pressed*/
  {
    VBencoderSwPressed = true; /*Flag VB encoder switch pressed*/
    Serial.println("SW Pressed");
    digitalWrite(emulatedEncoderSwPin, LOW); /*Set rotary encoder button
    emulation pin low*/
  }
  else if ((digitalRead(VBencoderSwPin) == HIGH) && VBencoderSwPressed)
    /*Is VB encoder switch pin high and was previously pressed*/
  {
    if (currentMode == CF) /*Toggle DSP set mode*/
    {
      currentMode = BW;
    }
    else
    {
      currentMode = CF;
    }
    Serial.println("SW Released");
    digitalWrite(emulatedEncoderSwPin, HIGH); /*Set rotary encoder button
    emulation pin high*/
    displayStatus(); /*Show current VB parameters on local display*/
    VBencoderSwPressed = false; /*Reset for next pass*/
    delay(20); /*Debounce VB encoder switch transitions*/
  }
} //END checkVBencoderSwitch

/*************************************************************/
/*
  The bandwidth step size varies based on the current values
  Bandwidth(Hz)/Step Size +/-
  200-400/20
  400-700/50
  700-3500/100
*/
void updateDspBW(byte rotationDirection)
{
  /*Value goes down by appropriate step based on current value*/
  if (rotationDirection == CCW)
  {
    switch (dspCurrentBW) /*Determine appropriate step*/
    {
      case 800 ... 3500:
        {
          dspCurrentBW = dspCurrentBW - 100;
          break;
        }
      case 450 ... 700:
        {
          dspCurrentBW = dspCurrentBW - 50;
          break;
        }
      case 220 ... 400:
        {
          dspCurrentBW = dspCurrentBW - 20;
          break;
        }
      default:
        {
          /*Value at min*/
          break;
        }
    }
  }
  else
    /*Value goes up by appropriate step based on current value*/
  {
    switch (dspCurrentBW) /*Determine appropriate step*/
    {
      case 200 ... 380:
        {
          dspCurrentBW = dspCurrentBW + 20;
          break;
        }
      case 400 ... 650:
        {
          dspCurrentBW = dspCurrentBW + 50;
          break;
        }
      case 700 ... 3400:
        {
          dspCurrentBW = dspCurrentBW + 100;
          break;
        }
      default:
        {
          /*Value at max*/
          break;
        }
    }
  }
} //END updateDspBW

/*************************************************************/
/*
  The center frequency step size varies based on the current values
  center frequency(Hz)/Step Size +/-
  200-2000/25
  2000-3500/50
*/
void updateDspCenterFreq(byte rotationDirection)
{
  /*Value goes down by appropriate step based on current value*/
  if (rotationDirection == CCW)
  {
    switch (dspCurrentCenterFreq) /*Determine appropriate step*/
    {
      case 2050 ... 3500:
        {
          dspCurrentCenterFreq = dspCurrentCenterFreq - 50;
          break;
        }
      case 225 ... 2000:
        {
          dspCurrentCenterFreq = dspCurrentCenterFreq - 25;
          break;
        }
      default:
        {
          /*Value at min*/
          break;
        }
    }
  }
  else
    /*Value goes up by appropriate step based on current value*/
  {
    switch (dspCurrentCenterFreq) /*Determine appropriate step*/
    {
      case 200 ... 1975:
        {
          dspCurrentCenterFreq = dspCurrentCenterFreq + 25;
          break;
        }
      case 2000 ... 3450:
        {
          dspCurrentCenterFreq = dspCurrentCenterFreq + 50;
          break;
        }
      default:
        {
          /*Value at max*/
          break;
        }
    }
  }
} //END updateDspCenterFreq

/***********************************************************/
/*Sends the current bandwidth, center frequency and mode to
  local display*/
void displayStatus()
{
  display.clearDisplay(); /*Clear to background color*/
  display.setCursor(0, 0); /*Start of first line*/
  display.print("  BW:"); /*Label first line*/
  display.println(dspCurrentBW); /*Display current bandwidth*/
  display.print("  CF:"); /*Label next line*/
  display.println(dspCurrentCenterFreq); /*Display current
  center frequency*/
  display.print("MODE:"); /*Label next line*/
  if (currentMode == CF) /*What mode is controlling*/
  {
    display.println("CF"); /*Mode is center frequency*/
  }
  else
  {
    display.println("BW"); /*Mode is bandwidth*/
  }
  display.display(); /*Display buffer*/
} //END displayStatus()

/***********************************************************/
void checkVbEncoder()
{
  if (VBencoderChanged) /*Has VB encoder been rotated - set by
  ISR*/
  {
    if (currentMode == CF) /*What is the DSP mode*/
    {
      updateDspCenterFreq(VBencoderDir); /*Update display to show we
      are controlling center frequency*/
    }
    else
    {
      updateDspBW(VBencoderDir); /*Update display to show we are
      controlling bandwidth*/
    }
    vbStepEmulation(VBencoderDir); /*Emulate four gray code state
    changes to step appropriate VB parameter in correct direction*/
    displayStatus(); /*Show current VB parameters on local display*/
    VBencoderChanged = false; /*Reset for next pass*/
  }
} //END checkVbEncoder

/***********************************************************/
void setup()
{
  Serial.begin(115200); /*For debugging only*/
  digitalWrite(emulatedEncoderPinA, LOW); /*Setup encoder emulation pine*/
  digitalWrite(emulatedEncoderPinB, HIGH);
  digitalWrite(emulatedEncoderSwPin, HIGH);
  pinMode(emulatedEncoderPinA, OUTPUT);
  pinMode(emulatedEncoderPinB, OUTPUT);
  pinMode(emulatedEncoderSwPin, OUTPUT);
  pinMode(VBencoderSwPin, INPUT_PULLUP); /*Hardware encoder pins - set as inputs pulled HIGH*/
  pinMode(VBencoderPinA, INPUT_PULLUP);
  pinMode(VBencoderPinB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(VBencoderPinA), serviceVBencoderPinA, CHANGE);
  /*Set an interrupt on VB PinA*/
  attachInterrupt(digitalPinToInterrupt(VBencoderPinB), serviceVBencoderPinB, CHANGE);
  /*Set an interrupt on VB PinB*/
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);  /*Initialize with the I2C addr 0x3D (128x64)
  If your dipslay is different, chage this value*/
  display.display(); /*Flush display buffer*/
  display.clearDisplay(); /*Set display background*/
  display.setTextSize(2); /*A little larger size for ease of reading*/
  display.setTextColor(WHITE); /*Text color*/
  displayStatus(); /*Show current VB parameters on local display*/
} //END setup

/***********************************************************/
void loop()
{
  checkVbEncoder(); /*Has VB encoder been rotated*/
  checkVBencoderSwitch(); /*Has VB encoder switch been pressed*/
} //END loop
