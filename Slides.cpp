#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Pins.h"
#include "Config.h"
#include "LCD.h"
#include "Slides.h"

namespace Slides
{
  // see  https://docs.arduino.cc/libraries/sd/
  File root;
  bool haveFile = false;
  char fileName[8 + 1 + 3 + 1]; // space for 8.3 + NUL

  bool CheckFile(File& file)
  {
    // true if the files is not a directory
    // copies file name
    // closes file 
    if (file)
    {
      if (!file.isDirectory())
      {
        haveFile = true;
        strcpy(fileName, file.name());
      }
      file.close();
      return true;
    }
    return false;
  }

#ifdef CFG_RANDOM_ORDER
  // images cycle in pseudo-random order
  uint32_t numberOfFiles = 0;
  uint32_t previousN = 0;
  uint32_t seed = 0; // generate some entropy from all the file names and seed the PRNG

  uint32_t ScanFiles(uint32_t n)
  {
    // scan the directory counting files
    // if n is non-zero, stops on the nth file (counting from 1)
    // if n is 0, scans ALL files and, if not DEBUG, generates a PRNG seed from the names
    // returns the number of files scanned
    // 
    uint32_t count = 0;
    root = SD.open(CFG_IMAGE_FOLDER);
    if (root)
    {
      while (count != n || n == 0)
      {
        File file = root.openNextFile();
        if (file)
        {
          if (CheckFile(file))
          {
            count++;
#ifndef DEBUG
            if (n==0)
              seed ^= NameAsSeed();
#endif
          }
          file.close();
        }
        else
          break;  // done
      }
      root.close();
    }
    return count;
  }
  
  void GetFirst()
  {
    // count the images, set the current image to the LAST
    // if not DEBUG, seeds the PRNG
    numberOfFiles = 0;
    if (SD.begin(PIN_SD_CHIP_SELECT) && SD.exists(CFG_IMAGE_FOLDER))
      numberOfFiles = ScanFiles(0);
    haveFile = numberOfFiles != 0;
    previousN = numberOfFiles;
#ifndef DEBUG
    if (seed)
      randomSeed(seed);
#endif
  }

  void GetNext()
  {
    // get a random file
    haveFile = false;
    if (numberOfFiles)
    {
      uint32_t n = random(numberOfFiles) + 1;
      if (n == previousN) // make ONE attempt at avoiding duplicate
        n = random(numberOfFiles) + 1;
      previousN = n;
      haveFile = ScanFiles(n) == n;
    }
  }
#else
  // images cycle in directory order
  void GetFirst()
  {
    // find the first image in the folder
    haveFile = false;
    if (SD.begin(PIN_SD_CHIP_SELECT) && SD.exists(CFG_IMAGE_FOLDER))
    {
      root = SD.open(CFG_IMAGE_FOLDER);
      File file = root.openNextFile();
      CheckFile(file);
    }
  }

  void GetNext()
  {
    // find the next image in the folder
    File file;
    if (haveFile)
    {
      file = root.openNextFile();
      if (!CheckFile(file))
      {
        // start again
        haveFile = false;
        root.close();
        root = SD.open(CFG_IMAGE_FOLDER);
        file = root.openNextFile();
        CheckFile(file);
      }
    }
  }
#endif

  uint32_t ReadDWord(File& file)
  {
    // BMP is little-endian
    uint32_t dw = 0;
    for (uint32_t b = 0; b < 4; b++)
    {
      uint32_t temp = file.read();
      dw |= temp << (8UL*b);
    }
    return dw;
  }
  
