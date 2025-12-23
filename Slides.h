#pragma once

// Image file IO and rendering
#define SLIDE_APPENDED_TEXT_MAX_LEN 40 // Max len of appended string. See make_slides.bat

namespace Slides
{
  void GetFirst();
  void GetNext();
  bool PaintCurrent(uint32_t x, uint32_t y, uint32_t w, uint32_t h, char* pName);
  uint32_t NameAsSeed();
};
