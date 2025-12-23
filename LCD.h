#pragma once

// Optionally dump graphics cmds to serial:
//#define SERIALIZE
#ifdef SERIALIZE
#define SERIALISE_ON(_on) LCD_serialize = _on;
#define SERIALISE_COMMENT(_c) if (LCD_serialize) { Serial.print("; ");Serial.println(_c);}
#else
#define SERIALISE_ON(_on)
#define SERIALISE_COMMENT(_c)
#endif

#define RGB(_r, _g, _b) (word)((_b & 0x00F8) >> 3) | ((_g & 0x00FC) << 3) | ((_r & 0x00F8) << 8)

#define LCD_WIDTH   480
#define LCD_HEIGHT  320

#define LCD_BLACK 0x0000
#define LCD_WHITE 0xFFFF

// Functions, but could be macros, if speed mattered
void LCD_INIT();
uint32_t LCD_BEGIN_FILL(uint16_t x, uint16_t y, uint16_t w, uint16_t h); 
void LCD_FILL_COLOUR(uint32_t n, uint16_t c);
void LCD_ONE_WHITE();
void LCD_ONE_BLACK();
void LCD_FILL_RECT(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour);
bool LCD_GET_TOUCH(int& x, int& y);

extern bool LCD_serialize;
