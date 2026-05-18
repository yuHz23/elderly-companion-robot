#!/usr/bin/env python3
"""
Generate DXF files for chassis layers from the authoritative specification.

Many laser-cutting shops in VN prefer DXF over SVG. This script produces both
`chassis-layer1.dxf` and `chassis-layer2.dxf` matching the SVG geometry exactly.

Requirements:
    pip install ezdxf

Usage:
    python generate_dxf.py

Output:
    chassis-layer1.dxf  (top layer)
    chassis-layer2.dxf  (bottom layer)

The geometry is defined in code (single source of truth alongside the SVG).
Edit the constants below if specs change — then regenerate both DXF and SVG
to keep them in sync.
"""

from pathlib import Path

try:
    import ezdxf
except ImportError:
    raise SystemExit("ezdxf not installed. Run: pip install ezdxf")


# ----------------------------------------------------------------------
# Geometry constants (matches chassis-spec.md)
# ----------------------------------------------------------------------

SHEET_W = 200.0          # mm
SHEET_H = 150.0          # mm

M3_RADIUS = 1.6          # M3 clearance hole (dia 3.2)
M2_RADIUS = 1.1          # M2 clearance hole (dia 2.2)

# 6 standoff positions — IDENTICAL on both layers
STANDOFFS = [
    (10,  10),  (100, 10),  (190, 10),
    (10, 140),  (100, 140), (190, 140),
]

# ----------------------------------------------------------------------
# Layer 1 (TOP) features
# ----------------------------------------------------------------------

# PTZ servo body cutout (SG90 22x12mm at front-center)
PTZ_BODY = (89, 25, 22, 12)            # (x, y, w, h)
PTZ_FLANGE_HOLES = [(86, 31), (114, 31)]   # M2

# Main PCB mount (72x52mm pattern)
PCB_HOLES = [(64, 54), (136, 54), (64, 106), (136, 106)]   # M3

# Cable pass-through slot (20x10mm with 2mm rounded corners)
CABLE_SLOT = (15, 125, 20, 10, 2)      # (x, y, w, h, radius)

# ----------------------------------------------------------------------
# Layer 2 (BOTTOM) features
# ----------------------------------------------------------------------

# Outer outline as polyline (with wheel notches on left/right at Y=58..92)
OUTLINE_PATH = [
    (0, 0), (200, 0),
    (200, 58), (192, 58), (192, 92), (200, 92),
    (200, 150), (0, 150),
    (0, 92), (8, 92), (8, 58), (0, 58),
    (0, 0),
]

# BO motor bracket holes (2 per side, 20mm vertical spacing)
MOTOR_HOLES = [
    (14, 65),  (14, 85),     # left motor
    (186, 65), (186, 85),    # right motor
]

# Caster wheel mount (25mm square pattern, front-center)
CASTER_HOLES = [(88, 20), (113, 20), (88, 45), (113, 45)]

# 18650 battery holder (70x65mm pattern, center)
BATTERY_HOLES = [(65, 58), (135, 58), (65, 123), (135, 123)]

# ----------------------------------------------------------------------
# DXF helpers
# ----------------------------------------------------------------------

def new_doc():
    """Create new DXF doc with red 'CUT' layer (laser convention)."""
    doc = ezdxf.new("R2010", units=ezdxf.units.MM)
    doc.layers.add(name="CUT", color=1)        # AutoCAD color 1 = red
    return doc, doc.modelspace()


def add_circle(msp, cx, cy, r):
    msp.add_circle((cx, cy), r, dxfattribs={"layer": "CUT"})


def add_rect(msp, x, y, w, h):
    """Sharp-corner rectangle as closed polyline."""
    pts = [(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y)]
    msp.add_lwpolyline(pts, dxfattribs={"layer": "CUT"}, close=True)


def add_rounded_rect(msp, x, y, w, h, r):
    """Rounded rectangle — 4 arcs + 4 lines."""
    if r <= 0:
        add_rect(msp, x, y, w, h)
        return
    # Lines (clockwise from top-left)
    msp.add_line((x + r, y),         (x + w - r, y),     dxfattribs={"layer": "CUT"})
    msp.add_line((x + w, y + r),     (x + w, y + h - r), dxfattribs={"layer": "CUT"})
    msp.add_line((x + w - r, y + h), (x + r, y + h),     dxfattribs={"layer": "CUT"})
    msp.add_line((x, y + h - r),     (x, y + r),         dxfattribs={"layer": "CUT"})
    # Arcs (counter-clockwise convention in DXF; specify start/end angles)
    msp.add_arc(center=(x + r,     y + r),     radius=r, start_angle=180, end_angle=270, dxfattribs={"layer": "CUT"})
    msp.add_arc(center=(x + w - r, y + r),     radius=r, start_angle=270, end_angle=360, dxfattribs={"layer": "CUT"})
    msp.add_arc(center=(x + w - r, y + h - r), radius=r, start_angle=0,   end_angle=90,  dxfattribs={"layer": "CUT"})
    msp.add_arc(center=(x + r,     y + h - r), radius=r, start_angle=90,  end_angle=180, dxfattribs={"layer": "CUT"})


def add_polyline(msp, pts):
    msp.add_lwpolyline(pts, dxfattribs={"layer": "CUT"}, close=False)


# ----------------------------------------------------------------------
# Builders
# ----------------------------------------------------------------------

def build_layer1():
    doc, msp = new_doc()

    # Outer border (simple rectangle)
    add_rect(msp, 0, 0, SHEET_W, SHEET_H)

    # Standoffs
    for x, y in STANDOFFS:
        add_circle(msp, x, y, M3_RADIUS)

    # PTZ
    x, y, w, h = PTZ_BODY
    add_rect(msp, x, y, w, h)
    for cx, cy in PTZ_FLANGE_HOLES:
        add_circle(msp, cx, cy, M2_RADIUS)

    # Main PCB mount
    for cx, cy in PCB_HOLES:
        add_circle(msp, cx, cy, M3_RADIUS)

    # Cable slot
    add_rounded_rect(msp, *CABLE_SLOT)

    return doc


def build_layer2():
    doc, msp = new_doc()

    # Outer outline with wheel notches
    add_polyline(msp, OUTLINE_PATH)

    # Standoffs (same positions as layer 1)
    for x, y in STANDOFFS:
        add_circle(msp, x, y, M3_RADIUS)

    # Motor bracket holes
    for cx, cy in MOTOR_HOLES:
        add_circle(msp, cx, cy, M3_RADIUS)

    # Caster wheel mount
    for cx, cy in CASTER_HOLES:
        add_circle(msp, cx, cy, M3_RADIUS)

    # Battery holder mount
    for cx, cy in BATTERY_HOLES:
        add_circle(msp, cx, cy, M3_RADIUS)

    # Cable slot (matches layer 1)
    add_rounded_rect(msp, *CABLE_SLOT)

    return doc


# ----------------------------------------------------------------------
# Entry point
# ----------------------------------------------------------------------

def main():
    out_dir = Path(__file__).parent

    layer1 = build_layer1()
    layer1_path = out_dir / "chassis-layer1.dxf"
    layer1.saveas(layer1_path)
    print(f"  wrote {layer1_path.name}")

    layer2 = build_layer2()
    layer2_path = out_dir / "chassis-layer2.dxf"
    layer2.saveas(layer2_path)
    print(f"  wrote {layer2_path.name}")

    print("\nDone. Send these DXF files to your laser-cutting shop.")
    print("Material: 3mm acrylic, 200x150mm per layer.")


if __name__ == "__main__":
    main()
