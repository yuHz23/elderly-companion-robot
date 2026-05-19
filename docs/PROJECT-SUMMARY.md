# Project Summary — Elderly Companion Robot

> Tổng hợp toàn bộ dự án. Đây là cửa ngõ đầu tiên cho người mới đến repo. Click qua các tài liệu chi tiết theo nhu cầu.
>
> **Status (2026-05-19)**: All 12 phases per `HDSD-Lap-Rap-Robot.md` shipped. Firmware production-ready. Pending: physical assembly + bring-up + 24h soak + deployment.

---

## 1. Bối cảnh & mục tiêu

Robot bánh xe theo dõi người già sống một mình. Thiết kế quanh 4 capability:

1. **PTZ camera** — pan-tilt 2-servo, MJPEG stream qua browser
2. **Voice I/O** — INMP441 mic + MAX98357A amp + speaker 3W (pipeline đầy đủ là post-Phase 12)
3. **Auto-dock + sạc** — IR beacon homing + copper contact, 80% docking success
4. **Cảnh báo khẩn cấp** — SIM800L gửi SMS + gọi điện gia đình khi phát hiện té ngã

Triết lý: **"Pattern over Invention"** — 9 reference project được tích hợp lại, không sáng tạo subsystem mới.

---

## 2. Hardware tổng quan

```
                    ESP32-S3-CAM N16R8
                   ╔════════════════════╗
                   ║  16MB flash        ║
                   ║  8MB Octal PSRAM   ║
                   ║  OV3660 DVP cam    ║
                   ╚═════════╤══════════╝
                             │ GPIO matrix
       ┌──────────┬──────────┼──────────┬──────────┬──────────┐
       │          │          │          │          │          │
       ▼          ▼          ▼          ▼          ▼          ▼
   USB-C       I2C bus    LEDC×3     UART_1     GPIO×12    ADC×2
   console     400kHz     timers     SIM800L    sensors    VBAT
   (CDC)       │          │          │          │          DOCK
               │          │          │          │
       ┌───────┼───────┐  │          │   ┌──────┼──────┬──────┬──────┐
       ▼       ▼       ▼  │          │   ▼      ▼      ▼      ▼      ▼
    MPU6050  OLED   reserved         │  HC-SR04 x4 (F/B/L/R via voltage divider)
    0x68    0x3C                     │  + servo PTZ (GPIO44/45)
                                     │  + IR_RX dock (GPIO5)
                                     │  + L298N motor (GPIO6-11)
                                     │
                                     ▼
                              SIM800L GSM
                              (4V rail riêng)
```

### Power tree (4 rail riêng)

| Rail | Reg | Max | Loads |
|------|-----|-----|-------|
| 12V | Battery 3S 18650 direct (fused 5A) | 8A | L298N motor power |
| 5V | LM2596S-ADJ buck #1 | 3A | Camera, servo, audio, L298N logic |
| 4V | LM2596S-ADJ buck #2 (separate!) | 2A | SIM800L only — 2A peak TX burst |
| 3V3 | AP2112K LDO from 5V | 600mA | I2C devices, IR, pull-ups |

→ Chi tiết: `hardware/power-tree-spec.md`, calculation tool `power_budget.py`.

---

## 3. Firmware kiến trúc

```
┌────────────────────────────────────────────────────────────┐
│ Layer 4 — Home Assistant (MQTT, Telegram alerts)            │
├────────────────────────────────────────────────────────────┤
│ Layer 3 — task_behavior (top FSM)                           │
│  IDLE ⇄ PATROL ⇄ RETURN_HOME → DOCKED                       │
│         ↑ EVT_FALL_DETECTED preempts → SOS_ACTIVE           │
├────────────────────────────────────────────────────────────┤
│ Layer 2 — Domain tasks (9 FreeRTOS task)                    │
│  navigation · ptz · sensor_fusion · dock · audio            │
│  sos · oled · mqtt · schedule                               │
├────────────────────────────────────────────────────────────┤
│ Layer 1 — Drivers (10 HAL component)                        │
│  motor_l298n · servo_pwm · i2c_bus · hcsr04 · mpu6050       │
│  audio_i2s · sim800l · battery · ir_dock · ssd1306          │
├────────────────────────────────────────────────────────────┤
│ Layer 0 — ESP-IDF v5 + FreeRTOS                             │
└────────────────────────────────────────────────────────────┘
```

→ Chi tiết: `firmware/architecture.md`.

### Concurrency model

- **9 task chạy đồng thời** trên dual-core ESP32-S3
- **xQueueOverwrite single-slot** cho sensor state (latest-wins, no backlog)
- **EventGroup** cho fall + obstacle bits
- **portMUX critical section** cho state nhỏ, mutex cho state lớn
- **Watchdog 500ms** trong task_navigation → motor brake nếu mất command

---

## 4. Cấu trúc thư mục

