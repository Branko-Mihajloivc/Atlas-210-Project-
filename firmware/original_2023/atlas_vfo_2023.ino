#include <Rotary.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <avr/eeprom.h>

// S meter glyphs
unsigned long previousMillis = 0;
unsigned long starttime = 0;

struct settings_t
{
    int32_t bfo_offset ;
    uint32_t vfo ;
    bool superhet;
    int fcorrection;
}

settings;

byte bar5[8]={B00000,
B11011,
B11011,
B11011,
B11011,
B11011,
B11011,
B00000};

byte bar1[8]={B10000,
B11000,
B11100,
B11110,
B11110,
B11100,
B11000,
B10000};

#define F_MIN        300000000UL             // Lower frequency limit
#define F_MAX        3000000000UL

#define BAND_SELECT_IN A3                   // 5way rotary switch for band select

#define ENCODER_A    3                      // Encoder pin A
#define ENCODER_B    2                      // Encoder pin B
#define ENCODER_BTN  6                      //Step button  
#define TX_BTN  A2                          // Should be TX
#define SMETER_IN  A1                       //S meter in test the voltage and calibrate
#define BUTTON_BAND    4                    // Controls the band

// LCD - pin assignement in
// LCD defs
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

Si5351 si5351;
Rotary r = Rotary(ENCODER_A, ENCODER_B);
volatile uint32_t LSB = 552330000ULL;
volatile uint32_t USB = 552000000ULL;
volatile uint32_t bfo = 552000000ULL; //start in usb
//These USB/LSB frequencies are added to or subtracted from the vfo frequency in the "Loop()"
//In this example my start frequency will be 14.20000 plus 9.001500 or clk0 = 23.2015Mhz
volatile uint32_t vfo = 1000000000ULL / SI5351_FREQ_MULT; //start freq - change to suit
volatile uint32_t radix = 100;  //start step size - change to suit
volatile uint32_t ritstep=1;

volatile int32_t menu=0;  //start step size - change to suit
uint16_t menuoption=0;

boolean changed_f = 0;
String tbfo = "";
boolean rit=0;
boolean txon=0;
boolean savetag=0;
boolean setupmenu=0;
volatile int32_t rit_offset=0;

#define IF_Offset //Output is the display plus or minus the bfo frequency

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
int debounce = 20; // ms debounce period to prevent flickering when pressing or releasing the button
int DCgap = 250; // max ms between clicks for a double click event
int holdTime = 2000; // ms hold period: how long to wait for press+hold event
int longHoldTime = 5000; // ms long hold period: how long to wait for press+hold event

// Other button variables
boolean buttonVal = HIGH; // value read from button
boolean buttonLast = HIGH; // buffered value of the button's previous state
boolean DCwaiting = false; // whether we're waiting for a double click (down)
boolean DConUp = false; // whether to register a double click on next release, or whether to wait and click
boolean singleOK = true; // whether it's OK to do a single click
long downTime = -1; // time the button was pressed down
long upTime = -1; // time the button was released
boolean ignoreUp = false; // whether to ignore the button release because the click+hold was triggered
boolean waitForUp = false; // when held, whether to wait for the up event
boolean holdEventPast = false; // whether or not the hold event happened already
boolean longHoldEventPast = false;// whether or not the long hold event happened already

// Structure for Bands database
typedef struct
{
  char *name;
  uint64_t minFreq;       // Min. frequency value for the band (unit 0.01Hz)
  uint64_t maxFreq;       // Max. frequency value for the band (unit 0.01Hz)
  uint64_t lastFreq;      // Store the current frequency before change to other band (starts with minFreq value)
  long long offset;
  char *unitFreq;         // MHz or KHz
  float divider;          // value that will be the divider of current clock (just to present on display)
  short decimals;         // number of digits after the comma
  short initialStepIndex; // Index to the initial step of incrementing
  short finalStepIndex;   // Index to the final step of incrementing
  short lastStepIndex;    // Index to the last step used (initial index value same index defult)
  //void (*doSmth)(void);   // pointer to the function that will handle specific things for the band immediately after the band is selected
  bool ifPositive;
 } Band;
