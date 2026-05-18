#!/usr/bin/env python3
"""
Power budget calculator for Elderly Companion Robot.

Defines every load attached to each voltage rail, computes:
- Typical and peak current per rail
- Total power per rail (typ/peak)
- Battery runtime estimate
- Headroom vs regulator rating

Single source of truth — edit LOADS below when adding a component, rerun:

    python power_budget.py

Output is a markdown table that can be pasted back into the spec doc.
"""

from dataclasses import dataclass

# ---------------------------------------------------------------------------
# Battery
# ---------------------------------------------------------------------------

BATTERY_CELLS = 3
BATTERY_CAPACITY_AH = 3.0          # 3000 mAh per cell, 3S series
BATTERY_NOMINAL_V = 3.7 * BATTERY_CELLS   # 11.1 V
BATTERY_USABLE_AH = BATTERY_CAPACITY_AH * 0.8   # 80% DoD to preserve life

# ---------------------------------------------------------------------------
# Regulator ratings (datasheet max continuous)
# ---------------------------------------------------------------------------

RAIL_RATINGS = {
    "12V": 8.0,    # Battery direct — limited by BMS / wire / fuse
    "5V":  3.0,    # LM2596S-ADJ buck
    "3V3": 0.6,    # AP2112K-3.3 LDO
    "4V":  2.0,    # LM2596S-ADJ buck (second one, for SIM800L)
}

REG_EFFICIENCY = {
    "5V":  0.85,   # LM2596S at 12V→5V/2A
    "4V":  0.82,   # LM2596S at 12V→4V/1A avg
    "3V3": None,   # LDO — efficiency = Vout/Vin = 3.3/5 = 0.66
}

# ---------------------------------------------------------------------------
# Load definitions
# ---------------------------------------------------------------------------

@dataclass
class Load:
    name: str
    rail: str          # "12V" | "5V" | "3V3" | "4V"
    i_typ_ma: float    # typical average current
    i_peak_ma: float   # peak / burst current
    note: str = ""


LOADS = [
    # ---- 12V rail (direct from battery) -----------------------------------
    Load("L298N motor power (both motors)", "12V",  800, 4000, "Stall up to 2A/motor; avg cruise ~0.4A each"),

    # ---- 5V rail (LM2596S buck) -------------------------------------------
    Load("ESP32-S3-CAM module",      "5V",  350,  500, "WiFi TX + camera active"),
    Load("SG90 servo Pan",           "5V",   80,  600, "Idle 80mA; peak under load 600mA"),
    Load("SG90 servo Tilt",          "5V",   80,  600, ""),
    Load("L298N logic VSS",          "5V",   50,   80, "Internal LDO supplies driver logic"),
    Load("HC-SR04 x 4",              "5V",   60,   60, "15mA × 4, mostly idle"),
    Load("MAX98357A amplifier",      "5V",  100,  500, "Class-D, varies w/ audio level"),
    Load("INMP441 mic",              "5V",    2,    2, "Powered via I2S 3V3 actually — moved? see 3V3"),
    Load("AP2112K LDO input",        "5V",  120,  150, "Passes 3V3 rail load through"),
    Load("IR LEDs (dock beacon RX)", "5V",    5,   30, "Pulsed 38kHz"),

    # ---- 3V3 rail (AP2112K LDO from 5V) -----------------------------------
    Load("ESP32-S3 core logic share","3V3",  50,  100, "If module uses 5V input it self-regulates; tap a bit for sensors only"),
    Load("MPU6050 IMU",              "3V3",   4,    4, "I2C device"),
    Load("SSD1306 OLED",             "3V3",  20,   25, "All pixels on"),
    Load("INMP441 mic",              "3V3",   2,    5, "I2S MEMS — usually on 3V3"),
    Load("I2C/I2S pull-ups",         "3V3",  10,   15, ""),
    Load("Boot button + EN cap",     "3V3",   1,    1, ""),

    # ---- 4V rail (LM2596S buck #2, dedicated for SIM800L) -----------------
    Load("SIM800L GSM",              "4V",  350, 2000, "Burst 2A every 4.6ms when TX; avg ~350mA"),
]

