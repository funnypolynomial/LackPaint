#!/usr/bin/python
import os
import sys
import math
from PIL import Image, ImageDraw # https://pillow.readthedocs.io/en/stable/

# encode regions in the MacPaint reference PNG
# 1 bit per pixel, most significant bit is leftmost pixel, 1=white
# areas inside frames, frames are drawn in code
# duplicate rows compressed as a single byte
# 0b1010nnnn x=1, repeat row nnn, x=0, repeat row (nnn + 1)th prev full row
# saves ~440 program storage bytes
# It's mostly about compressing the toolbar

defines = """// duplicate row encoding:
#define BMP_ROW_ESC_MASK        0b11110000 // mask for escape byte
#define BMP_ROW_ESC_VALUE       0b10100000 // escape byte
#define BMP_ROW_ESC_IDX         0b00001111 // index to recent or initial
#define BMP_ROW_ESC_IDX_BLACK   0b00001000 // index to black row

"""

BMP_ROW_ESC_MASK   = 0b11110000 # mask for escape byte
BMP_ROW_ESC_VALUE  = 0b10100000 # escape byte, no row starts with 0xAx
BMP_ROW_ESC_IDX    = 0b00001111 # index to recent or initial
BMP_ROW_ESC_IDX_BLACK = 0b00001000

def ByteStr(b):
    return "0x" + hex(256 + b)[3:].upper()

def HiLoStr(b):
    return str(b / 256) +"," + str(b % 256)


def EncodeRegion(x, y, w, h, name):
    # encode the region as a PROGMEM array called name
    count = 0
    if w == 0 and h == 0:
        # whole image, don't write x, y etc
        w = image.width
        h = image.height
        file.write("static const uint8_t p" + name + "Image[] PROGMEM =\n{\n")
    else:
        # a region
        file.write("static const uint8_t p" + name + "Data[] PROGMEM =\n{\n")
        file.write("  " + HiLoStr(x) + ", " + HiLoStr(y) + ", " + HiLoStr(w) + ", " + HiLoStr(h) + ",  // xHi,xLo, yHi,yLo, wHi,wLo, hHi,hLo,\n")
        count = 8
    byte = 0
    bit = 0
    prevFullRows = [""]*(BMP_ROW_ESC_IDX+1) # 8x prev, 8x first
    initialFullRowsIdx = (BMP_ROW_ESC_IDX + 1)/2
    prevFullRows[initialFullRowsIdx] = "<all black>"
    initialFullRowsIdx += 1
    recentFullRowsIdx = 0    
    for row in range(h):
        rowStr = "  "
        allBlacks = True
        for col in range(w):
            byte <<= 1
            if image.getpixel((x + col, y + row)) == 1:
                byte += 1
                allBlacks = False
            bit += 1
            if bit == 8:
                rowStr += ByteStr(byte) + ", "
                bit = 0
                byte = 0
        if bit != 0: # partial at end
          byte <<= 8 - bit
          rowStr += ByteStr(byte) + ","
          bit = 0
          byte = 0
        
        if allBlacks:
          file.write("  " + ByteStr(BMP_ROW_ESC_VALUE | BMP_ROW_ESC_IDX_BLACK) + ",\n")
          count += 1
        else:
          if rowStr in prevFullRows:
            file.write("  " + ByteStr(BMP_ROW_ESC_VALUE | prevFullRows.index(rowStr)) + ",\n")
            count += 1
          else:
            # unique full row
            file.write(rowStr)
            file.write("\n")
            count += rowStr.count(',')
            if initialFullRowsIdx <= BMP_ROW_ESC_IDX:
              prevFullRows[initialFullRowsIdx] = rowStr
              initialFullRowsIdx += 1
            prevFullRows[recentFullRowsIdx] = rowStr
            recentFullRowsIdx += 1
            recentFullRowsIdx %= (BMP_ROW_ESC_IDX + 1)/2
    global grand_total
    grand_total += count
    file.write("}; // " + str(count) + " bytes\n\n")

image = Image.open("MacPaint 480x320.png")
file = open("GraphicsData.h", "w")
file.write("// (created from \"" + image.filename + "\" by " + os.path.basename(__file__) + ")\n")
file.write(defines)
grand_total = 0
EncodeRegion(10, 29, 51, 197, "Tools")
EncodeRegion(72, 279, 401, 33, "Fills")
EncodeRegion(13, 254, 12, 9, "Tick")
# EncodeRegion(26, 248, 29, 53, "Lines") nope, done in code

# and some icons:
# App
image.close()
image = Image.open("Icon.png")
file.write("// (created from \"" + image.filename + "\")\n")
EncodeRegion(0, 0, 0, 0, "Icon")

# Busy
image.close()
image = Image.open("Busy.png")
file.write("// (created from \"" + image.filename + "\")\n")
EncodeRegion(0, 0, 0, 0, "Busy")

# Disk
image.close()
image = Image.open("Disk.png")
file.write("// (created from \"" + image.filename + "\")\n")
EncodeRegion(0, 0, 0, 0, "Disk")

file.write("// total " + str(grand_total) + " bytes\n\n")
file.close()
sys.stdout.write("Created " + file.name + "\n")
