# Elderly Companion Robot — Pin Mapping & Hardware Reference

> Authoritative GPIO assignment table for ESP32-S3-CAM N16R8. All schematic and firmware work must reference this document.
>
> **Updated 2025-05-15**: Architecture changed from separate ESP32-S3 + ESP32-CAM to integrated ESP32-S3-CAM N16R8. SIM800L GSM module added for cellular emergency alerts. XiaoZhi audio combo (MAX98357A + INMP441 + 3W speaker) confirmed.

## ESP32-S3-CAM GPIO Allocation

| GPIO | Peripheral | Signal | Direction | Notes |
|------|-----------|--------|-----------|-------|
| 0 | Boot | BOOT | Input | Strapping pin — button to GND, pull-up 10K to +3V3 |
| 4 | IR Dock | IR_TX | Output | IR LED beacon transmit (220R series) |
| 5 | IR Dock | IR_RX | Input | IR receiver output |
| 6 | L298N | ENA | Output | PWM Motor A speed (LEDC channel 0) |
| 7 | L298N | IN1 | Output | Motor A direction |
| 8 | L298N | IN2 | Output | Motor A direction |
| 9 | L298N | IN3 | Output | Motor B direction |
| 10 | L298N | IN4 | Output | Motor B direction |
| 11 | L298N | ENB | Output | PWM Motor B speed (LEDC channel 1) |
| 12 | I2S Audio | LRCLK/WS | Output | **10K pull-down required** (strapping pin) |
| 14 | I2S Audio | BCLK/SCK | Output | Shared clock for mic + amp |
| 15 | DVP Camera | Y2/DVP | — | OV3660 DVP pin (internal to ESP32-S3-CAM module) |
| 16 | I2S Audio | DOUT | Output | To MAX98357A DIN |
| 17 | I2S Audio | DIN | Input | From INMP441 SD |
| 18 | DVP Camera | PCLK/DVP | — | OV3660 pixel clock (internal) |
| 19 | DVP Camera | HSYNC/DVP | — | OV3660 horizontal sync (internal) |
| 21 | I2C Bus | SDA | Bidirectional | Default I2C data, 4.7K pull-up to +3V3 |
| 22 | I2C Bus | SCL | Output | Default I2C clock, 4.7K pull-up to +3V3 |
| 35 | HC-SR04 Front | TRIG | Output | Ultrasonic trigger pulse |
| 36 | HC-SR04 Front | ECHO | Input | Via voltage divider (1K+2K: 5V→3.33V) |
| 37 | HC-SR04 Back | TRIG | Output | Ultrasonic trigger pulse |
| 38 | HC-SR04 Back | ECHO | Input | Via voltage divider |
| 39 | HC-SR04 Left | TRIG | Output | Ultrasonic trigger pulse |
| 40 | HC-SR04 Left | ECHO | Input | Via voltage divider |
| 42 | HC-SR04 Right | TRIG | Output | Ultrasonic trigger pulse |
| 43 | HC-SR04 Right | ECHO | Input | Via voltage divider |
| 44 | Servo | PAN_PWM | Output | SG90 pan servo (50Hz PWM) |
| 45 | Servo | TILT_PWM | Output | SG90 tilt servo (50Hz PWM) |
| 46 | SIM800L | UART2_RX | Input | ESP32-S3 RX ← SIM800L TXD |
| 47 | SIM800L | PWRKEY | Output | SIM800L power key (active LOW pulse) |
| 48 | SIM800L | UART2_TX | Output | ESP32-S3 TX → SIM800L RXD |
| EN | Reset | EN | Input | 10K pull-up to +3V3 + 100nF cap to GND |

> **Note**: GPIO 1 and 3 (formerly UART0 to ESP32-CAM) are now freed. The camera connects via DVP internally on the ESP32-S3-CAM module. UART0 is available for USB debug/programming.

> **Note**: SIM800L TX outputs 2.8V logic (compatible with ESP32-S3 3.3V input). SIM800L RX accepts 2.5V+ (compatible with ESP32-S3 3.3V output). No level shifter needed.