# ---------------------------------------------------------------------------
# Calculations
# ---------------------------------------------------------------------------

def sum_by_rail(field):
    totals = {r: 0.0 for r in RAIL_RATINGS}
    for load in LOADS:
        totals[load.rail] += getattr(load, field)
    return totals


def battery_draw_estimate():
    """Estimate average battery draw including regulator losses."""
    typ = sum_by_rail("i_typ_ma")     # mA at output of each rail

    # Power out of each rail (mW)
    p_12  = typ["12V"] * 12.0
    p_5   = typ["5V"]  * 5.0
    p_4   = typ["4V"]  * 4.0
    p_3v3 = typ["3V3"] * 3.3

    # 3V3 sourced from 5V via LDO — its power comes from 5V budget
    # We already counted "AP2112K LDO input" on 5V at 120mA average,
    # so don't double-count: just sum 5V × Vin and add to 12V budget via efficiency.

    # 5V buck input power (W) = Pout / eff
    p_5_in = (p_5 / 1000.0) / REG_EFFICIENCY["5V"]
    # 4V buck input power
    p_4_in = (p_4 / 1000.0) / REG_EFFICIENCY["4V"]
    # 12V loads direct
    p_12_direct = p_12 / 1000.0

    total_p_w = p_5_in + p_4_in + p_12_direct
    i_battery_a = total_p_w / BATTERY_NOMINAL_V
    runtime_h = BATTERY_USABLE_AH / i_battery_a if i_battery_a > 0 else float("inf")
    return total_p_w, i_battery_a, runtime_h


def main():
    print("# Power Budget — Elderly Companion Robot\n")
    print(f"Battery: {BATTERY_CELLS}S 18650, {BATTERY_CAPACITY_AH * 1000:.0f} mAh/cell, "
          f"nominal {BATTERY_NOMINAL_V:.1f} V, usable {BATTERY_USABLE_AH:.1f} Ah (80% DoD)\n")

    print("## Per-rail summary\n")
    print("| Rail | Reg max | I_typ | I_peak | Headroom (typ) | Headroom (peak) |")
    print("|------|---------|-------|--------|----------------|------------------|")
    typ = sum_by_rail("i_typ_ma")
    peak = sum_by_rail("i_peak_ma")
    for rail, rating_a in RAIL_RATINGS.items():
        rating_ma = rating_a * 1000
        headroom_typ = rating_ma - typ[rail]
        headroom_peak = rating_ma - peak[rail]
        mark_typ = "OK" if headroom_typ > 0 else "OVER"
        mark_peak = "OK" if headroom_peak > 0 else "WARN"
        print(f"| {rail} | {rating_ma:.0f} mA | {typ[rail]:.0f} mA | {peak[rail]:.0f} mA | "
              f"{headroom_typ:+.0f} mA {mark_typ} | {headroom_peak:+.0f} mA {mark_peak} |")

    print("\n## Per-load detail\n")
    for rail in RAIL_RATINGS:
        loads = [l for l in LOADS if l.rail == rail]
        if not loads:
            continue
        print(f"### {rail} rail")
        print("| Load | I_typ (mA) | I_peak (mA) | Note |")
        print("|------|------------|-------------|------|")
        for l in loads:
            print(f"| {l.name} | {l.i_typ_ma:.0f} | {l.i_peak_ma:.0f} | {l.note} |")
        print()

    p_w, i_a, runtime_h = battery_draw_estimate()
    print("## Battery runtime estimate (typical load)\n")
    print(f"- Total power drawn (typ): **{p_w:.2f} W**")
    print(f"- Battery current (typ):   **{i_a*1000:.0f} mA @ {BATTERY_NOMINAL_V:.1f} V**")
    print(f"- Runtime to 80% DoD:      **{runtime_h:.1f} hours** "
          f"({runtime_h*60:.0f} minutes)")
    print()
    print("> Peak draws are bursty (servo stall, GSM TX). Runtime estimate uses")
    print("> typical loads only. Real runtime depends heavily on patrol/idle ratio.")


if __name__ == "__main__":
    main()
