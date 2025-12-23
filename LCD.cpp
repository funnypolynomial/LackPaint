#include <Arduino.h>
#include <MCUFRIEND_kbv.h>  // https://github.com/prenticedavid/MCUFRIEND_kbv
#include "Pins.h"
#include "Config.h"
#include "LCD.h"

MCUFRIEND_kbv lcd;
// The Library API forces these:
bool LCD_First = false;
uint16_t kBlack = LCD_BLACK;
uint16_t kWhite = LCD_WHITE;

#ifdef SERIALIZE
bool LCD_serialize = false;
#define SERIALISE_INIT(_w,_h,_s) if (LCD_serialize) { Serial.print(_w);Serial.print(',');Serial.print(_h);Serial.print(',');Serial.println(_s);}
#define SERIALISE_BEGINFILL(_x,_y,_w,_h) if (LCD_serialize) { Serial.print(_x);Serial.print(',');Serial.print(_y);Serial.print(',');Serial.print(_w);Serial.print(',');Serial.println(_h);}
#define SERIALISE_FILLCOLOUR(_len,_colour) if (LCD_serialize) { Serial.print(_len);Serial.print(',');Serial.println(_colour);}
#define SERIALISE_FILLBYTE(_len,_colour) if (LCD_serialize) { Serial.print(_len);Serial.print(',');Serial.println(_colour?0xFFFF:0x0000);}
#define SERIALISE_ONEBLACK() if (LCD_serialize) Serial.println("B");
#define SERIALISE_ONEWHITE() if (LCD_serialize) Serial.println("W");
#else
#define SERIALISE_INIT(_w,_h,_s)
#define SERIALISE_BEGINFILL(_x,_y,_w,_h)
#define SERIALISE_FILLCOLOUR(_len,_colour)
#define SERIALISE_FILLBYTE(_len,_colour)
#define SERIALISE_ONEBLACK()
#define SERIALISE_ONEWHITE()
#endif

void TouchCalib();

// Init the LCD
void LCD_INIT()
{
  lcd.begin(0x6814); // found by running try running CUFRIEND_kbv sample "diagnose_TFT_support"
#ifdef CFG_LCD_USB_LEFT  
  lcd.setRotation(1); // USB top-left 
#else  
  lcd.setRotation(3); // USB bottom-right 
#endif
  SERIALISE_INIT(LCD_WIDTH, LCD_HEIGHT, 1); 
#ifdef CFG_TOUCH_CALIB
  TouchCalib();
#endif  
}

// Define a window to fill with pixels at (x,y) w w, h h
// Return number of pixels
uint32_t LCD_BEGIN_FILL(uint16_t x, uint16_t y, uint16_t w, uint16_t h) 
{
  SERIALISE_BEGINFILL(x, y, w, h);
  lcd.setAddrWindow(x, y, x + w - 1, y + h - 1); 
  LCD_First = true; 
  uint32_t n = w;
  n *= h;
  return n;
}

void LCD_FILL_COLOUR(uint32_t n, uint16_t c)
{
  SERIALISE_FILLCOLOUR(n, c);
  while (n--)
    lcd.pushColors(&c, 1, LCD_First); 
  LCD_First = false; 
}

// Sends a single white pixel
void LCD_ONE_WHITE() 
{ 
  SERIALISE_ONEWHITE(); 
  lcd.pushColors(&kWhite, 1, LCD_First); 
  LCD_First = false; 
}

// Sends a single black pixel
void LCD_ONE_BLACK()
{
  SERIALISE_ONEBLACK(); 
  lcd.pushColors(&kBlack, 1, LCD_First);
  LCD_First = false; 
}

// Fill a rectangle
void LCD_FILL_RECT(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour)
{
  if (LCD_serialize)
  {
    SERIALISE_BEGINFILL(x, y, w, h);
    uint32_t n = w;
    n *= h;
    SERIALISE_FILLCOLOUR(n, colour);
  }
  lcd.fillRect(x, y, w, h, colour); 
}