## Strapping Pin Constraints (ESP32-S3)

| GPIO | Default | Constraint |
|------|---------|-----------|
| GPIO0 | Pull-up | LOW = USB download boot; HIGH = SPI boot. Use as boot button only. |
| GPIO12 | Weak pull-down | **MUST add 10K external pull-down** to prevent flash voltage crash. |
| GPIO46 | Pull-up | ESP32-S3 strapping — check datasheet before use. |
| GPIO26-32 | N/A | **OPI flash pins on N16R8 variant — DO NOT USE for I/O.** |
| ADC2 | N/A | Unusable during WiFi TX. Use ADC1 (GPIO1-10) for analog reads only. |

## Power Architecture

```
3x 18650 Battery (11.1V nominal, 12.6V max charge)
│
├── LM2596S-5 Buck Converter (12V → 5V, 3A)
│   │   Input:  C1 100uF electrolytic (input bypass)
│   │            C2 10uF ceramic (input bypass)
│   │   Output:  L1 33uH inductor
│   │            D1 SS34 Schottky (catch diode)
│   │            C5 22uF ceramic (output filter)
│   │
│   ├── +5V Rail (3A max)
│   │   ├── L298N motor driver (2A peak)
│   │   ├── 2x SG90 servos (1.2A peak)
│   │   ├── ESP32-S3-CAM module (350mA WiFi + camera)
│   │   ├── 4x HC-SR04 ultrasonic (240mA)
│   │   └── MAX98357A amplifier (varies with volume)
│   │
│   ├── AP2112K-3.3 LDO (5V → 3.3V, 600mA)
│   │   │   Input:  C3 10uF ceramic
│   │   │   Output: C4 10uF + 100nF ceramic
│   │   │
│   │   └── +3V3 Rail (600mA max)
│   │       ├── ESP32-S3 (260mA WiFi active)
│   │       ├── MPU6050 (4mA)
│   │       ├── SSD1306 OLED (20mA)
│   │       ├── INMP441 mic (5mA)
│   │       ├── IR LEDs (50mA)
│   │       └── Logic pull-ups/pull-downs
│   │
│   └── SIM800L LDO (5V → 4.0V, 2A peak)
│       │   Input:  C6_SIM 100uF + 100nF
│       │   Output: C7_SIM 100uF + 100nF
│       │   Use AMS1117-ADJ or dedicated SIM800L power module
│       │   (SIM800L requires 3.4–4.4V, 2A peak during TX burst)
│       │
│       └── +4V Rail (2A peak, ~350mA avg)
│           └── SIM800L GSM module
│
└── TP4056 + BMS (from dock)
    └── Balance charging for 3S 18650 pack
```

## I2C Bus Devices

| Address | Device | Pin | Notes |
|---------|--------|------|-------|
| 0x68 | MPU6050 GY-521 | SDA/SCL | AD0=GND → 0x68; AD0=VCC → 0x69 |
| 0x3C | SSD1306 OLED | SDA/SCL | SA0=GND → 0x3C; SA0=VCC → 0x3D |

**I2C pull-up calculation**: 4.7KΩ at 3.3V for 400kHz Fast Mode, 2 devices, ~200pF bus capacitance.

## I2S Audio Bus

| Signal | GPIO | Direction | Connected To |
|--------|------|-----------|-------------|
| BCLK | 14 | ESP32-S3 → both | INMP441 SCK, MAX98357A BCLK |
| LRCLK | 12 | ESP32-S3 → both | INMP441 WS, MAX98357A LRCLK |
| DOUT | 16 | ESP32-S3 → MAX98357A | MAX98357A DIN |
| DIN | 17 | INMP441 → ESP32-S3 | INMP441 SD |

**INMP441 L/R pin**: GND = left channel (data on WS low phase)

**MAX98357A GAIN pin**: GND = 9dB, NC = 12dB, VCC = 15dB. Using GND for moderate gain.

## SIM800L UART2 Connection