bool bandChanged = 0;
// Band database. You can change the band ranges if you need.
// The unit of frequency here is 0.01Hz (1/100 Hz). See Etherkit Library at https://github.com/etherkit/Si5351Arduino
 Band band[] = {
     {"80 ", 3300000LLU, 4200000LLU, 3700000LLU, 45500000LU, "KHz", 100000.0f, 0, 3, 6, 5, true},
     {"40 ", 6800000LLU, 7400000LLU, 7100000LLU, 45500000LU, "KHz", 100000.0f, 2, 1, 6, 3, true},
     {"20 ", 13800000LLU, 14650000LLU, 14100000LLU, 45500000LU, "KHz", 100000.0f, 2, 1, 6, 3, false},
     {"15 ", 20800000LLU, 21650000LLU, 21250000LLU, 45500000LU, "KHz", 100000.0f, 2, 1, 6, 3, false},
     {"10 ", 28200000LLU, 29600000LLU, 28900000LLU, 45500000LU, "KHz", 100000.0f, 2, 1, 7, 3, false}};
 // Calculate the last element position (index) of the array band
 const int lastBand = (sizeof band / sizeof(Band)) - 1; // For this case will be 26.
 short currentBand = 0;                                 // First band. For this case, AM is the current band.
 short oldBand = 255;

uint16_t tempReadSmeter;

int checkButton()
{ 
    
  currentBand = ReadBandSelect();
    if(currentBand != oldBand)
    {
        oldBand = currentBand;
        bandChanged = true;
        set_frequency(-1);
        vfo = band[currentBand].lastFreq;
    }
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
if ((millis()-upTime) < DCgap && DConUp == false && DCwaiting == true) DConUp = true;
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
if ( buttonVal == HIGH && (millis()-upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true) {
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
//downTime = millis();
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

/// @brief Reads band selection switch
/// @return Switch position from 0-4.
uint8_t ReadBandSelect()
{
    Serial.print(F("BandSelectRead - "));
    Serial.print(analogRead(BAND_SELECT_IN));
    delay(10);
    uint16_t tempRead = analogRead(BAND_SELECT_IN);
    Serial.print(F(" - "));
    Serial.println(tempRead);
    if(tempRead < 180) //0
    {
        return 0;
    }
    else if(tempRead < 400) //255
    {
        return 1;
    }
    else if(tempRead < 650) //512
    {
        return 2;
    }
    else if(tempRead < 900) //768
    {
        return 3;
    }
    else //1024
    {
        return 4;
    }
}

/**************************************/
/* Interrupt service routine for      */
/* encoder frequency change           */
/**************************************/
ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result == DIR_CW)
  {
    if(!setupmenu)
    set_frequency(1);
    else
    setup_menu(1);
  }
  else if (result == DIR_CCW)
  {
    if(!setupmenu)
    set_frequency(-1);
    else
    setup_menu(-1);    
  }
}
/**************************************/
/* Change the frequency               */
/* dir = 1    Increment               */
/* dir = -1   Decrement               */
/**************************************/
void set_frequency(short dir)
{
  savetag=false;
    if(!txon) //lock on tx
 {   
    
    if (dir == 1)
    {
      if(rit)
      rit_offset+= ritstep ;
      else
      vfo += radix; 
    }  
    if (dir == -1)
    {
      if(rit)
      rit_offset-= ritstep;
      else
      vfo -= radix;
    }

      if(vfo > band[currentBand].maxFreq)
        vfo = band[currentBand].maxFreq;
      if(vfo < band[currentBand].minFreq)
        vfo = band[currentBand].minFreq;

   if(rit)
   rit_offset=constrain(rit_offset,-10000,10000);

  
  changed_f = 1;
 }
}
/**************************************/
/* Read the button with debouncing    */
/**************************************/

boolean get_button()
{
  if (!digitalRead(ENCODER_BTN))
  {
    delay(20);
    if (!digitalRead(ENCODER_BTN))
    {
      while (!digitalRead(ENCODER_BTN));
      return 1;
    }
  }
  return 0;
}

/*****************************/
/* display S meter           */
/****************************/
void display_smeter(int strength)

{
// range is 0 to 1024 of adc  
//meter run from position 3 to 14 (12 )
int scale = (strength*15)/1024;
lcd.setCursor(0, 1);
lcd.print("S");
int smeter=(scale*11)/15;
if(smeter<10)
lcd.print(smeter);
else
lcd.print("9");
lcd.print(">");
unsigned long currentMillis = millis();
if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
//clear all after a small interval
lcd.setCursor(3, 1);
lcd.print("            ");
lcd.noCursor();
}

// write it and show for 500 milli sec
for (int i = 3; i<scale; i++)
  {
      lcd.setCursor(i, 1);
      lcd.write(byte(0));
      if(smeter>9)
      lcd.print("+");
  }
  
}
void display_rit()
{
  lcd.setCursor(0, 0);
  lcd.print("RIT:            ");
  lcd.setCursor(6, 0);
  lcd.print(rit_offset);
  lcd.print("Hz");  

        lcd.setCursor(15, 0);
        switch (ritstep)
          {
              case 1: 
                  lcd.print("1");
                   break;
              case 100:
                  lcd.print("2");
                   break;
              case 2500:
                  lcd.print("3"); 
                  break;
          }
  
}
/**************************************/
/* Displays the frequency             */
/**************************************/
void display_frequency()
{
  uint16_t f, g;

  lcd.setCursor(0, 0);
  lcd.print("VFO:             ");
  lcd.setCursor(4, 0);
  f = vfo / 1000000;  //variable is now vfo instead of 'frequency'
  Serial.println(vfo);
  
  if (f < 10)
    lcd.print(' ');
  lcd.print(f);
  lcd.print('.');
  f = (vfo % 1000000) / 1000;
  if (f < 100)
    lcd.print('0');
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  lcd.print('.');
  f = vfo % 1000;
  if (f < 100)
    lcd.print('0');
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  //lcd.print("Hz");
  lcd.setCursor(0, 1);
  //lcd.print(tbfo);
  //Serial.println(vfo + bfo);
  //Serial.println(tbfo);

}
void show_stored()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("vfo=");
  lcd.print(vfo);
  lcd.print("#");
  if(settings.superhet)
  lcd.print("S");
  else
  lcd.print("s");

}

