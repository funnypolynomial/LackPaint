#include <Arduino.h>
#include "Config.h"
#include "LCD.h"
#include "Slides.h"
#include "App.h"

// data: see resources sub-directory:
#include "GraphicsData.h"
#include "FontData.h"

// the rounded corners, made by hand, 5x5
#define ROUND_CORNER_SIZE 5
static const uint8_t TopLeft[] PROGMEM =
{
  0,0, 0,0, 0,5, 0,5,  // xHi,xLo, yHi,yLo, wHi,wLo, hHi,hLo,
  0b00000111,
  0b00011111,
  0b00111111,
  0b01111111,
  0b01111111,
};

static const uint8_t TopRight[] PROGMEM =
{
  1,219, 0,0, 0,5, 0,5,  // xHi,xLo, yHi,yLo, wHi,wLo, hHi,hLo,
  0b00000000,
  0b11000000,
  0b11100000,
  0b11110000,
  0b11110000,
};

static const uint8_t BottomLeft[] PROGMEM =
{
  0,0, 1,59, 0,5, 0,5,  // xHi,xLo, yHi,yLo, wHi,wLo, hHi,hLo,
  0b00101000,
  0b01010000,
  0b00101000,
  0b00010000,
  0b00000000,
};

// Note the bottom-right corner looks a bit less rounded because of the way
// it combines with the checkerboard pattern.
static const uint8_t BottomRight[] PROGMEM =
{
  1,219, 1,59, 0,5, 0,5,  // xHi,xLo, yHi,yLo, wHi,wLo, hHi,hLo,
  0b01010000,
  0b10100000,
  0b01000000,
  0b10000000,
  0b00000000,
};

// multiple strings in a single PROGMEM string
#undef MSTR
#define MSTR(_s) _s "\0"

namespace App
{
  // The app menu (\x80=<apple>)
  static const char pMenu[] PROGMEM = MSTR("\x80") MSTR("File") MSTR("Edit") MSTR("Goodies") MSTR("Font") MSTR("FontSize") MSTR("Style");
  // Splash text (\x7F=<bullet>, \x81=<command>)
  static const char pSplash[] PROGMEM = "\x81 LackPaint \x80 Mark Wilson \x7F MMXXV \x81";
  // Default image name
  static const char pUntitled[] PROGMEM = "untitled";
  // Messages
  static const char pPausedMsg[] PROGMEM = "Paused";
  static const char pSeedMsg[] PROGMEM = "Randomized";
  // Help
  static const char pHelp[] PROGMEM = "Tap image to pause/resume."
  #ifdef CFG_RANDOM_ORDER
    " Elsewhere to randomize."
  #endif
  ;
  
  bool paused = false;
  bool getNextSlide = false;
  
