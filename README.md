# LackPaint
A photoframe/slideshow in the style of _MacPaint_ from 1981. Arduino Uno plus 480x320 LCD plus microSD card. A script to convert JPGs to monochrome dithered BMPs.

<img width="1824" height="1368" alt="image" src="https://github.com/user-attachments/assets/d9711936-674b-4e6f-b778-a5dba2212f4f" />
<img width="1824" height="1368" alt="image" src="https://github.com/user-attachments/assets/667fb6da-0930-4f9e-8658-78c0a11318ba" />

YouTube video: https://youtu.be/0k75EQjjzhA  
Flickr album: https://flic.kr/s/aHBqjCEDmW  
Hackaday: https://hackaday.io/project/204744-lackpaint

---
**What**:
 A photoframe in the style of v1.0 of MacPaint (1981) https://en.wikipedia.org/wiki/MacPaint
 It uses a Arduino Uno and 480x320 LCD shield with built-in SD card support.  
 Files are loaded from a directory on a MicroSD card on the LCD shield and displayed 
 sequentially or randomly.
 The images on the card MUST be
   * uncompressed bitmaps (.BMP), and either
   * monochrome (1 bit-per-pixel) OR
   * 4-bit greyscale.
   * sized to fit the display window's width (396 pixels) and/or height (218 pixels).
 By default they are pulled from the SLIDES folder in the SD card (but see CFG_IMAGE_FOLDER).
  
 The look is based closely on the original MacPaint but adjusted to work with the LCD's aspect
 ratio.  I have reduced the original's dimensions by removing 
   * a column of fill styles (second from right) and 
   * a row of tools (second from bottom)
 Refer to the "resources" subdirectory: 
   * "MacPaint Original.png" is the reference image,
   * "MacPaint 480x320.png" is the adjusted image.
 The resources directory includes two Python scripts:
   * "encode_windows.py"
     This encodes regions in "MacPaint 480x320.png" (and "Icon.png", "Busy.png" & Disk.png) as 
     1-bpp PROGMEM arrays written to "GraphicsData.h" which is copied up a level and #included 
     into App.cpp. This is  how the parts of the screen (tools, fills) are rendered 
     (see DrawOneBPPData()).
     It uses the PIL library, https://pillow.readthedocs.io/en/stable/
     There is a simple duplicate-row compression scheme.
   * "encode_font.py"
     This scans "Chicago-12.bdf" and builds a single 1-bpp PROGMEM array of the character pixels
     in "FontData.h" which is copied up a level and #included into App.cpp.

**Slides**:
 Refer to the "slides" subdirectory.
 I have automated the process of taking a directory full of JPGs (eg photos) and converting them
 to the format required.  The JPGs are copied as appropriately resized uncompressed bitmaps in a 
 specified folder.  They are converted from colour to either 
   * monochrome images dithered with a 3x3 tile OR
   * 4-bit greyscale images
 This is achieved by the "make_slides.bat" file and by using *ImageMagick* to make the image edits.
 ImageMagick MUST be installed, and ideally be on the path (but see "magick_exe" in the batch file).
 The batch file can be configured in several ways, how to size-to-fit Landscape images, whether to 
 include Portrait images etc.
 Note that dithered monochrome looks better, is more in keeping with the MacPaint aesthetic, but 
 greyscale does produce more realistic images.

 The SD library interface is "8.3", meaning that photos with meaningful filenames will be called things
 like "WEDDIN~1.BMP". To counter this, "make_slides.bat" can optionally append to the .BMP
   * the original file name and/or
   * image date information (from EXIF)
 The sketch will use this text, if found, to caption the slides instead of the raw file name.

**Random display**:
 Displaying the files randomly (CFG_RANDOM_ORDER) incurs a cost as the directory is scanned
 to the Nth file, this may take some time (500 files in ~40seconds, YMMV) a "disk" icons is
 shown on the far right of the menu bar.

**Configuration**:
 Config.h has a number of defines to control the sketch behavour, for example CFG_IMAGE_FOLDER
 sets the location of the slides, CFG_SECONDS_BETWEEN_IMAGES sets the seconds between images, etc, etc.

**Touch**:
 There is basic touch support (if applicable, see CFG_LCD_HAS_TOUCH). 
 Touching in the image pauses or resumes the slideshow. 
 Touching elsewhere randomizes the slides (if applicable, see CFG_RANDOM_ORDER). The PRNG is seeded
 from millis(), touch position and current image file name.
 Touch is only checked when idle -- when there is no busy icon on the menu bar's far right.

**Libraries**:
 Because speed is not really a consideration, I've used an external library to deal with the LCD.
 The interface the code deals with is my usual "define a window, send pixels to it" but implemented
 (a little awkwardly) with the MCUFRIEND_kbv library.  The library can auto-detect the LCD but I've 
 hard-wired mine (to save space. try running CUFRIEND_kbv sample diagnose_TFT_support).  
 The library compiles with warnings when Compiler warnings:All is set.
 I use the SD library for reading the MicroSD card, it also compiles with warnings when 
 All is set.
 The libraries bulk out the size of the sketch to ~98% of program storage space.

**Requirements**:  
 Arduino Uno  
 MicroSD card:               I've used a no-name "6GB U3 Class 10 MicroSD" and a Verbatum "16GB Class 10 MicroSDHC". YMMV.  
 SD adaptor/reader:          I used https://www.jaycar.co.nz/usb-micro-sd-card-reader/p/XC4740  
 480x320 LCD Shield:         https://www.waveshare.com/4inch-tft-touch-shield.htm (or similar! YMMV!)  
 The MCUFRIEND_kbv library:  https://docs.arduino.cc/libraries/mcufriend_kbv/  
 The SD library:             https://docs.arduino.cc/libraries/sd/  
 ImageMagick:                https://imagemagick.org/  
If rebuilding resources  
 Python & PIL                https://pillow.readthedocs.io/en/stable/  

**Links**:  
 https://archive.org/details/mac_Paint_2  
 https://en.wikipedia.org/wiki/MacPaint  
 https://github.com/danfe/fonts  

Mark Wilson. Dec 2025