void setup_menu(short dir)
{
  //called in setup mode
      if (dir == 1)
    {
      menu += radix; 
    }  
    if (dir == -1)
    {
      menu -= radix;
    }
  menu=constrain(menu,-10000000,10000000);
}

void  show_vfo_setup()
{

lcd.clear();
lcd.setCursor(0, 0);
lcd.print("VFO SETUP MODE"); 
delay(2000);  
lcd.clear();
//lcd.setCursor(0, 1);
//show_stored();
while(1)
    {
      //delay(300);
      //settings.superhet=true;
      //settings.fcorrection=0;
      //settings.bfo_offset=0;
      int b = checkButton();
      if ((b == 1) && (menuoption==0)) vfosteps();
      if (b == 2) menu_option();
      if (b == 3) holdEvent();
      if (b == 4) longHoldEvent();
      
      if(menuoption==0)
      show_bfo();
      if(menuoption==1)
      show_fcorrection();
      if(menuoption==2)
      show_bits();
    } 
}

void show_fcorrection()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FREQ CORRECTION");  
}

void show_bits()
{
  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print("SH  x4  DC"); 
  lcd.setCursor(0, 1);
  lcd.print(settings.superhet);  
}

void menu_option()
{  
menuoption+=1;
if(menuoption>2)
menuoption=0;  
}

void show_bfo(){
  settings.bfo_offset=menu;       
  uint32_t f, g;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" OFFSET:        ");
  lcd.setCursor(9, 0);
  lcd.print(settings.bfo_offset);
  lcd.setCursor(0, 1);
  lcd.print(" STEP:");
  lcd.print(round(log10(radix)+1));
  if((int(log10(radix)) % 2)==0)
  {
  lcd.setCursor(0, 0);
  lcd.write(byte(1));
  }  
  else
  {
  lcd.setCursor(0, 1);
  lcd.write(byte(1));
  }
}