```
elderly-companion-robot/
├── docs/
│   ├── PROJECT-SUMMARY.md                ← bạn đang đọc
│   ├── HDSD-Lap-Rap-Robot.md             ← master 12-phase build guide
│   ├── firmware/
│   │   ├── architecture.md
│   │   └── bringup-integration.md
│   ├── hardware/
│   │   ├── pin-mapping.md                ← authoritative GPIO table
│   │   ├── power-tree-spec.md            + power-bringup.md
│   │   ├── decoupling-network.md
│   │   ├── pcb-layout-power.md
│   │   ├── mcu-core-spec.md              + mcu-bringup.md
│   │   ├── ptz-spec.md                   + ptz-bringup.md
│   │   ├── drive-spec.md                 + drive-bringup.md
│   │   ├── sensor-spec.md                + sensor-bringup.md
│   │   ├── audio-spec.md                 + audio-bringup.md
│   │   ├── sim800l-spec.md               + sim800l-bringup.md
│   │   ├── dock-spec.md                  + dock-bringup.md
│   │   └── power_budget.py
│   ├── test/
│   │   ├── test-plan.md                  ← 6 test category + edge cases
│   │   ├── 24h-soak-plan.md
│   │   └── test-results-template.md
│   └── deploy/
│       ├── home-assistant.md             ← HASS YAML + Telegram automations
│       ├── production-deploy.md
│       └── ota-update.md
├── firmware/                              ← PlatformIO + ESP-IDF v5
│   ├── platformio.ini
│   ├── partitions.csv                    ← 16MB layout, 2 OTA slots
│   ├── sdkconfig.defaults                ← Octal PSRAM, USB-CDC, brownout
│   ├── main/                             ← entry + smoke_test (HTTP server + web UI)
│   ├── drivers/                          ← 10 hardware abstractions
│   │   ├── motor_l298n/ · servo_pwm/ · audio_i2s/
│   │   ├── mpu6050/ · hcsr04/ · i2c_bus/ · ssd1306/
│   │   ├── sim800l/ · battery/ · ir_dock/
│   ├── tasks/                            ← 9 FreeRTOS tasks
│   │   ├── task_behavior/ · task_navigation/ · task_ptz/
│   │   ├── task_sensor_fusion/ · task_dock/ · task_audio/
│   │   ├── task_sos/ · task_oled/ · task_mqtt/ · task_schedule/
│   └── utils/
│       ├── wifi_manager/                 ← softAP captive portal
│       ├── robot_mqtt/                   ← esp-mqtt wrapper
│       └── self_test/                    ← 10-check diagnostic
├── hardware/kicad/                       ← KiCad PCB (in progress, ngoài 12 phase)
└── mechanical/
    ├── chassis-spec.md
    ├── chassis-layer1.svg + .dxf
    ├── chassis-layer2.svg + .dxf
    ├── generate_dxf.py
    └── assembly-phase1.md
```

---

## 5. Tài nguyên & metric

| Metric | Value |
|--------|-------|
| Tổng code + docs | ~17,000 dòng |
| Driver components | 10 |
| FreeRTOS tasks | 9 (+ HTTP worker) |
| HTTP endpoints | 38 |
| MQTT topics | 6 publish + 1 subscribe |
| NVS namespaces | 7 (wifi, servo, nav, mpu6050, sos, mqtt, schedule) |
| Free heap idle | > 150 KB |
| PSRAM usage | ~430 KB (camera fb + audio buf) |
| Battery runtime typical | ~98 phút continuous, 3-5h mixed |
| Total BOM | ~700-800k VND |

---

## 6. Hardware BOM tổng (đã owned vs to-buy)

### Đã có (từ Sesame Robot + previous projects)
- ESP32-S3-CAM N16R8 (XiaoZhi variant)
- MPU6050 GY-521
- SSD1306 OLED 128×64
- MG90S × 8 (dùng 2 cho PTZ)
- INMP441 MEMS mic
- MAX98357A I2S amp + 3W speaker
- SIM800L GSM module

### Cần mua (~700-800k VND)
- L298N module — 25k
- 2× BO motor + bánh xe 65mm — 100k
- 4× HC-SR04 — 60k
- 3× 18650 Samsung/LG — 150k
- 3S BMS + balance — 30k
- AC adapter 12.6V/3A — 80k
- LM2596S-ADJ × 2, AP2112K, capacitors, resistors — 50k
- Spring contact pogo × 2 — 10k
- IR LED 940nm + 38kHz module — 15k
- Acrylic chassis laser-cut — 50k
- 3D-print PTZ mount — 50k
- Standoff M3, vít, đai ốc — 30k
- Nano SIM 2G + nạp tiền — 50k
- Connector + dây — 20k
- Dock plate copper + acrylic — 50k

---

## 7. 12 phase progression