| Signal | ESP32-S3 GPIO | Direction | SIM800L Pin | Notes |
|--------|---------------|-----------|-------------|-------|
| TX | 48 | ESP32-S3 → SIM800L | RXD | 3.3V logic compatible (SIM800L RX accepts 2.5V+) |
| RX | 46 | SIM800L → ESP32-S3 | TXD | SIM800L TX outputs 2.8V (3.3V compatible) |
| PWRKEY | 47 | ESP32-S3 → SIM800L | PWRKEY | Active LOW pulse >1s to power on/off |

**SIM800L Power Note**: Requires dedicated 3.4–4.4V supply with 2A peak capability. Cannot share +3V3 rail. Use separate LDO from +5V.

## HC-SR04 Voltage Divider

Each HC-SR04 Echo pin outputs 5V, which exceeds ESP32-S3 3.3V tolerance.

```
Echo pin ──┬── R1 (1KΩ) ──┬── ESP32-S3 GPIO
           │               │
           └───────────────┴── R2 (2KΩ) ── GND

Voltage at junction: 5V × 2K/(1K+2K) = 3.33V ✓
```

## Components — On Hand + Purchased

| Component | Qty | Source | Notes |
|-----------|-----|--------|-------|
| ESP32-S3-CAM N16R8 + OV3660 | 1 | Shopee (XiaoZhi) | Integrated camera via DVP, replaces separate ESP32-CAM |
| INMP441 I2S MEMS Mic (MH-ET-LIVE) | 1 | Shopee | I2S interface, L/R channel select |
| MAX98357A I2S Amp + 3W 8Ω Speaker | 1 | Shopee (XiaoZhi combo) | Class-D amp, gain pin configurable |
| SIM800L GPRS/GSM Module | 1 | Shopee | UART2 interface, PWRKEY control, +4V supply |
| ESP32 WROOM-32 | 2 | Previous projects | Available |
| MPU6050 GY-521 | 1 | Available | IMU, I2C addr 0x68 |
| SSD1306 OLED 128x64 | 1 | Available | I2C addr 0x3C |
| MG90S servo | 8 | Sesame Robot | Can repurpose 2 for PTZ |
| TP4056 module | — | Design IC available | Module TBD |
| L298N motor driver | — | Design IC available | H-bridge, dual motor |
| LM2596S-5 buck converter | — | Design IC available | 12V→5V, 3A |
| AP2112K-3.3 LDO | — | Design IC available | 5V→3.3V, 600mA |

## Reference Projects

| Design Part | Source Repo | Reuse |
|------------|-----------|-------|
| Auto-dock & charging station | [Watney Rover](https://github.com/nikivanov/watney) | Copper contacts, dock design, WebRTC |
| PTZ Camera mount | [Rudra](https://github.com/aceta-minophen/Rudra) | 2x servo pan-tilt, elderly care features |
| ESP32-CAM docking | [Caretaker](https://github.com/positron48/robot) | Copper spring contacts, Home Assistant |
| IR beacon navigation | [Caretaker](https://github.com/positron48/robot) | IR beacon homing, dock approach |
| Voice interaction | [Desk Buddy](https://www.hackster.io/roboattic_Lab/desk-buddy-companion-robot-on-wheels-speech-recognition-89b9cb) | ESP32-S3 voice, ElevenLabs STT |
| I2S audio wiring | [Security Bot](https://github.com/SarmaHighOnCode/robotsecurity) | INMP441 + MAX98357A pattern |
| XiaoZhi AI voice ecosystem | [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) | ESP32-S3 voice assistant firmware reference |
| ROS auto-dock algorithm | [osrf/autodock](https://github.com/osrf/autodock) | Visual docking state machine (future) |
| ML dock navigation | [RobotDockCenter](https://github.com/PatzEdi/RobotDockCenter) | YOLO dock approach (future) |
| ESP32-S3 pin mapping | Sesame Robot (Obsidian) | GPIO allocation, boot/strapping |
| Power tree & LDO selection | Obsidian vault | AP2112K vs AMS1117, buck+LDO |
| I2C design rules | Obsidian vault | Pull-up calc, bus capacitance |