  bool ExtractOriginalName(File& file, char* pName)
  {
    // original file name may be appended to .BMP as:
    //  "NAME:=\r\n<name-with-extension> etc\r\n"
    uint32_t sig = 0;
    int idx = 0;
    char name[SLIDE_APPENDED_TEXT_MAX_LEN + 1];
    // Is there an original name appended to the file?
    // go back from EOF by: max len of name, plus start sig size, plus final \r\n, plus a fudge.
    file.seek(file.size() - (SLIDE_APPENDED_TEXT_MAX_LEN + 8 + 2 + 2));
    while (file.peek() != -1)
    {
      sig <<= 8;
      sig |= file.read();
      if (sig == 0x4E414D45UL && ReadDWord(file) == 0x0A0D3D3AUL) // "NAME" and ":=\r\n" (with different byte-order)
      {
        // start found, try building a string
        while (file.peek() != -1)
        {
          char ch = file.read();
          if (::isprint(ch))
          {
            if (idx < SLIDE_APPENDED_TEXT_MAX_LEN)
              name[idx++] = ch;
            else
              break;  // too long
          }
          else if (ch == '\r')
          {
            // found the end, done and good
            name[idx] = '\0';
            strcpy(pName, name);
            return true;
          }
          else
            break; // invalid char
        }
        break;
      }
    }
    return false;
  }

  uint8_t lfsr = 0x55; // anything non-zero
  uint8_t GetLFSR(uint8_t max)
  {
      // returns 0..max-1 in pseudo-random order
      // Taps at 8, 6, 5 & 4. T=255
      while (true)
      {
        uint8_t temp = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 1u;
        lfsr >>= 1;
        lfsr |= (uint8_t)(temp << 7);
        temp = lfsr - 1;
        if (temp < max)
          return temp;
      }
  }