  void DrawOneBPPData(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* pData, bool white1, uint16_t size = 1, bool compressed = true)
  {
    // draw 1BPP data from pData into x, y, w, h. if white1, 1's are white, else black
    // fill sigle pixels if size=1, else blocks
    const byte* prevFullRows[BMP_ROW_ESC_IDX + 1]; // record ptrs to the most recent 8 (lower 8) full rows and the first 8 full rows (upper 8)
    byte recentFullRowsIdx = 0;
    byte initialFullRowsIdx = BMP_ROW_ESC_IDX_BLACK;
    prevFullRows[initialFullRowsIdx++] = NULL; // black row
    if (size == 1)
      LCD_BEGIN_FILL(x, y, w, h);
    for (uint16_t row = 0; row < h; row++)
    {
      const uint8_t* pRow = NULL;
      // there is some simple compression, 1-byte escape values indicating a duplicate row
      uint8_t firstByte = pgm_read_byte(pData);
      if (compressed && (firstByte & BMP_ROW_ESC_MASK) == BMP_ROW_ESC_VALUE)
      {
        // add escaped row
        pRow = prevFullRows[firstByte & BMP_ROW_ESC_IDX];
        if (!pRow)
          LCD_FILL_COLOUR(size * w, LCD_BLACK); // all black
        // repaint a previous row
        pData++; // just one byte in row
      }
      else
      {
        // add a new full row
        pRow = pData;
        // update initial and recent full row ptrs
        if (initialFullRowsIdx <= BMP_ROW_ESC_IDX)
          prevFullRows[initialFullRowsIdx++] = pRow;
        prevFullRows[recentFullRowsIdx++] = pRow;
        if (recentFullRowsIdx >= BMP_ROW_ESC_IDX_BLACK) // wrap recent rows
          recentFullRowsIdx = 0;
        pData = NULL; // update pData with pRow
      }
      if (pRow)
      {
        uint8_t Mask = 0x80;
        uint8_t Byte = pgm_read_byte(pRow++);
        for (uint16_t col = 0; col < w; col++)
        {
          if (size == 1)
          {
            if ((bool)(Byte & Mask) == white1) // a 1 == white
              LCD_ONE_WHITE();
            else
              LCD_ONE_BLACK();
          }
          else
          {
            if ((bool)(Byte & Mask) != white1) // only draw black (white backgound)
              LCD_FILL_RECT(x + col*size, y + row*size, size, size, LCD_BLACK);
          }
          Mask >>= 1;
          if (!Mask && (col < (w - 1))) // next byte in row
          {
            Mask = 0x80;
            Byte = pgm_read_byte(pRow++);
          }
        }
        if (!pData)
          pData = pRow;
      }
    }
  }
  
  int8_t SignedNibble(uint8_t value)
  {
    // convert 0bsxxx to signed value s=1 is -ve
    if (value & 0x08)
      return -(int8_t)(value & 0x07);
    else
      return  (int8_t)(value & 0x07);
  }
  
  void DrawChar(uint8_t ch, uint16_t& x, uint16_t y, bool draw)
  {
    // draw char with baseline at x, y. advances x
    if (ch == ' ')
    {
      x += 4;
      return;
    }
    const uint8_t* pData = Font;
    uint8_t def = 0;
    int8_t w = 0, h = 0, dx = 0, dy = 0;
    do
    {
      def = pgm_read_byte(pData++);
      if (def)
      {
        uint8_t temp = pgm_read_byte(pData++);  // (w, h):0bwwwwhhhh, (dx, dy)
        w = (int8_t)(temp >> 4);
        h = (int8_t)(temp & 0x0F);
        temp = pgm_read_byte(pData++);          // (dx, dy):0bsxxxsyyy s:sign, 1=-ve
        dx = SignedNibble(temp >> 4);
        dy = SignedNibble(temp & 0x0F);
        if (def < ch)
          pData += (w <= 8)?h:2*h;
        else
          break;
      }
    } while (def);
    if (def == ch)
    {
      x += dx;
      y += -dy - h + 1;
      if (draw)
        DrawOneBPPData(x, y, w, h, pData, false, 1, false);
      x += w + 1;
    }
  }
  
  void DrawString(const char* str, uint16_t& x, uint16_t y, bool progmem)
  {
    // draw the string at x, y. advanced x. progmem or RAM
    if (str)
      while (progmem?pgm_read_byte(str):*str)
        DrawChar(progmem?pgm_read_byte(str++):*str++, x, y, true);
  }
  
  uint16_t PixelWidth(const char* str, bool progmem)
  {
    // return pixel width of string. progmem or RAM
    uint16_t x = 0;
    if (str)
      while (progmem?pgm_read_byte(str):*str)
        DrawChar(progmem?pgm_read_byte(str++):*str++, x, 0, false);
    return x;
  }
  
  uint16_t GetHiLo(const uint8_t*& pData)
  {
    // read Hi, Lo bytes from progmen, return word
    uint16_t hi = pgm_read_byte(pData++)*256;
    return hi + pgm_read_byte(pData++);
  }

