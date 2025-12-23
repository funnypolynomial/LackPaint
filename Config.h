#pragma once

// Configuration of many things!

// If defined, Serial output, no splash, etc
//#define DEBUG

// Defines path to images, use "/" for root.
#define CFG_IMAGE_FOLDER      "SLIDES/"

// Defines interval between images.
// This is the *minimum*. The interval may be longer if slides are random.
#define CFG_SECONDS_BETWEEN_IMAGES 10

// If defined, images are shown in random order.  
// This adds an additional delay between images as the directory is scanned, ~40ms/file?
// If not defined, images cycle in directory listing order (saves ~320 program storage bytes).
#define CFG_RANDOM_ORDER

// If defined, uses appended file name instead of 8.3, if found.
#define CFG_READ_IMAGE_NAME

// If defined, filenames are show with trailing .BMP/JPG extension, otherwise it is stripped.
//#define CFG_SHOW_IMAGE_EXT

// If defined, filenames are show lowercase.
//#define CFG_LOWERCASE_IMAGE_NAME

// If defined, greyscale images are supported. 
// File must use 4-bit, but code can show 4, 2 or 1 bit.
// Greyscale is not as consistent with the look of the rest of the display, but does give better images.
// Undefining to remove support saves ~440 program storage bytes.
//#define CFG_GREYSCALE_BITS  4

// Colour used to fill empty gaps either side of slide, LCD_BLACK or LCD_WHITE.
#define CFG_GAP_FILL_COLOUR LCD_WHITE

// If defined, image rows are drawn in pseudo-random order, otherwise top-down (off saves ~220 program storage bytes).
#define CFG_DISSOLVE

// If defined, LCD supports touch and touching pauses/resumes/randomises slides (off saves ~1000 program storage bytes).
#define CFG_LCD_HAS_TOUCH

// If defined, LCD is Landscape, Uno USB on the left, else on the right.
#define CFG_LCD_USB_LEFT

// If defined, splash includes icon and help line (off saves ~450 program storage bytes).
#define CFG_FULL_SPLASH

// If defined, a small disk or watch icon is shown on the far right of the menu bar when
// scanning the SD card for the next slide, or while painting the slide, respectively.
#define CFG_SHOW_BUSY_ICONS

// If defined, draws a small red cross at computed touch point. 
// *** Don't expect perfect correlation! ***
//#define CFG_SHOW_TOUCH

// If defined, reports raw touch values to Serial at 9600. Use to update TOUCH_ defines in LCD.cpp. Requires DEBUG
//#define CFG_TOUCH_CALIB