  bool PaintCurrent(uint32_t x, uint32_t y, uint32_t w, uint32_t h, char* pName)
  {
    // draws current BMP into a window x, y, w, h and fills-in pName
    // returns true if successful (valid bmp, no compression, size OK etc)
    // BMP: https://www.ece.ualberta.ca/~elliott/ee552/studentAppNotes/2003_w/misc/bmp_file_format/bmp_file_format.htm
    strcpy(pName, "");
    bool result = false;
    if (haveFile)
    {
      // provide the name of potentially bad file
      strcpy(pName, fileName);
      strcat(pName, "?");
      char name[32];
      strcpy(name, CFG_IMAGE_FOLDER);
      strcat(name, fileName);
      File file = SD.open(name, FILE_READ);
      if (!file)
        return result;

      if (file.read() == 'B' && file.read() == 'M') // BMP Signature
      {
        file.seek(0x000A);
        uint32_t DataOffset = ReadDWord(file);
        // InfoHeader
        uint32_t Size = ReadDWord(file);
        uint32_t Width = ReadDWord(file);
        uint32_t Height = ReadDWord(file);
        uint32_t BPP = ReadDWord(file) >> 16;
        file.seek(0x001E);
        uint32_t Compression = ReadDWord(file); 
        file.seek(0x0036);
        uint32_t Palette0 = ReadDWord(file); 

        if (Size == 40 &&                 // Version 3.x BMP
          (Width == w || Height == h) &&  // must fit in onde direction
#ifdef CFG_GREYSCALE_BITS
          (BPP == 1 || BPP == 4) &&       // mono or 4BPP
#else            
          BPP == 1 &&                     // mono
#endif            
          Compression == 0 &&             // uncompressed
          Palette0 == 0)                  // 0=black
        {
          uint32_t StartRow = 0;
          uint32_t StartCol = 0;
          uint32_t EndCol = Width;
          uint32_t PaintX = x;
          uint32_t PaintWidth = w;
          bool dimsOK = true;
          if (Height == h)
          {
            // height fits, deal with width
            if (Width < w)
            {
              // need whitespace either side
              uint32_t blankCols = (w - Width) / 2;
              PaintX = x + blankCols;
              PaintWidth = Width;
              LCD_FILL_RECT(x, y, blankCols, h, CFG_GAP_FILL_COLOUR); // left
              blankCols = w - Width - blankCols;
              LCD_FILL_RECT(PaintX + Width, y, blankCols, h, CFG_GAP_FILL_COLOUR); // right              
            }
            else if (Width >= w)
            {
              // need to clip either side
              StartCol = (Width - w)/2;
              EndCol = Width - StartCol;
            }
          }
          else if (Width == w)
          {
            // width fits, deal with height
            if (Height >= h) // fits or taller. paint the central strip
              StartRow = (Height - h) / 2;
            else
             dimsOK = false; // won't do a thin strip
          }
          else
            dimsOK = false;  // must fit in one direction

          if (dimsOK)
          {
            uint32_t RowSize = (BPP*Width) / 8;
            if ((BPP*Width) % 8)
              RowSize++; // extra byte
            if (RowSize % 4)
              RowSize += (4 - (RowSize % 4)); // row is multiple of 4 bytes
#ifndef CFG_DISSOLVE
            LCD_BEGIN_FILL(PaintX, y, PaintWidth, h);
#endif            
            if (BPP == 1)
              for (uint32_t rowCtr = 0; rowCtr < h; rowCtr++)
              {
#ifdef CFG_DISSOLVE
                int row = GetLFSR(h);
                LCD_BEGIN_FILL(PaintX, y + row, PaintWidth, 1);
#else
                int row = rowCtr;
#endif  
                // rows are in reverse order
                file.seek(DataOffset + (Height - row - 1 - StartRow) * RowSize);
                uint8_t Mask = 0x80;
                uint8_t Values[4];
                file.read(Values, sizeof(Values)); // read 4 bytes at a time, faster paint
                uint8_t* pValue = Values;
                uint8_t ctr = sizeof(Values);
                for (uint32_t col = 0; col < Width; col++)
                {
                  if (StartCol <= col && col < EndCol)
                  {
                    // not clipped
                    if (*pValue & Mask) // 1 is white, 0 is black 
                      LCD_ONE_WHITE();
                    else
                      LCD_ONE_BLACK();
                  }
                  Mask >>= 1;
                  if (!Mask)
                  {
                    Mask = 0x80;
                    if (--ctr)
                      pValue++;
                    else
                    {
                     file.read(Values, sizeof(Values));
                     pValue = Values;
                     ctr = sizeof(Values);
                    }
                  }
                }
              }
#ifdef CFG_GREYSCALE_BITS
            else
              for (uint32_t rowCtr = 0; rowCtr < h; rowCtr++)
              {
#ifdef CFG_DISSOLVE
                int row = GetLFSR(h);
                LCD_BEGIN_FILL(PaintX, y + row, PaintWidth, 1);
#else
                int row = rowCtr;
#endif  
                // rows are in reverse order
                file.seek(DataOffset + (Height - row - 1 - StartRow) * RowSize);
                bool hi = true;
                uint8_t Values[4];
                file.read(Values, sizeof(Values)); // read 4 bytes at a time, faster paint
                uint8_t* pValue = Values;
                uint8_t ctr = sizeof(Values);
                for (uint32_t col = 0; col < Width; col++)
                {
                  uint8_t component = hi ? (*pValue >> 4) : (*pValue & 0x0F);
  
                  // reduce grey values, 4, 2 or 1
                  uint8_t shift = 4 - CFG_GREYSCALE_BITS;
                  component >>= shift;
                  component <<= shift;
  
                  component = component | (component << 4);
                  LCD_FILL_COLOUR(1, RGB(component, component, component));
                  hi = !hi;
                  if (hi)
                  {
                    if (--ctr)
                      pValue++;
                    else
                    {
                      file.read(Values, sizeof(Values));
                      pValue = Values;
                      ctr = sizeof(Values);
                    }
                  }
                }
              }
#endif
     
#ifdef CFG_READ_IMAGE_NAME          
            if (!ExtractOriginalName(file, pName))
#endif
              strcpy(pName, file.name());
  
#ifndef CFG_SHOW_IMAGE_EXT
            if (*pName && strlen(pName) > 4 && *(pName + strlen(pName) - 4) == '.')
              *(pName + strlen(pName) - 4) = '\0'; // zap the extension
#endif
#ifdef CFG_LOWERCASE_IMAGE_NAME
            char*pCtr = pName;
            while (*pName)
              *pName = ::tolower(*pName++);
#endif
            result = true;
          }
        }
      }
      file.close();
    }
    return result;
  }

  uint32_t NameAsSeed()
  {
    // return the first 4 chars of the current filename as a uint32_t (for entropy)
    return *((uint32_t*)fileName);
  }

};