void setup()
{
  pinMode(BAND_SELECT_IN, INPUT);
  currentBand = ReadBandSelect();
  bool i2c_found;
  Serial.begin(19200);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.createChar(0, bar5); 
  lcd.createChar(1, bar1);   
  lcd.begin(16, 2); 
  lcd.setCursor(0, 0);
  lcd.print("YU5KBM ATLAS 210");
  lcd.setCursor(4, 1);
  lcd.print("loading");
  delay(1000);
  lcd.clear();
  //read from eeprom
  eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
  vfo=constrain(settings.vfo,band[currentBand].minFreq,band[currentBand].maxFreq);
  //vfo=1000000;//remove it after the test run
     if(vfo==band[currentBand].minFreq||vfo==band[currentBand].maxFreq)
     {
      //dirty hack to fix the first run with eeprom
      settings.superhet=true;
      settings.fcorrection=0;
      settings.bfo_offset=552000000;
     }
  bfo= settings.bfo_offset; 
 //si5351.set_correction(settings.fcorrection); //**mine. There is a calibration sketch in File/Examples/si5351Arduino-Jason
  //where you can determine the correction by using the serial monitor.

  //initialize the Si5351
  i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0); //If you're using a 27Mhz crystal, put in 27000000 instead of 0
  // 0 is the default crystal frequency of 25Mhz.

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  // Set CLK0 to output the starting "vfo" frequency as set above by vfo = ?

  
  // Set CLK1 to output bfo frequency
  si5351.set_freq( bfo, SI5351_CLK1);
  
  si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_2MA); //you can set this to 2MA, 4MA, 6MA or 8MA
  si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA); //be careful though - measure into 50ohms
  //si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_2MA); //


  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(TX_BTN, INPUT_PULLUP);
  PCICR |= (1 << PCIE2);           // Enable pin change interrupt for the encoder
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  display_frequency();  // Update the display

  
  while(!digitalRead(ENCODER_BTN))
  {
  setupmenu=true;  
  show_vfo_setup();  
  delay(50000);
  }

  
}
void loop()
{

//eeprom timer

unsigned long currentMillis = millis();
if (currentMillis - starttime >= 120000) {
    starttime = currentMillis;
//clear all after a small interval
savetag=true;
}


    Serial.print(F("SMetarRead - "));
    Serial.print(analogRead(SMETER_IN));
    delay(10);
    tempReadSmeter = analogRead(SMETER_IN);
    Serial.print(F(" - "));
    Serial.println(tempReadSmeter);
  display_smeter(tempReadSmeter);
  // Update the display if the frequency has been changed
  
  //tx status
  if(!digitalRead(TX_BTN))  
  txon=true;
  else
  txon=false;
  
  
  if (changed_f)
  {
    if(rit & !txon)
    {
    display_rit();
    }
    else
    {    
    display_frequency();
    //reset rit offset to zero if frequency changed in non rit mode
    rit_offset=0;
    ritstep=1;
    }

    
    if(rit & !txon)  
    {
      if(band[currentBand].ifPositive)
      {
        si5351.set_freq(((vfo+rit_offset) * SI5351_FREQ_MULT) + bfo, SI5351_CLK0);
      }
      else
      {
        si5351.set_freq(((vfo+rit_offset) * SI5351_FREQ_MULT) - bfo, SI5351_CLK0);
      }
    }
    else
    {
      if(band[currentBand].ifPositive)
      {
        si5351.set_freq((vfo * SI5351_FREQ_MULT) + bfo, SI5351_CLK0);
      }
      else
      {
        si5351.set_freq((vfo * SI5351_FREQ_MULT) - bfo, SI5351_CLK0);
      }
    }
    
    //you can also subtract the bfo to suit your needs
    //si5351.set_freq((vfo * SI5351_FREQ_MULT) - bfo, SI5351_CLK0);
    //set clock1 as bfo
    si5351.set_freq( bfo, SI5351_CLK1);
    
    
     changed_f = 0;
  }


if(txon)
{
    display_frequency();
    delay(300);    
}
else
{
    if(rit)
    {
    display_rit();
    delay(100);    
    }    
}    
int b = checkButton();

if (b == 1) clickEvent();
if (b == 2) doubleClickEvent();
if (b == 3) holdEvent();
if (b == 4) longHoldEvent();


if(savetag)
{
//save to eeprom after 30 second of no frequency change
if(vfo !=settings.vfo)
  {
    settings.vfo=vfo;
    eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
    savetag=false;
    lcd.clear();
    delay(2000);
    display_frequency();

  }
}

}

void clickEvent() {

if(!rit)
vfosteps();
else
      {
        //steps for RIT
        lcd.setCursor(15, 0);
        switch (ritstep)
          {
              case 1: 
                  ritstep=100;
                  break;
              case 100:
                  ritstep=2500;
                  break;
              case 2500:
                  ritstep=1 ; 
                  break;
          }
          display_rit();
  
     }
}

void doubleClickEvent() {
  rit=!rit;
 if(rit)
 display_rit();
 else
 display_frequency();
 
}

void holdEvent() {
  lcd.setCursor(0, 0);
  lcd.print("HO ");
}

void longHoldEvent() {
  lcd.setCursor(0, 0);
  lcd.print("LO ");
  settings.vfo=vfo;
  eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));

}

void vfosteps()
{
  
switch (radix)
    {
      case 1:
        radix = 10;
        break;
      case 10:
        radix = 100;
        break;
      case 100:
        radix = 1000;
        break;
      case 1000:
        radix = 10000;
        break;
      case 10000:
        radix = 100000;
        break;
      case 100000:
        radix = 1000000;
        break;
      case 1000000:
        radix = 1;
        break;  
    }//end switch
      //show cursor 
  if(!setupmenu)
  {     
    if(radix<1000)
    lcd.setCursor((13-log10(radix)), 0);
    else if (radix<1000000)
    lcd.setCursor((12-log10(radix)), 0);
    else if (radix=1000000)
    lcd.setCursor(5, 0);
    lcd.cursor(); 
    delay(500);
    lcd.noCursor();
  }
    
}