  void DrawWindowData(const uint8_t* pData, bool compressed = true)
  {
    // draw the encoded data at pData (with or without compression)
    uint16_t x = GetHiLo(pData);
    uint16_t y = GetHiLo(pData);
    uint16_t w = GetHiLo(pData);
    uint16_t h = GetHiLo(pData);
    DrawOneBPPData(x, y, w, h, pData, true, 1, compressed);
  }
  
  void DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
  {
    // black rect x, y, w, h. 1 pixel lines
    LCD_FILL_RECT(x, y, w, 1, LCD_BLACK);         // top
    LCD_FILL_RECT(x, y, 1, h, LCD_BLACK);         // left
    LCD_FILL_RECT(x + w - 1, y, 1, h, LCD_BLACK); // right
    LCD_FILL_RECT(x, y + h - 1, w, 1, LCD_BLACK); // bottom
  }
  
  void DrawWindowFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
  {
    // x, y, w, h are of window interior
    w += 2;
    h += 2;
    DrawRect(--x, --y, w, h);
    // drop shadow
    LCD_FILL_RECT(x + w, y, 1, h + 1, LCD_BLACK); // right
    LCD_FILL_RECT(x, y + h, w + 1, 1, LCD_BLACK); // bottom
  }
  
  void DrawImageTitleBarBands(uint16_t x, uint16_t y, uint16_t h, uint16_t len)
  {
    // Draw the banded part of the title bar
    LCD_FILL_RECT(x, y, len, 3, LCD_WHITE); // upper gap
    for (int bar = 0; bar < 6; bar++) // bands
    {
      LCD_FILL_RECT(x, y + 3 + 2 * bar, len, 1, LCD_BLACK);
      LCD_FILL_RECT(x, y + 4 + 2 * bar, len, 1, LCD_WHITE);
    }
    LCD_FILL_RECT(x, y + h - 2, len, 2, LCD_WHITE); // lower gap
  }
  
  void DrawImageTitleBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
  {
    // Initial draw of blank titlebar
    DrawImageTitleBarBands(x + 1, y, h, w - 2);
    LCD_FILL_RECT(x, y + h, w, 1, LCD_BLACK); // bottom line
    // Square on LHS
    LCD_FILL_RECT(x + 7, y + 3, 13, 11, LCD_WHITE);
    DrawRect(x + 8, y + 3, 11, 11);
  }
  
  void DrawImageTitleText(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* pStr, bool progmem)
  {
    // Update the text on the title bar
    const uint16_t kBandsLHS = 20;
    const uint16_t kBandsRHS = 2;
    if (pStr)
    {
      // allow a title bar with no title
      const uint16_t gap = 6;
      uint16_t len = PixelWidth(pStr, progmem);
      uint16_t cenX = x + (w - len) / 2;
      uint16_t bandX = x + kBandsLHS;
      DrawImageTitleBarBands(bandX, y, h, cenX - gap - bandX); // LHS to text
      bandX = cenX - gap + len + 2*gap;
      DrawImageTitleBarBands(bandX, y, h, x + w - kBandsRHS - bandX); // text to RHS
      LCD_FILL_RECT(cenX - gap, y + 1, len + 2*gap, h - 2, LCD_WHITE);
      DrawString(pStr, cenX, y + FONT_HEIGHT, progmem);
    }
    else // blank
      DrawImageTitleBarBands(x + kBandsLHS, y, h, w - kBandsRHS - kBandsLHS);
  }

  #define MENU_TEXT_Y 13
  #define MENU_ITEM_GAP 13
  #define MENU_BAR_HEIGHT 19
  void DrawMenuMessage(const char* pMsg, bool draw)
  {
    // draw/clear msg on far right of menu bar
    int w = PixelWidth(pMsg, true);
    uint16_t x = LCD_WIDTH - w - ROUND_CORNER_SIZE;
    if (draw)
      DrawString(pMsg, x, MENU_TEXT_Y, true);
    else
      LCD_FILL_RECT(x, 0, w, MENU_BAR_HEIGHT - 1, LCD_WHITE);
  }
  