// ----------- Touch -----------
#ifdef CFG_LCD_HAS_TOUCH
// LCD pixel
#define LCD_X_MIN   0
#define LCD_X_MAX   (LCD_WIDTH - 1)
#define LCD_Y_MIN   0
#define LCD_Y_MAX   (LCD_HEIGHT - 1)
// Roughly calibrated, but close enough (see CFG_TOUCH_CALIB)
// imperfect pixel correspondence
// ADC reading, 0..1023
#define TOUCH_X_MIN 70
#define TOUCH_X_MAX 830
#define TOUCH_Y_MIN 110
#define TOUCH_Y_MAX 830

int TouchAxis(int PinGnd, int Pin5V, int PinPullUp, int PinRead)
{
  // general read an axis
  pinMode(PinPullUp, INPUT); // works(?)
  pinMode(PinRead, INPUT);
    
  // voltage across axis:
  pinMode(PinGnd, OUTPUT);
  digitalWrite(PinGnd, LOW);
  pinMode(Pin5V, OUTPUT);
  digitalWrite(Pin5V, HIGH);

  // read voltage divider
  int value = analogRead(PinRead);
  // restore pins
  pinMode(PinPullUp, OUTPUT);
  pinMode(PinRead, OUTPUT);
  return value;  
}  

// -ve means none. Only Portrait left/right orientations
int GetTouchX()
{
  int x = TouchAxis(PIN_TOUCH_X_DIGITAL, PIN_TOUCH_X_ANALOG, PIN_TOUCH_Y_DIGITAL, PIN_TOUCH_Y_ANALOG);
  if (TOUCH_X_MIN <= x && x <= TOUCH_X_MAX)
#ifdef CFG_LCD_USB_LEFT  
    x = map(x, TOUCH_X_MIN, TOUCH_X_MAX, LCD_X_MIN, LCD_X_MAX);
#else    
    x = LCD_X_MAX - map(x, TOUCH_X_MIN, TOUCH_X_MAX, LCD_X_MIN, LCD_X_MAX);
#endif    
  else
    x = -1;
  return x;
}

word GetTouchY()
{
  int y = TouchAxis(PIN_TOUCH_Y_DIGITAL, PIN_TOUCH_Y_ANALOG, PIN_TOUCH_X_DIGITAL, PIN_TOUCH_X_ANALOG);
  if (TOUCH_Y_MIN <= y && y <= TOUCH_Y_MAX)
#ifdef CFG_LCD_USB_LEFT  
    y = map(y, TOUCH_Y_MIN, TOUCH_Y_MAX, LCD_Y_MIN, LCD_Y_MAX);
#else    
    y = LCD_Y_MAX - map(y, TOUCH_Y_MIN, TOUCH_Y_MAX, LCD_Y_MIN, LCD_Y_MAX);
#endif
  else
    y = -1;
  TouchAxis(PIN_TOUCH_X_DIGITAL, PIN_TOUCH_X_ANALOG, PIN_TOUCH_Y_DIGITAL, PIN_TOUCH_Y_ANALOG); // discard x (?)
  return y;
}

bool LCD_GET_TOUCH(int& x, int& y)
{
  // x, y from top-left, true if both valid
  x = GetTouchX();
  if (x > 0)
  {
    y = GetTouchY();
    return y > 0;
  }
  return false;
}

void TouchCalib()
{
  LCD_FILL_RECT(0, 0, LCD_WIDTH, LCD_HEIGHT, LCD_WHITE);
  Serial.println("Drag stylus to all edges. Note min/max values, update TOUCH_*");
  while (true)
  {
    int x = TouchAxis(PIN_TOUCH_X_DIGITAL, PIN_TOUCH_X_ANALOG, PIN_TOUCH_Y_DIGITAL, PIN_TOUCH_Y_ANALOG);
    int y = TouchAxis(PIN_TOUCH_Y_DIGITAL, PIN_TOUCH_Y_ANALOG, PIN_TOUCH_X_DIGITAL, PIN_TOUCH_X_ANALOG);
    TouchAxis(PIN_TOUCH_X_DIGITAL, PIN_TOUCH_X_ANALOG, PIN_TOUCH_Y_DIGITAL, PIN_TOUCH_Y_ANALOG);
    if (x > 5) // else no touch?
    {
      Serial.print("x: ");Serial.print(x);
      Serial.print(", y: ");Serial.println(y);
    }
  }
}
#else
bool LCD_GET_TOUCH(int& , int& )
{
  return false;
}
#endif
