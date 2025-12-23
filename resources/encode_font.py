#!/usr/bin/python
import os
import sys

# encode characters from .bdf file
# not row-compressed, cf encode_windows

# The <command> char isn't in the file, but is nice to have!
extras = """
  // (not in font file)
  129,  // <command>
  0xA9, 0x00,
  0b0110001, 0b10000000,
  0b1001010, 0b01000000,
  0b1001010, 0b01000000,
  0b0111111, 0b10000000,
  0b0001010, 0b00000000,
  0b0111111, 0b10000000,
  0b1001010, 0b01000000,
  0b1001010, 0b01000000,
  0b0110001, 0b10000000,
"""

def DataStr(hex):
    # hex is nn or nnnn, return a 1 or 2 byte string
    if len(hex) == 2:
        return "0x" + hex + ", "
    else:
        return DataStr(hex[:2]) + DataStr(hex[2:])

def ByteStr(b):
    return "0x" + hex(256 + b)[3:].upper()
    
def UPack(a, b):
    # 0baaaabbbb
    return ByteStr(((a % 16) << 4) + (b % 16))

def SPack(a, b):
    # 0bsaaasbbb s is the sign, 1=-ve
    neg = a < 0
    a = abs(a) % 8
    if neg:
        a += 8
        
    neg = b < 0
    b = abs(b) % 8
    if neg:
        b += 8
    return UPack(a, b)
    
bdf = open("Chicago-12.bdf", "r")
lines = bdf.readlines()
file = open("FontData.h", "w")
file.write("// (created from \"" + bdf.name + "\" by " + os.path.basename(__file__) + ")\n")
line = 0
count = 1
file.write("#define FONT_HEIGHT    12\n")
file.write("#define FONT_DESCENDER  3\n")
file.write("static const uint8_t Font[] PROGMEM =\n{\n")
file.write("  // char\n")
#file.write("  // w, h, dx, dy\n")
file.write("  // (w, h):0bwwwwhhhh, (dx, dy):0bsxxxsyyy s:sign, 1=-ve\n")
file.write("  // 1BPP row data...\n")
while not lines[line].startswith("ENDFONT"):
    while not lines[line].startswith("ENCODING"):
        line += 1
    if lines[line].startswith("ENCODING"):
        ch = int(lines[line].split(" ")[1])
        line += 1
        if (32 < ch and ch < 127) or ch == 63743 or ch == 8226:
            # process char
            if ch == 8226:  # bullet
                ch = 127
                file.write("\n")
            if ch == 63743: # apple
                ch = 128
            line += 2
            # BBX
            BBX = lines[line].split(" ")
            w = int(BBX[1])
            h = int(BBX[2])
            dx = int(BBX[3])
            dy = int(BBX[4])
            #sys.stdout.write(str(ch) + " " + str(BBX) + "\n") 
            file.write("  " + str(ch) + ",")
            if ch == 127:
                file.write("  // <bullet>")
            elif ch == 128:
                file.write("  // <apple>")
            elif ch != '\\' and ch != '/':
                file.write("  // '" + str(chr(ch)) + "'")
            file.write("\n")
            #file.write("  " + str(w) + ", " + str(h) + ", " + str(dx) + ", " + str(dy) + ",\n") # todo back as 0xWH, 0xSxxxSyyyy
            file.write("  " + UPack(w, h) + ", " + SPack(dx, dy) + ",\n") # todo back as 0xWH, 0xSxxxSyyyy
            #count += 5
            count += 3
            count += h if w <= 8 else 2*h
            line += 2
            file.write("  ")
            while not lines[line].startswith("ENDCHAR"):
                file.write(DataStr(lines[line].strip()))
                line += 1
            file.write("\n")
        else:
            while not lines[line].startswith("ENDCHAR"):
                line += 1
        line += 1
file.write(extras)
count += extras.count(",")  
file.write("  0\n")
file.write("}; // " + str(count) + " bytes\n\n")
file.close()
bdf.close()
sys.stdout.write("Created " + file.name + "\n\n")