  void DrawMenuItems()
  {
    // draw the menu text items, apple, File etc
    const char* pStr = pMenu;
    uint16_t x = 12;
    while (pgm_read_byte(pStr))
    {
      DrawString(pStr, x, MENU_TEXT_Y, true);
      x += MENU_ITEM_GAP;
      pStr += strlen_P(pStr) + 1;
    }    
  }

  void DrawLines()
  {
    // Draw the palette of line types. Done in code vs window data, saves ~90 bytes code space
    const int kLinesX = 26;
    const int kLinesY = 248;
    const int kLinesW = 29;
    const int kLinesGap = 9; // whitespace between lines (first gap is 10)
    int y = kLinesY;
    int h = 1;
    LCD_BEGIN_FILL(kLinesX, y, kLinesW, h);
    for (uint16_t x = 0; x < kLinesW; x++)
      if (x & 0b11)
        LCD_ONE_WHITE();
      else
        LCD_ONE_BLACK();
    y++;
    for (int line = 0; line < 4; line++)
    {
      y += kLinesGap + h;
      h = 1 << line;
      LCD_FILL_RECT(kLinesX, y, kLinesW, h, LCD_BLACK);
    }
  }
  
  // constants are for INSIDE of window...
  // drawing window
  const uint16_t kDrawWindowX = 74;
  const uint16_t kDrawWindowY = 30;
  const uint16_t kDrawWindowW = 396;
  const uint16_t kDrawWindowH = 236;
  const uint16_t kDrawWindowTitleH = 17;
  const uint16_t kDrawWindowImageW = kDrawWindowW; // 396 pixels wide
  const uint16_t kDrawWindowImageH = kDrawWindowH - kDrawWindowTitleH - 1; // 218 pixels high

  // timing of slides:
  uint32_t LastImageAtMS = 0;
  uint32_t TimeToNextImageMS = 0;
  
  void DrawSplash(bool draw)
  {
#ifdef CFG_FULL_SPLASH    
    // draw (or clear) the icon centred and enlarged, and help line
    const int kIconSize = 32;
    const int kCellSize = 3;
    uint16_t x = kDrawWindowX + (kDrawWindowImageW - kIconSize*kCellSize)/2;
    uint16_t y = kDrawWindowY + kDrawWindowTitleH + (kDrawWindowImageH - kIconSize*kCellSize)/2;
    if (draw)
      DrawOneBPPData(x, y, kIconSize, kIconSize, pIconImage, false, kCellSize);
    else
      LCD_FILL_RECT(x, y, kIconSize*kCellSize, kIconSize*kCellSize, LCD_WHITE);
      
    uint16_t w = PixelWidth(pHelp, true);
    x = kDrawWindowX + (kDrawWindowW - w)/2;
    y = kDrawWindowY + kDrawWindowH - FONT_DESCENDER - 2; 
    if (draw)
      DrawString(pHelp, x, y, true);    
    else
      LCD_FILL_RECT(x, y - FONT_HEIGHT, w, FONT_HEIGHT + FONT_DESCENDER + 1, LCD_WHITE); 
#else
    (void)draw;
#endif  
  }

  void DrawBusy(bool draw, bool disk = false)
  {
    // draw/clear the busy "watch" or disk cursor/icon
#ifdef CFG_SHOW_BUSY_ICONS    
    const int kBusySize = 16;
    uint16_t x = LCD_WIDTH - kBusySize - ROUND_CORNER_SIZE;
    uint16_t y = (MENU_BAR_HEIGHT - kBusySize)/2;
    if (draw)
      DrawOneBPPData(x, y, kBusySize, kBusySize,disk?pDiskImage:pBusyImage, false);
    else
      LCD_FILL_RECT(x, y, kBusySize, kBusySize, LCD_WHITE);
#else
    (void)draw;
    (void)disk;
#endif      
  }

