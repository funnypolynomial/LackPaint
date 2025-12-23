MacPaint 480x320.png:
  Edited version of "MacPaint Original.png" by removing a column of fill styles (second from right) and a row of tools (second from bottom)

Icon.png
  MacPaint icon. Used in Splash

Busy.png, Disk.png
  Mac wait/busy icons.

encode_graphics.py:
  Extracts pixel regions (eg tool palette) and encodes them as 1BPP PROGMEM data
  Uses PIL library, https://pillow.readthedocs.io/en/stable/

(GraphicsData.h:
  1BPP PROGMEM data. Output from encode_graphics.py. Copy up to sketch directory.)

Chicago-12.bdf:
  Full font definition

encode_font.py:
  Encodes chars from the .bdf as PROGMEM data

(FontData.h:
  Font PROGMEM data. Output from encode_font.py. Copy up to sketch directory.)