| # | Phase | Commit | Files | Key output |
|---|-------|--------|-------|-----------|
| 1 | Chassis | ad78a6a | 9 | SVG/DXF laser-cut, assembly guide |
| 2 | Power tree | c0431ee | 5 | 4-rail spec, current budget, PCB layout guide |
| 3 | MCU + camera | c4d550f | 15 | PlatformIO scaffold, smoke test MJPEG stream |
| 4 | PTZ | 5df4ec1 | 10 | servo_pwm driver, joystick UI |
| 5 | Drive train | 096c871 | 10 | L298N driver, 2D touch joystick, watchdog |
| 6 | Sensors | ce96567 | 18 | IMU + 4 ultrasonic, fall FSM, obstacle gate |
| 7 | Audio | 954ab20 | 10 | I2S full-duplex, WAV download, loopback |
| 8 | SIM800L SOS | 52aa242 | 10 | AT command parser, fall→SMS+dial |
| 9 | Auto-dock | 064bbb7 | 13 | 7-state docking FSM, battery monitor |
| 10 | Integration | 32b1329 | 16 | Behavior FSM, OLED, MQTT scaffold |
| 11 | Test + diag | 630319d | 8 | Test plan, 24h soak, self-test 10-check |
| 12 | Deploy | dea7ea7 | 11 | HASS config, schedule cron, OTA prep |

---

## 8. Quy trình build vật lý (next steps)

### Order of operations

1. **Mua phần cứng** (BOM section 6) — 1 tuần wait time
2. **Đặt laser-cut chassis** theo `mechanical/chassis-*.dxf` — 2-3 ngày
3. **Schematic KiCad final review** — hiện đang dở dang trong `hardware/kicad/`. Verify against các `*-spec.md` checklists trước khi đặt PCB.
4. **Đặt PCB tại JLCPCB** (gerber + BOM + CPL) — 5-7 ngày
5. **Solder PCB** + assembly chassis — 1 ngày
6. **Bring-up theo thứ tự**:
   - Phase 2.A-G (power tree) — 1 giờ
   - Phase 3.A-E (MCU boot, WiFi, camera) — 30 phút
   - Phase 4 (PTZ) — 30 phút
   - Phase 5 (drive) — 1 giờ (bench → sàn)
   - Phase 6 (sensors) — 30 phút
   - Phase 7 (audio) — 30 phút
   - Phase 8 (SIM800L) — 1 giờ
   - Phase 9 (dock build + bring-up) — 2 giờ
   - Phase 10 (integration) — 1 giờ
7. **Phase 11 test plan** — 3-4 giờ functional + 24h soak (weekend)
8. **Phase 12 deploy** — 1-2 giờ on-site tại nhà người thân

**Tổng**: 2-3 tuần từ "mua hàng" đến "robot vận hành tại nhà".

---

## 9. Non-goals / out-of-scope

Các capability sau **không** nằm trong 12 phase HDSD. Sẽ là **separate effort** sau:

- **Voice pipeline đầy đủ**: wake word → STT → Claude API → TTS. Scaffolded trong `audio-spec.md` §7. Cost $15-30/tháng cloud services. Phase 13+.
- **OTA endpoint**: partition + rollback đã ready. Code documented in `ota-update.md`. Phase 13+.
- **Vision-based path planning**: thay random walk bằng visual SLAM. Cần TFlite Micro. Phase 14+.
- **Signed images / secure boot**: production hardening. Phase 14+.
- **Multi-robot fleet**: 1 broker, nhiều robot, dashboard. Phase 15+.

---

## 10. Quy ước khi maintain repo

### Commit messages
Format: `Phase N: <subsystem> — <one-line summary>` cho phase work, hoặc `<type>: <description>` cho post-phase changes.

### Test workflow
Mỗi PR phải:
1. `pio run -e esp32-s3-cam` không error
2. Run `/diag/selftest` trên hardware → 10/10 pass
3. Section "Pass criteria" trong relevant `*-bringup.md` verified

### Memory management
- Pin-mapping changes → update `docs/hardware/pin-mapping.md` first, then code
- LEDC channel changes → update `docs/firmware/architecture.md` task table
- NVS namespace additions → register in `architecture.md` §3.3

---

## 11. License & credits

License: MIT (xem `LICENSE`).

Reference projects (mỗi pattern attribute in commit message của phase tương ứng):
- [Watney Rover](https://github.com/nikivanov/watney) — chassis + auto-dock pattern
- [Rudra](https://github.com/aceta-minophen/Rudra) — PTZ camera
- [Caretaker](https://github.com/positron48/robot) — spring contact dock charging
- [Desk Buddy](https://www.hackster.io/roboattic_Lab) — voice UX flow
- [Security Bot](https://github.com/SarmaHighOnCode/robotsecurity) — I2S wiring
- [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) — firmware FreeRTOS pattern
- [osrf/autodock](https://github.com/osrf/autodock) — docking state machine
- [RobotDockCenter](https://github.com/PatzEdi/RobotDockCenter) — visual docking (future)
- Sesame Robot (own previous project) — ESP32-S3 pinout verified
