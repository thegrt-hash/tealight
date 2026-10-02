#!/usr/bin/env python3
"""Parametric enclosure for the mini color-changing LED tealight.

Generates two printable parts:
  * tealight_body.stl  -- cylindrical tub holding the XIAO ESP32-C3 + LiPo,
                          with a USB-C charging slot and a side button hole.
  * tealight_lid.stl   -- press-on cap with a central window + diffuser ledge
                          for the WS2812 "flame".

Everything is driven by the PARAMS block below -- tweak and re-run:
    /tmp/pio-venv/bin/python enclosure/tealight_case.py
(requires `cadquery`).

Fit philosophy: open cavity, no fragile internal posts. Secure the battery and
board with a dab of foam tape / hot glue. All clearances are generous; adjust
if your parts differ.
"""
import cadquery as cq

# ---------------------------------------------------------------- PARAMS (mm)
WALL        = 2.0     # side wall thickness
FLOOR_TH    = 2.0     # bottom thickness
OUTER_D     = 46.0    # outer diameter (real tealight ~38; bumped to fit parts)
BODY_H      = 28.0    # total body height (floor + cavity)

# Lid (sits on the rim; an inner plug locates it in the cavity)
PLUG_H      = 5.0     # how far the lid's locating plug drops into the body
PLUG_WALL   = 1.6     # thickness of that plug ring
PLUG_CLEAR  = 0.4     # radial clearance so the lid isn't a hammer-fit
LID_TH      = 2.0     # lid top thickness
WINDOW_D    = 22.0    # central LED/diffuser opening
DIFFUSER_D  = 26.0    # recess diameter so a diffuser disc rests on a ledge
DIFFUSER_REC= 1.2     # depth of that ledge

# USB-C charging slot (aligned to the XIAO's USB-C connector)
USB_W       = 11.0    # slot width
USB_H       = 6.0     # slot height
USB_Z       = 12.5    # vertical center of the slot above the floor
USB_FILLET  = 1.5     # rounds the slot corners

# Side button hole (for a panel-mount momentary; see README parts list)
BTN_D       = 7.0     # hole diameter (7mm = common mini momentary bezel)
BTN_Z       = 15.0    # vertical center above the floor
BTN_ANGLE   = 90.0    # degrees around from the USB slot (USB is at +Y)

inner_d = OUTER_D - 2 * WALL

# ------------------------------------------------------------------- BODY
body = cq.Workplane("XY").circle(OUTER_D / 2).extrude(BODY_H)

# hollow out the cavity (open at the top)
cavity = (cq.Workplane("XY").workplane(offset=FLOOR_TH)
          .circle(inner_d / 2).extrude(BODY_H))
body = body.cut(cavity)

# Build the USB slot as a box so corner fillets are easy, then pierce +Y wall.
usb_box = (cq.Workplane("XY")
           .box(USB_W, 2 * WALL + 2, USB_H)
           .edges("|Y").fillet(USB_FILLET)
           .translate((0, OUTER_D / 2 - WALL, USB_Z)))
body = body.cut(usb_box)

# Button hole on the wall, BTN_ANGLE around from the USB slot
import math
bx = (OUTER_D / 2 - WALL) * math.sin(math.radians(BTN_ANGLE))
by = (OUTER_D / 2 - WALL) * math.cos(math.radians(BTN_ANGLE))
btn_cut = (cq.Workplane("XY")
           .cylinder(2 * WALL + 2, BTN_D / 2, direct=(bx, by, 0), centered=True)
           .translate((bx, by, BTN_Z)))
body = body.cut(btn_cut)

# ------------------------------------------------------------------- LID
lid = cq.Workplane("XY").circle(OUTER_D / 2).extrude(LID_TH)
# downward locating plug that drops INTO the body cavity
plug_outer = inner_d / 2 - PLUG_CLEAR
plug = (cq.Workplane("XY")
        .circle(plug_outer).circle(plug_outer - PLUG_WALL)
        .extrude(-PLUG_H))
lid = lid.union(plug)
# central through-window for the LED/diffuser
lid = lid.cut(cq.Workplane("XY").circle(WINDOW_D / 2).extrude(LID_TH))
# top recess ledge so a diffuser disc sits flush
lid = lid.cut(cq.Workplane("XY").workplane(offset=LID_TH - DIFFUSER_REC)
              .circle(DIFFUSER_D / 2).extrude(DIFFUSER_REC))

# --------------------------------------------------------------- EXPORT
import os
here = os.path.dirname(os.path.abspath(__file__))
cq.exporters.export(body, os.path.join(here, "tealight_body.stl"))
cq.exporters.export(lid,  os.path.join(here, "tealight_lid.stl"))
print("wrote tealight_body.stl and tealight_lid.stl")
