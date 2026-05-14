# Elderly Companion Robot — Pin Mapping & Hardware Reference

> Authoritative GPIO assignment table for ESP32-S3 N16R8. All schematic and firmware work must reference this document.

## ESP32-S3 GPIO Allocation

| GPIO | Peripheral | Signal | Direction | Notes |
|------|-----------|--------|-----------|-------|
| 0 | Boot | BOOT | Input | Strapping pin — button to GND, pull-up 10K to +3V3 |
| 1 | ESP32-CAM | UART0_TX | Output | TX → ESP32-CAM RX |
| 3 | ESP32-CAM | UART0_RX | Input | RX ← ESP32-CAM TX |
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
| 16 | I2S Audio | DOUT | Output | To MAX98357A DIN |
| 17 | I2S Audio | DIN | Input | From INMP441 SD |
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
| EN | Reset | EN | Input | 10K pull-up to +3V3 + 100nF cap to GND |

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
│   │   ├── ESP32-CAM module (250mA)
│   │   ├── 4x HC-SR04 ultrasonic (240mA)
│   │   └── MAX98357A amplifier (varies with volume)
│   │
│   └── AP2112K-3.3 LDO (5V → 3.3V, 600mA)
│       │   Input:  C3 10uF ceramic
│       │   Output: C4 10uF + 100nF ceramic
│       │
│       └── +3V3 Rail (600mA max)
│           ├── ESP32-S3 (260mA WiFi active)
│           ├── MPU6050 (4mA)
│           ├── SSD1306 OLED (20mA)
│           ├── INMP441 mic (5mA)
│           ├── IR LEDs (50mA)
│           └── Logic pull-ups/pull-downs
│
└── TP4056 + BMS (from dock)
    └── Balance charging for 3S pack
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

## HC-SR04 Voltage Divider

Each HC-SR04 Echo pin outputs 5V, which exceeds ESP32-S3 3.3V tolerance.

```
Echo pin ──┬── R1 (1KΩ) ──┬── ESP32-S3 GPIO
           │               │
           └───────────────┴── R2 (2KΩ) ── GND

Voltage at junction: 5V × 2K/(1K+2K) = 3.33V ✓
```

## Reference Projects

| Design Part | Source Repo | Reuse |
|------------|-----------|-------|
| Auto-dock & charging station | [Watney Rover](https://github.com/nikivanov/watney) | Copper contacts, dock design, WebRTC |
| PTZ Camera mount | [Rudra](https://github.com/aceta-minophen/Rudra) | 2x servo pan-tilt, elderly care features |
| ESP32-CAM docking | [Caretaker](https://github.com/positron48/robot) | Copper spring contacts, Home Assistant |
| IR beacon navigation | [Caretaker](https://github.com/positron48/robot) | IR beacon homing, dock approach |
| Voice interaction | [Desk Buddy](https://www.hackster.io/roboattic_Lab/desk-buddy-companion-robot-on-wheels-speech-recognition-89b9cb) | ESP32-S3 voice, ElevenLabs STT |
| I2S audio wiring | [Security Bot](https://github.com/SarmaHighOnCode/robotsecurity) | INMP441 + MAX98357A pattern |
| ROS auto-dock algorithm | [osrf/autodock](https://github.com/osrf/autodock) | Visual docking state machine (future) |
| ML dock navigation | [RobotDockCenter](https://github.com/PatzEdi/RobotDockCenter) | YOLO dock approach (future) |
| ESP32-S3 pin mapping | Sesame Robot (Obsidian) | GPIO allocation, boot/strapping |
| Power tree & LDO selection | Obsidian vault | AP2112K vs AMS1117, buck+LDO |
| I2C design rules | Obsidian vault | Pull-up calc, bus capacitance |