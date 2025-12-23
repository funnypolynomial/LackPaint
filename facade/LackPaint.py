#! /usr/bin/env python
'''
Simple monitor-like facade for Uno+touch LCD 480x320
* a front face
* spacers, glued to front plate, with nylon m3 hex standoffs glued facing back
* a back clamp plate
* small square pads with hex holes to optionally reinforce glues standoffs

3mm black Satin

NOTE: inksnek.setup() is inksnek.DEVEL, switch to FINAL for cutting

'''

import inkex
import simplestyle, sys, copy
from math import *
from inksnek import *
import operator

# plates:
FRONT_PLAIN, MIDDLE_SPACER, BACK_CLAMP  = 0,1,2

class MyDesign(inkex.Effect):
  def __init__(self):
    inkex.Effect.__init__(self)

  def addStandoff(self, group, x, y, plate):
    # add/show hex cutout and m2 hole
    holeStyle = inksnek.cut_style if plate == BACK_CLAMP else inksnek.ignore_style
    hexStyle = inksnek.ignore_style if plate == BACK_CLAMP else inksnek.cut_style
    holeDiameter = 3.0 #if plate == 1 else 3.3 # add some slack? -- no
    inksnek.add_circle(group, x, y, holeDiameter/2.0, holeStyle)
    hex = inksnek.add_group(group, inksnek.translate_group(x, y))
    path = inksnek.path_move_to(inksnek.polar_to_rectangular(self.hex_radius, 30))
    for a in range(6):
        path += inksnek.path_line_to(inksnek.polar_to_rectangular(self.hex_radius, 30 + a*60))
    path += inksnek.path_close()
    inksnek.add_path(hex, path, hexStyle)
  
  def addMiddleSpacers(self, group, x, y, w, h):
    group = inksnek.add_group(group, inksnek.translate_group(x, y))
    inksnek.add_round_rect(group, 0, 0, w, h, self.surroundRadius, inksnek.cut_style)
    inksnek.add_rect(group, self.surroundWidth, self.surroundHeightBottom, w - 2.0*self.surroundWidth, h - self.surroundHeightBottom - self.surroundHeightTop, inksnek.cut_style) # lcd cutout
    # divide into 4
    inksnek.add_line_by(group, 0.0, h/2, self.surroundWidth, 0.0, inksnek.cut_style)
    inksnek.add_line_by(group, w, h/2, -self.surroundWidth, 0.0, inksnek.cut_style)
    inksnek.add_line_by(group, w/2, 0, 0, self.surroundHeightBottom, inksnek.cut_style)
    inksnek.add_line_by(group, w/2, h, 0, -self.surroundHeightTop, inksnek.cut_style)
    # holes for standoffs 15mm, diameter 5.48 (flat-flat) 6.27mm (corner-corner)
    plate = MIDDLE_SPACER
    lowerY = self.surroundHeightBottom - self.surroundHeightTop/2.0 # closer to LCD/Uno, symmetric
    self.addStandoff(group, self.surroundWidth, lowerY, plate)
    self.addStandoff(group, w - self.surroundWidth, lowerY, plate)
    self.addStandoff(group, self.surroundWidth, h - self.surroundHeightTop/2.0, plate)
    self.addStandoff(group, w - self.surroundWidth, h - self.surroundHeightTop/2.0, plate)    
      
  def addPlate(self, group, x, y, plate):
    # add the given plate
    group = inksnek.add_group(group, inksnek.translate_group(x, y))
    inksnek.add_round_rect(group, 0, 0, self.totalWidth, self.totalHeight, self.surroundRadius, inksnek.cut_style)
    if plate != BACK_CLAMP:
        inksnek.add_rect(group, self.surroundWidth, self.surroundHeightBottom, self.lcdWidth, self.lcdHeight, inksnek.cut_style) # lcd cutout
    if plate != FRONT_PLAIN:
        # holes for standoffs 15mm, diameter 5.48 (flat-flat) 6.27mm (corner-corner)
        lowerY = self.surroundHeightBottom - self.surroundHeightTop/2.0 # closer to LCD/Uno, symmetric
        self.addStandoff(group, self.surroundWidth, lowerY, plate)
        self.addStandoff(group, self.totalWidth - self.surroundWidth, lowerY, plate)
        self.addStandoff(group, self.surroundWidth, self.totalHeight - self.surroundHeightTop/2.0, plate)
        self.addStandoff(group, self.totalWidth - self.surroundWidth, self.totalHeight - self.surroundHeightTop/2.0, plate)
    if plate == BACK_CLAMP:
        # notch for power at right-angles
        path = inksnek.path_move_to(0, self.surroundHeightBottom + self.powerOffsetY - self.powerR1 - self.powerR2)
        path += inksnek.path_round_by(+self.powerR2, +self.powerR2, self.powerR2)
        path += inksnek.path_horz_by(self.surroundHeightBottom - self.powerR1 - self.powerR2)
        path += inksnek.path_round_by(+self.powerR1, +self.powerR1, -self.powerR1)
        path += inksnek.path_round_by(-self.powerR1, +self.powerR1, -self.powerR1)
        path += inksnek.path_horz_by(-(self.surroundHeightBottom - self.powerR1 - self.powerR2))
        path += inksnek.path_round_by(-self.powerR2, +self.powerR2, self.powerR2)
        inksnek.add_path(group, path, inksnek.cut_style)
    if plate == FRONT_PLAIN:
        self.addMiddleSpacers(group, self.surroundWidth+1, self.surroundHeightBottom +1, self.lcdWidth -2, self.lcdHeight-2)
        # standoff reinforcements
        gap = 3.0
        w = 20.0
        d = 2.0*self.hex_radius + gap
        for pad in range(4):
            g = inksnek.add_group(group, inksnek.translate_group(self.surroundWidth + w + (pad + 1)*d, self.surroundHeightBottom + self.lcdHeight/2))
            inksnek.add_rect(g, -d/2.0, -d/2.0, d, d - 1.1, inksnek.cut_style, "LTB" if pad != 3 else "LTRB")
            self.addStandoff(g, 0, 0, MIDDLE_SPACER)
        # pcb outline
        if inksnek.mode == inksnek.DEVEL:
            inksnek.add_rect(group, self.surroundWidth + self.pcbOffsetX, self.surroundHeightBottom + self.pcbOffsetY, self.pcbWidth, self.pcbHeight, inksnek.ignore_style)
        
          
  def effect(self):
    inksnek.setup(self, inksnek.A4, inksnek.ACRYLIC, 3.0, 'mm', inksnek.DEVEL) ## switch to FINAL to cut

    self.lcdWidth = 85.0
    self.lcdHeight = 55.0
    self.surroundWidth = 10.0
    self.surroundHeightTop = 10.0
    self.surroundHeightBottom = self.surroundHeightTop
    self.surroundRadius = 5.0
    self.totalWidth = self.lcdWidth + 2.0*self.surroundWidth
    self.totalHeight = self.lcdHeight + self.surroundHeightBottom + self.surroundHeightTop
    self.powerOffsetY = 8.0 # from LCD corner to middle of notch
    self.powerR1 = 6.0
    self.powerR2 = 2.0
    self.hex_radius = 6.3/2.0
    self.pcbWidth = 88.0
    self.pcbHeight = 59.0
    # bottom-left corner of PCB from corner of LCD
    self.pcbOffsetX = -1.0
    self.pcbOffsetY = -1.5
    design = inksnek.add_group(inksnek.top_group, inksnek.translate_group(10, 10))
    self.addPlate(design, 0, 0, FRONT_PLAIN)
    self.addPlate(design, 0, self.totalHeight + 2.0, BACK_CLAMP)