  #define TOUCH_SAMPLE_COUNT_SHIFT 3 // 8 samples
  #define TOUCH_SAMPLE_COUNT (1 << TOUCH_SAMPLE_COUNT_SHIFT)
  byte _xSamples[TOUCH_SAMPLE_COUNT];
  byte _ySamples[TOUCH_SAMPLE_COUNT];
  byte _sampleCount = 0;
  byte _sampleIdx = 0;
  
  int Average(byte* pSamples)
  {
    // return the average sample, TIMES TWO
    int sum = 0;
    for (int i = 0; i < TOUCH_SAMPLE_COUNT; i++)
      sum += *pSamples++;
    return sum >> (TOUCH_SAMPLE_COUNT_SHIFT - 1);
  }
  
  bool GetStableTouch(int& touchX, int& touchY)
  {
    // true if there's a touch. averaged position (seems quite noisy)
    // From AMeagreBall where RAM was tight, hence the byte-sized things
    if (LCD_GET_TOUCH(touchX, touchY))
    {
      // discard LSB so they fit in a byte (HALVED)
      touchX >>= 1;
      touchY >>= 1;
      if (_sampleCount == TOUCH_SAMPLE_COUNT)
      {
        _xSamples[_sampleIdx] = touchX;
        _ySamples[_sampleIdx] = touchY;
        _sampleIdx++;
        if (_sampleIdx >= TOUCH_SAMPLE_COUNT)
          _sampleIdx = 0;
        touchX = Average(_xSamples);
        touchY = Average(_ySamples);
        _sampleCount = _sampleIdx = 0; // reset
        return true;
      }
      else
      {
        // just add the samples
        _xSamples[_sampleIdx] = touchX;
        _ySamples[_sampleIdx] = touchY;
        _sampleCount = ++_sampleIdx;
        return false;
      }
    }
    else
    {
      _sampleCount = _sampleIdx = 0;
      return false;
    }
  }
  
  void Init()
  {
    // draw the entire screen...
    
    // fill with alternating white/black pixels
    SERIALISE_ON(false);
    LCD_BEGIN_FILL(0, 0, LCD_WIDTH, LCD_HEIGHT);
    for (uint16_t row = 0; row < LCD_HEIGHT; row++)
      for (uint16_t col = 0; col < LCD_WIDTH; col++)
        if ((col + (row & 0x0001)) & 0x0001)
          LCD_ONE_WHITE();
        else
          LCD_ONE_BLACK();    

    // if serializing the screen, add the alternating pixels as a short-cut command
    SERIALISE_ON(true);
#ifdef SERIALIZE
    Serial.println("!"); // serialize short-cut:fill with alt pix!
#endif      
        
    // menu bar
    LCD_FILL_RECT(0, 0, LCD_WIDTH, MENU_BAR_HEIGHT, LCD_WHITE);
    LCD_FILL_RECT(0, MENU_BAR_HEIGHT, LCD_WIDTH, 1, LCD_BLACK);

    DrawWindowFrame(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowH);
    LCD_FILL_RECT(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowH, LCD_WHITE);
  
    // drawing window title bar
    DrawImageTitleBar(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowTitleH);
#ifdef DEBUG
    DrawImageTitleText(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowTitleH, pUntitled, true);
    DrawMenuItems();
#else
    // if splash, no main menu
    DrawImageTitleText(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowTitleH, pSplash, true);
    DrawSplash(true);
#endif    
    
    // tools window
    const uint16_t kToolWindowX = 10;
    const uint16_t kToolWindowY = 29;
    const uint16_t kToolWindowW = 51;
    const uint16_t kToolWindowH = 197;
    DrawWindowFrame(kToolWindowX, kToolWindowY, kToolWindowW, kToolWindowH);
    DrawWindowData(pToolsData);
  
    // line window
    const uint16_t kLineWindowX = 10;
    const uint16_t kLineWindowY = 237;
    const uint16_t kLineWindowW = 51;
    const uint16_t kLineWindowH = 75;
    DrawWindowFrame(kLineWindowX, kLineWindowY, kLineWindowW, kLineWindowH);
    LCD_FILL_RECT(kLineWindowX, kLineWindowY, kLineWindowW, kLineWindowH, LCD_WHITE);
    DrawWindowData(pTickData);
    DrawLines();
  
    // fill window
    const uint16_t kFillWindowX = 72;
    const uint16_t kFillWindowY = 279;
    const uint16_t kFillWindowW = 401;
    const uint16_t kFillWindowH = 33;
    DrawWindowFrame(kFillWindowX, kFillWindowY, kFillWindowW, kFillWindowH);
    DrawWindowData(pFillsData);
  
    // rounded corners
    DrawWindowData(TopLeft, false);
    DrawWindowData(TopRight, false);
    DrawWindowData(BottomLeft, false);
    DrawWindowData(BottomRight, false);
    
#ifndef DEBUG
    delay(5000);  // dwell on splash
    // if splash, draw main menu
    DrawImageTitleText(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowTitleH, pUntitled, true);
    DrawMenuItems();
    DrawSplash(false);
#endif

    DrawBusy(true, true);
    Slides::GetFirst();
    DrawBusy(false);
    getNextSlide = false;
    LastImageAtMS = millis();
    TimeToNextImageMS = 2000UL; // blank at the start
  }

  void Loop()
  {
    // called on main loop, checks if it's time for the next slide
    uint32_t NowMS = millis();
    if (!paused && (NowMS - LastImageAtMS) >= TimeToNextImageMS)
    {
      // next slide
      // scanning the dir may take a long time, show the busy cursor
      DrawBusy(true, true);
      if (getNextSlide)
        Slides::GetNext();
      getNextSlide = true;
      char pFileName[SLIDE_APPENDED_TEXT_MAX_LEN + 1];
      DrawBusy(true);
      DrawImageTitleText(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowTitleH, NULL, false); // no title while drawing
      bool painted = Slides::PaintCurrent(kDrawWindowX, kDrawWindowY + kDrawWindowTitleH + 1, kDrawWindowImageW, kDrawWindowImageH, pFileName);
      bool haveName = strlen(pFileName);
      if (haveName)
        DrawImageTitleText(kDrawWindowX, kDrawWindowY, kDrawWindowW, kDrawWindowTitleH, pFileName, false);
      DrawBusy(false);
      LastImageAtMS = millis();
      if (painted)
        TimeToNextImageMS = CFG_SECONDS_BETWEEN_IMAGES;
      else
        TimeToNextImageMS = haveName?1:0; // go to next quickly. Flash bad file name
      TimeToNextImageMS *= 1000UL;
    }
#ifdef CFG_LCD_HAS_TOUCH    
    else
    {
      int x, y;
      if (GetStableTouch(x, y))
      {
        int x1, y1;
        while (LCD_GET_TOUCH(x1, y1)) // wait for up
          ;
#ifdef CFG_SHOW_TOUCH          
        LCD_FILL_RECT(x - 3, y, 7, 1, RGB(255,0,0));
        LCD_FILL_RECT(x, y - 3, 1, 7, RGB(255,0,0));
#endif        
        if ((int)kDrawWindowX <= x && x <= (int)(kDrawWindowX + kDrawWindowW) &&
            (int)kDrawWindowY <= y && y <= (int)(kDrawWindowY + kDrawWindowH))
        {
          // touch in image window, pause/resume
          paused = !paused;
          DrawMenuMessage(pPausedMsg, paused);
        }
        else if (!paused)
        {
          // touch elsewhere, randomize. 
          // PRNG is seeded from millis(), touch position and current image file name.
          DrawMenuMessage(pSeedMsg, true);
          uint32_t seed = x*y; 
          seed += millis();
          seed ^= Slides::NameAsSeed();
          randomSeed(seed);
          delay(1000); // show msg briefly
          DrawMenuMessage(pSeedMsg, false);
        }
      }
    }
#endif    
  }
  
}
