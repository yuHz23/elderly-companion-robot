# HDSD LẮP RÁP — ELDERLY COMPANION ROBOT

> Sổ tay tích hợp **9 pattern** từ các project tham khảo thành **1 sản phẩm hoàn chỉnh**.
>
> Robot bánh xe theo dõi người già sống một mình — PTZ camera, voice 2 chiều, auto-dock sạc, cảnh báo khẩn cấp qua GSM.
>
> **Đối tượng**: Kỹ sư embedded có kinh nghiệm ESP32/KiCad. Không phải tutorial nhập môn.
>
> **Phiên bản**: v1.0 — 2026-05-18
> **MCU chính**: ESP32-S3-CAM N16R8 (16MB flash + 8MB PSRAM + OV3660 camera DVP)

---

## MỤC LỤC

- [Chương 0 — Triết lý thiết kế](#chương-0--triết-lý-thiết-kế)
- [Chương 1 — Bản đồ Pattern → Sản phẩm](#chương-1--bản-đồ-pattern--sản-phẩm)
- [Chương 2 — Tổng quan kiến trúc](#chương-2--tổng-quan-kiến-trúc)
- [Phase 1 — Chassis & cơ khí](#phase-1--chassis--cơ-khí)
- [Phase 2 — Power tree (nguồn 3 cấp)](#phase-2--power-tree-nguồn-3-cấp)
- [Phase 3 — MCU core + Camera DVP](#phase-3--mcu-core--camera-dvp)
- [Phase 4 — PTZ Pan-Tilt (pattern Rudra)](#phase-4--ptz-pan-tilt-pattern-rudra)
- [Phase 5 — Drive train (pattern Watney)](#phase-5--drive-train-pattern-watney)
- [Phase 6 — Sensor suite (IMU + ultrasonic)](#phase-6--sensor-suite-imu--ultrasonic)
- [Phase 7 — Audio I/O (pattern XiaoZhi + Security Bot)](#phase-7--audio-io-pattern-xiaozhi--security-bot)
- [Phase 8 — Cellular emergency (SIM800L)](#phase-8--cellular-emergency-sim800l)
- [Phase 9 — Auto-dock + charging (pattern Watney + Caretaker)](#phase-9--auto-dock--charging-pattern-watney--caretaker)
- [Phase 10 — Firmware architecture (FreeRTOS)](#phase-10--firmware-architecture-freertos)
- [Phase 11 — Test & calibration](#phase-11--test--calibration)
- [Phase 12 — Deploy & Home Assistant](#phase-12--deploy--home-assistant)
- [Phụ lục A — Checklist trước khi đặt PCB](#phụ-lục-a--checklist-trước-khi-đặt-pcb)
- [Phụ lục B — Bảng tra cứu LCSC/MPN](#phụ-lục-b--bảng-tra-cứu-lcscmpn)
- [Phụ lục C — Troubleshooting](#phụ-lục-c--troubleshooting)

---

## Chương 0 — Triết lý thiết kế

### Nguyên tắc "Pattern over Invention"

Không có khối nào trong robot này là phát minh mới. Mỗi khối lấy ý tưởng đã chứng minh hoạt động từ một project open-source, **rồi tích hợp lại** quanh ESP32-S3-CAM. Đây là kỹ thuật **systems engineering**, không phải research.

**Lý do**: Một robot care-elderly cần tin cậy. Mỗi mạch lạ là một điểm fail tiềm năng. Dùng pattern đã được hàng nghìn người verify giảm rủi ro xuống nhiều bậc.

### 3 nguyên tắc bất di bất dịch

1. **Mỗi pin có 1 chủ duy nhất** — Không share GPIO. Xem [pin-mapping.md](hardware/pin-mapping.md).
2. **Mỗi rail nguồn có decoupling cap riêng** — 100nF gốm sát chip + 10µF tantali gốc rail.
3. **Mọi I/O ra ngoài board đều có ESD/over-voltage protection** — TVS diode hoặc voltage divider.

---

## Chương 1 — Bản đồ Pattern → Sản phẩm

| # | Pattern source | Lấy gì | Phase áp dụng | Trạng thái |
|---|----------------|--------|----------------|------------|
| 1 | [Watney Rover](https://github.com/nikivanov/watney) | Chassis bánh xe + dock copper contact + IR beacon homing | Phase 1, 5, 9 | Reference |
| 2 | [Rudra](https://github.com/aceta-minophen/Rudra) | PTZ camera mount 2 servo, firmware servo control | Phase 4 | Reference |
| 3 | [Caretaker](https://github.com/positron48/robot) | Copper spring contact, ESP32-CAM dock approach | Phase 9 | Reference |
| 4 | [Desk Buddy](https://www.hackster.io/roboattic_Lab/desk-buddy-companion-robot-on-wheels-speech-recognition-89b9cb) | UX flow voice interaction, ElevenLabs STT pipeline | Phase 7, 12 | Reference |
| 5 | [Security Bot](https://github.com/SarmaHighOnCode/robotsecurity) | I2S wiring INMP441 + MAX98357A | Phase 7 | Reference |
| 6 | [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) | Firmware voice assistant ESP32-S3 (FreeRTOS task pattern) | Phase 7, 10 | Reference |
| 7 | [osrf/autodock](https://github.com/osrf/autodock) | State machine docking (PREDOCK → DOCK → CHARGE) | Phase 9, 10 | Reference |
| 8 | [RobotDockCenter](https://github.com/PatzEdi/RobotDockCenter) | YOLO visual docking (future upgrade) | Phase 12+ | Optional |
| 9 | Sesame Robot (project trước của bạn) | ESP32-S3 boot/strapping, LEDC PWM cho MG90S | Phase 3, 4 | Tự reuse |

### Nguyên tắc trộn pattern

- **Watney + Caretaker**: Watney có chassis tốt, Caretaker có dock chargingcompact hơn → dùng chassis Watney + copper contact Caretaker.
- **Rudra + Sesame**: Rudra có code 2 servo, Sesame có pinout ESP32-S3 đã verify → dùng pinout Sesame + code Rudra (port sang LEDC API).
- **XiaoZhi + Desk Buddy**: XiaoZhi nặng về firmware (đã có wake-word), Desk Buddy có UX flow → firmware base XiaoZhi + tinh chỉnh prompt cho elderly.

---

## Chương 2 — Tổng quan kiến trúc

### Block diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                    ELDERLY COMPANION ROBOT                       │
│                                                                  │
│  ┌─────────────────┐    ┌──────────────────┐                    │
│  │ ESP32-S3-CAM    │◄──►│ OV3660 Camera    │ (DVP nội bộ)       │
│  │ N16R8 (Brain)   │    │ 2MP, 30fps       │                    │
│  └────────┬────────┘    └──────────────────┘                    │
│           │                                                      │
│  ┌────────┼─────────┬──────────┬──────────┬──────────┐          │
│  │        │         │          │          │          │          │
│  ▼        ▼         ▼          ▼          ▼          ▼          │
│ I2S    I2C bus   PWM x2     PWM x2    UART2     GPIO x12        │
│  │      │         │          │          │          │            │
│  ▼      ▼         ▼          ▼          ▼          ▼            │
│ Audio  IMU+OLED  PTZ servo  L298N      SIM800L   Ultrasonic     │
│ (Mic + (MPU6050 (Pan+Tilt   (Motor    (GSM       x4 (F/B/L/R)   │
│ Spkr)  + OLED)   SG90)       DC x2)    SOS)        + IR dock    │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Phân tầng phần mềm

```
┌─────────────────────────────────────────────┐
│ Layer 4: Cloud / Home Assistant             │ ← MQTT
├─────────────────────────────────────────────┤
│ Layer 3: Behavior (Idle, Patrol, SOS, Dock) │ ← State machine
├─────────────────────────────────────────────┤
│ Layer 2: Tasks (Vision, Voice, Nav, Dock)   │ ← FreeRTOS tasks
├─────────────────────────────────────────────┤
│ Layer 1: Drivers (servo, motor, mic, imu)   │ ← HAL
├─────────────────────────────────────────────┤
│ Layer 0: ESP-IDF + FreeRTOS                 │ ← Platform
└─────────────────────────────────────────────┘
```

---

## Phase 1 — Chassis & cơ khí

### Pattern: Watney chassis (đã đơn giản hóa)

Watney dùng base aluminum CNC. Chúng ta dùng **chassis nhựa 2 tầng laser-cut acrylic** (rẻ và dễ làm hơn ở VN).

### BOM cơ khí

| Item | Qty | Source | Note |
|------|-----|--------|------|
| Tấm acrylic 3mm 200×150mm | 2 | Shopee laser-cut | Tầng trên + tầng dưới |
| Cột đồng M3×30mm | 6 | Shopee | Nối 2 tầng |
| BO motor + bánh xe 65mm | 2 | Shopee | DC 6V có encoder càng tốt |
| Bánh xe omni đỡ sau 25mm | 1 | Shopee | Caster wheel |
| Khung 18650 3 pin | 1 | Shopee | Có lỗ vít M3 |
| Vít M3×8mm + M3×10mm | x20 | — | Lắp ráp tổng |

### Sơ đồ tầng

```
Tầng trên (200×150mm):
┌────────────────────────────────┐
│  ┌──────────┐    ┌──────────┐ │
│  │ PCB chính │    │ SSD1306  │ │
│  │ ESP32-S3 │    │  OLED    │ │
│  │ Camera   │    │ display  │ │
│  └──────────┘    └──────────┘ │
│  ┌─────────────────┐           │
│  │ INMP441 + speaker│           │
│  └─────────────────┘           │
└────────────────────────────────┘

Tầng dưới (200×150mm):
┌────────────────────────────────┐
│ ┌──┐ ┌────────┐ ┌──┐           │
│ │BO│ │ Battery │ │BO│           │
│ │M ├─┤ 3×18650├─┤M │           │
│ └──┘ └────────┘ └──┘           │
│ ┌──────┐  ┌─────┐  ┌────┐      │
│ │L298N │  │TP4056│ │BUCK│      │
│ └──────┘  └─────┘  └────┘      │
│         (Caster wheel ●)        │
└────────────────────────────────┘
```

### Kích thước tổng

- L × W × H: **200 × 150 × 130mm**
- Trọng lượng tải: 600g (chưa battery), ~900g (đủ battery)
- Speed mục tiêu: 0.3 m/s (đủ để theo người già đi bộ trong nhà)

### Checklist Phase 1

- [ ] Đặt laser-cut 2 tấm acrylic theo DXF (xem `mechanical/chassis-layer1.dxf`, `chassis-layer2.dxf`)
- [ ] Lắp 6 cột đồng M3×30 cố định 2 tầng
- [ ] Lắp motor BO + bánh xe vào tầng dưới
- [ ] Lắp caster wheel sau
- [ ] Kiểm tra: robot đứng cân bằng trên mặt phẳng, không vênh

---

## Phase 2 — Power tree (nguồn 3 cấp)

### Pattern: tự thiết kế dựa trên Watney + best-practice trong Obsidian vault

3 cấp nguồn riêng biệt tránh nhiễu lẫn nhau:

```
12V (3×18650) ──┬── LM2596S-5 ──► 5V (3A) ──┬── L298N + servo + ESP32-CAM
                │                            │
                │                            └── AP2112K-3.3 ──► 3.3V (600mA) → Logic
                │
                └── AMS1117-ADJ (tunable 4.0V) ──► 4V (2A) → SIM800L (riêng!)

Charge in (5V từ dock) ──► TP4056×3 BMS ──► Charge pack 3S 18650
```

### Tại sao tách rail 4V cho SIM800L?

SIM800L kéo **2A peak trong burst TX GSM** (~577µs mỗi 4.6ms). Nếu share rail với MCU, dropout sẽ reset ESP32. Tách rail = an toàn tuyệt đối.

### Decoupling đi kèm

| Vị trí | Cap |
|--------|-----|
| Đầu vào buck | 100µF electrolytic + 10µF ceramic |
| Đầu ra buck (5V) | 22µF ceramic + 100nF |
| Đầu vào LDO | 10µF ceramic |
| Đầu ra LDO (3.3V) | 10µF + 100nF |
| Mỗi chip IC | 100nF ceramic sát chân VCC |
| ESP32-S3-CAM | 470µF tantali bulk + 100nF |
| SIM800L | 470µF (tốt nhất 1000µF) + 100nF |

### Checklist Phase 2

- [ ] Đo điện áp các rail trước khi cắm MCU:
  - 5V rail = 5.0 ± 0.1V (no load) → 4.85V (full load) ✓
  - 3.3V rail = 3.3 ± 0.05V ✓
  - 4V SIM = 3.9–4.1V ✓
- [ ] Đo ripple (oscilloscope AC coupling): < 50mV pp trên 3.3V, < 100mV pp trên 5V
- [ ] Test sạc: cắm dock → BMS sạc, dòng 1A, dừng ở 4.2V/cell

---

## Phase 3 — MCU core + Camera DVP

### Pattern: Sesame Robot (ESP32-S3 pinout) + ESP32-S3-CAM module (đã có camera tích hợp)

Khác với 2 MCU rời (Watney/Rudra dùng ESP32 + ESP32-CAM riêng), **ESP32-S3-CAM N16R8** tích hợp camera OV3660 qua DVP nội bộ. Ưu điểm:
- Giảm 1 MCU → giảm BOM, ít cần UART bridging
- PSRAM 8MB cho frame buffer + WebRTC
- 16MB flash cho XiaoZhi firmware + ML model

### Wiring tối thiểu để boot

| Pin | Kết nối | Lý do |
|-----|---------|-------|
| GPIO0 | Nút BOOT + pull-up 10K | Strapping — LOW để vào download mode |
| EN | Pull-up 10K + tụ 100nF GND | Reset stable |
| GPIO12 | **Pull-down 10K** | Tránh strapping chọn nhầm flash voltage |
| USB D+/D- | Header USB-C | Programming + debug |
| 3V3 | Từ AP2112K | Đã có cap decoupling 100nF |

### Boot sequence verification

Cắm USB lần đầu, mở serial monitor (115200 baud):
```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x44c
...
I (1234) cpu_start: Pro cpu up.
I (1235) cpu_start: Application information:
I (1236) cpu_start: Project name:     elderly-companion
```

Nếu lặp lại reboot loop → kiểm tra GPIO12 strapping pin (lỗi phổ biến #1).

### Test camera

Flash example `camera_web_server` từ ESP-IDF, mở browser:
```
http://<ESP32-S3-IP>/
```
Phải thấy stream MJPEG 30fps.

### Checklist Phase 3

- [ ] ESP32-S3 boot OK, serial log hiện "Pro cpu up"
- [ ] Camera stream 30fps qua web example
- [ ] WiFi connect OK (`ping <ip>` từ máy tính)
- [ ] PSRAM detect = 8MB (xem log `Found 8MB PSRAM`)

---

## Phase 4 — PTZ Pan-Tilt (pattern Rudra)

### Pattern: Rudra 2-servo PTZ mount

Rudra dùng MG90S × 2 với mount in 3D. Chúng ta tận dụng **MG90S đã có từ Sesame Robot** (8 con, dùng 2).

### Cơ khí

| Item | Source |
|------|--------|
| Mount PTZ in 3D (PLA) | Thingiverse "MG90S Pan Tilt" hoặc tự in từ `mechanical/ptz-mount.stl` |
| Vít M2×8mm | x4 cố định servo |

Lắp servo Pan (đáy) lên tầng trên của chassis (hướng lên), lắp servo Tilt vuông góc lên trục Pan. Camera ESP32-S3-CAM cố định lên mount Tilt.

### Wiring (đã có trong pin-mapping.md)

| Servo | PWM pin | Power |
|-------|---------|-------|
| Pan SG90 | GPIO44 | +5V (rail chung), GND |
| Tilt SG90 | GPIO45 | +5V (rail chung), GND |

**LƯU Ý**: SG90/MG90S kéo peak 600mA mỗi con khi stall. 2 servo + L298N + camera trên cùng rail 5V → tổng peak ~2.5A. Buck 3A là vừa đủ.

### Driver code (port từ Rudra → ESP-IDF LEDC)

```c
// firmware/drivers/servo_pwm.c
#include "driver/ledc.h"

#define SERVO_PAN_PIN  44
#define SERVO_TILT_PIN 45
#define SERVO_FREQ_HZ  50      // 50Hz = 20ms period
#define SERVO_RES      LEDC_TIMER_14_BIT  // 16384 ticks / 20ms

// 1ms pulse = 0° → duty = 1/20 × 16384 = 819
// 2ms pulse = 180° → duty = 2/20 × 16384 = 1638
static uint32_t angle_to_duty(uint8_t angle) {
    return 819 + (angle * (1638 - 819)) / 180;
}

void servo_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = SERVO_RES,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t pan_ch = {
        .channel = LEDC_CHANNEL_2,  // 0,1 đã dùng cho motor
        .gpio_num = SERVO_PAN_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .duty = angle_to_duty(90),  // Center
    };
    ledc_channel_config(&pan_ch);
    // ... tilt tương tự
}

void servo_set_pan(uint8_t angle)  { ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, angle_to_duty(angle)); ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2); }
void servo_set_tilt(uint8_t angle) { ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, angle_to_duty(angle)); ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3); }
```

### Checklist Phase 4

- [ ] Servo Pan quét 0°→180° trong 1 giây mượt
- [ ] Servo Tilt quét 30°→150° (giới hạn để tránh đụng chassis)
- [ ] Không nghe tiếng buzz lạ (= PWM freq sai)
- [ ] Đo dòng peak khi 2 servo move đồng thời: < 1.2A

---

## Phase 5 — Drive train (pattern Watney)

### Pattern: Watney 2WD differential drive

H-bridge L298N + 2 motor BO + bánh xe 65mm. Differential drive (quay tại chỗ bằng cách quay 2 bánh ngược chiều).

### Wiring L298N

| L298N | ESP32 | Function |
|-------|-------|----------|
| ENA | GPIO6 (LEDC ch0) | PWM Motor A speed |
| IN1 | GPIO7 | Motor A dir bit 0 |
| IN2 | GPIO8 | Motor A dir bit 1 |
| IN3 | GPIO9 | Motor B dir bit 0 |
| IN4 | GPIO10 | Motor B dir bit 1 |
| ENB | GPIO11 (LEDC ch1) | PWM Motor B speed |
| +12V | Battery (qua nút công tắc chính) | Power motor |
| +5V | (NC — L298N có LDO nội bộ) | Logic |
| GND | Common GND | — |

### Truth table direction

| IN1 | IN2 | Motor A |
|-----|-----|---------|
| 0 | 0 | Coast (free) |
| 0 | 1 | Reverse |
| 1 | 0 | Forward |
| 1 | 1 | Brake |

### Driver code

```c
// firmware/drivers/motor_l298n.c
typedef enum { MOTOR_FWD, MOTOR_REV, MOTOR_BRAKE, MOTOR_COAST } motor_dir_t;

void motor_set(uint8_t side, motor_dir_t dir, uint8_t speed_pct) {
    uint8_t in1_pin = (side == 0) ? 7 : 9;
    uint8_t in2_pin = (side == 0) ? 8 : 10;
    ledc_channel_t ch = (side == 0) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;

    switch (dir) {
        case MOTOR_FWD:   gpio_set_level(in1_pin, 1); gpio_set_level(in2_pin, 0); break;
        case MOTOR_REV:   gpio_set_level(in1_pin, 0); gpio_set_level(in2_pin, 1); break;
        case MOTOR_BRAKE: gpio_set_level(in1_pin, 1); gpio_set_level(in2_pin, 1); break;
        case MOTOR_COAST: gpio_set_level(in1_pin, 0); gpio_set_level(in2_pin, 0); break;
    }
    uint32_t duty = (speed_pct * 1023) / 100;  // 10-bit PWM
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

// API cấp cao
void drive_forward(uint8_t speed)  { motor_set(0, MOTOR_FWD, speed); motor_set(1, MOTOR_FWD, speed); }
void drive_stop(void)              { motor_set(0, MOTOR_BRAKE, 0);   motor_set(1, MOTOR_BRAKE, 0); }
void drive_rotate_cw(uint8_t spd)  { motor_set(0, MOTOR_FWD, spd);   motor_set(1, MOTOR_REV, spd); }
void drive_rotate_ccw(uint8_t spd) { motor_set(0, MOTOR_REV, spd);   motor_set(1, MOTOR_FWD, spd); }
```

### Calibration

- **Dead zone**: Motor không quay dưới 20% PWM. Map input 0-100% → output 20-100%.
- **Bánh lệch**: Nếu robot đi forward bị lệch trái → motor phải yếu hơn → tăng `speed_b * 1.05` hệ số bù.

### Checklist Phase 5

- [ ] `drive_forward(50)` → robot tiến thẳng 1m không lệch > 5°
- [ ] `drive_rotate_cw(60)` → robot quay 90° trong ~0.8s
- [ ] L298N không nóng > 60°C khi chạy liên tục 5 phút

---

## Phase 6 — Sensor suite (IMU + ultrasonic)

### IMU MPU6050 (I2C 0x68)

**Dùng để**: Fall detection (gia tốc > 2g trong < 0.5s = té), heading correction (giúp đi thẳng).

```c
// firmware/drivers/mpu6050.c — interrupt-driven
void mpu6050_init(void) {
    i2c_master_init();
    mpu_write(MPU_PWR_MGMT_1, 0x00);    // Wake up
    mpu_write(MPU_CONFIG, 0x03);        // DLPF 44Hz
    mpu_write(MPU_GYRO_CONFIG, 0x10);   // ±1000°/s
    mpu_write(MPU_ACCEL_CONFIG, 0x10);  // ±8g
}

bool detect_fall(void) {
    int16_t ax, ay, az;
    mpu6050_read_accel(&ax, &ay, &az);
    float g = sqrtf(ax*ax + ay*ay + az*az) / 4096.0f;  // ±8g → 4096 LSB/g
    return (g > 2.5f);  // > 2.5g = sự kiện bất thường
}
```

Pattern: Detection 2 giai đoạn → spike gia tốc THEN orientation lạ kéo dài > 3s = fall xác nhận.

### Ultrasonic HC-SR04 × 4

**Pattern**: Watney có 2 ultrasonic, ta dùng 4 (F/B/L/R) cho 360° coverage.

Voltage divider 1KΩ + 2KΩ trên mỗi Echo (5V → 3.3V) — đã ghi trong pin-mapping.md.

```c
// firmware/drivers/hcsr04.c — non-blocking dùng RMT peripheral
uint16_t hcsr04_read_cm(uint8_t sensor_id) {
    static const gpio_num_t trig[] = {35, 37, 39, 42};
    static const gpio_num_t echo[] = {36, 38, 40, 43};

    gpio_set_level(trig[sensor_id], 0); esp_rom_delay_us(2);
    gpio_set_level(trig[sensor_id], 1); esp_rom_delay_us(10);
    gpio_set_level(trig[sensor_id], 0);

    int64_t start = esp_timer_get_time();
    while (gpio_get_level(echo[sensor_id]) == 0) {
        if (esp_timer_get_time() - start > 30000) return UINT16_MAX;  // Timeout
    }
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(echo[sensor_id]) == 1) {
        if (esp_timer_get_time() - echo_start > 30000) return UINT16_MAX;
    }
    int64_t duration = esp_timer_get_time() - echo_start;
    return (uint16_t)(duration / 58);  // µs → cm
}
```

### Sensor fusion: bản đồ né tránh

```c
typedef struct {
    uint16_t front, back, left, right;  // cm
    float pitch, roll;                  // độ
    bool fall_detected;
} robot_state_t;

void task_sensor_fusion(void *arg) {
    robot_state_t state;
    while (1) {
        state.front = hcsr04_read_cm(0);
        state.back  = hcsr04_read_cm(1);
        state.left  = hcsr04_read_cm(2);
        state.right = hcsr04_read_cm(3);
        mpu6050_read_orientation(&state.pitch, &state.roll);
        state.fall_detected = detect_fall();

        xQueueSend(state_queue, &state, 0);  // Push to other tasks
        vTaskDelay(pdMS_TO_TICKS(50));        // 20Hz
    }
}
```

### Checklist Phase 6

- [ ] I2C scan: phát hiện 0x68 (MPU) + 0x3C (OLED)
- [ ] MPU6050 đứng yên → accel z ≈ 1g (16384 LSB)
- [ ] 4 ultrasonic đọc 10-400cm chính xác ± 2cm
- [ ] Detect fall: thả robot từ cao 30cm → trigger event

---

## Phase 7 — Audio I/O (pattern XiaoZhi + Security Bot)

### Pattern: Security Bot I2S wiring + XiaoZhi firmware base

INMP441 (mic) + MAX98357A (amp) share **cùng I2S bus** (BCLK + LRCLK) nhưng khác data line (DOUT cho amp, DIN từ mic).

### Wiring (đã trong pin-mapping.md)

```
ESP32-S3 ──BCLK(14)──┬── INMP441 SCK
                     └── MAX98357A BCLK

ESP32-S3 ──LRCLK(12)─┬── INMP441 WS
                     └── MAX98357A LRCLK

ESP32-S3 ──DOUT(16)──── MAX98357A DIN  (TX path)
ESP32-S3 ◄─DIN(17)───── INMP441 SD     (RX path)
```

INMP441 L/R pin → **GND** (left channel, data on WS LOW).
MAX98357A GAIN pin → **GND** (9dB gain — moderate, đủ cho phòng 4×4m).
SD (shutdown) pin của MAX98357A → tie HIGH (always on).

### I2S driver — Full-duplex

```c
// firmware/drivers/audio_i2s.c
void audio_init(void) {
    i2s_config_t cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
    };
    i2s_pin_config_t pins = {
        .bck_io_num = 14,
        .ws_io_num = 12,
        .data_out_num = 16,
        .data_in_num = 17,
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
}
```

### Pipeline voice (XiaoZhi-inspired)

```
Mic → I2S RX → VAD (Voice Activity Detect) → Wake word "Hey Bot"
                                                    │
                                            (wake) ▼
                                       Buffer 5s audio → upload WebSocket
                                                    │
                                                    ▼
                                          Cloud STT (ElevenLabs/Whisper)
                                                    │
                                                    ▼
                                            LLM (Claude API)
                                                    │
                                                    ▼
                                            TTS → mp3 → ESP32
                                                    │
                                                    ▼
                                            Decode → I2S TX → MAX98357A → Speaker
```

### Checklist Phase 7

- [ ] Record 5s từ mic → save WAV qua serial → playback trên PC: nghe rõ giọng
- [ ] Generate sine wave 1kHz qua amp → đo bằng oscilloscope ngõ ra spkr: clean sine
- [ ] Wake word "Hey Bot" trigger sau 3 lần test
- [ ] Echo cancellation OK (mic không loop ngược loa)

---

## Phase 8 — Cellular emergency (SIM800L)

### Pattern: SOS dial thuần (không có project tham khảo trực tiếp, dùng AT command chuẩn)

Khi `fall_detected` hoặc nút SOS bấm, robot gọi điện + nhắn tin đến số người nhà.

### Wiring (UART2)

| ESP32-S3 | SIM800L | Notes |
|----------|---------|-------|
| GPIO48 (TX) | RXD | 3.3V OK |
| GPIO46 (RX) | TXD | 2.8V → ESP32 nhận OK |
| GPIO47 | PWRKEY | Active LOW pulse > 1s |
| 4V rail | VCC | **2A peak — rail riêng** |
| GND | GND | Common |

### Sequence khởi động SIM800L

```c
void sim800_power_on(void) {
    gpio_set_level(47, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(47, 0);          // Pulse LOW
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(47, 1);

    vTaskDelay(pdMS_TO_TICKS(5000));  // Wait register network

    uart_send("AT\r\n");              // Expect "OK"
    uart_send("AT+CREG?\r\n");        // Expect "+CREG: 0,1" or "0,5"
    uart_send("AT+CSQ\r\n");          // Signal quality > 15 = OK
}
```

### Lệnh SOS

```c
void sos_trigger(const char *msg) {
    char buf[160];

    // 1. Gửi SMS
    uart_send("AT+CMGF=1\r\n");  // Text mode
    snprintf(buf, sizeof(buf), "AT+CMGS=\"+84909123456\"\r\n");
    uart_send(buf);
    uart_send(msg);
    uart_send_byte(0x1A);  // Ctrl+Z to send

    vTaskDelay(pdMS_TO_TICKS(3000));

    // 2. Gọi điện
    uart_send("ATD+84909123456;\r\n");
}
```

### Anten

SIM800L cần anten externe (spring antenna đi kèm hoặc khá hơn dùng anten dán LTE GSM). Module SIM thường không khoẻ tín hiệu nếu để trong vỏ kim loại — **vỏ robot bằng acrylic là OK**.

### Checklist Phase 8

- [ ] AT command response "OK" sau 5s
- [ ] CSQ > 15 (signal quality khá)
- [ ] Test gửi SMS → điện thoại nhận được trong < 10s
- [ ] Test gọi → điện thoại đổ chuông trong < 15s

---

## Phase 9 — Auto-dock + charging (pattern Watney + Caretaker)

### Pattern combined

- **Watney**: Cấu trúc dock cao 100mm, 2 copper plate vertical
- **Caretaker**: Spring contact ở robot side (đảm bảo tiếp xúc tốt khi parking lệch ± 5mm)
- **IR beacon homing**: IR LED 940nm ở dock + receiver TSOP38238 ở robot

### Cơ khí dock station

```
DOCK (cố định tường):
┌────────────────────────────┐
│                            │  ◄ IR LED beacon (940nm, modulated 38kHz)
│  ╔═══╗      ╔═══╗          │
│  ║+5V║      ║GND║  ◄ Copper plates (50×30mm đồng đỏ)
│  ╚═══╝      ╚═══╝          │
│                            │
│  ┌──────────────────────┐  │
│  │ AC-DC Adapter 5V 3A  │  │
│  └──────────────────────┘  │
└────────────────────────────┘
```

### Cơ khí robot side

```
ROBOT (mặt sau):
        ┌────────────────────────┐
        │                        │
   ◄ IR │  ╔═══╗      ╔═══╗     │  ◄ Spring contact x 2
   recv │  ║+IN║      ║-IN║     │     (đẩy ra 3mm)
        │  ╚═══╝      ╚═══╝     │
        │       │        │       │
        │       └────────┴────► TP4056 + BMS
        └────────────────────────┘
```

### State machine docking (pattern osrf/autodock)

```
[IDLE]
   │ (battery < 20%)
   ▼
[SEARCH_BEACON]  ── quay tại chỗ, scan IR signal
   │ (IR detected)
   ▼
[APPROACH]       ── tiến về phía beacon, dùng IR strength làm reward
   │ (distance < 30cm via ultrasonic back)
   ▼
[ALIGN]          ── micro-adjust góc bằng IR phase difference
   │ (aligned within ±3°)
   ▼
[REVERSE_DOCK]   ── lùi từ từ 5cm/s
   │ (copper voltage detected > 4.5V)
   ▼
[CHARGING]       ── motor off, monitor charge current
   │ (charge done — current < 100mA)
   ▼
[IDLE_DOCKED]
```

### Code state machine

```c
// firmware/tasks/task_dock.c
typedef enum {
    DOCK_IDLE, DOCK_SEARCH, DOCK_APPROACH,
    DOCK_ALIGN, DOCK_REVERSE, DOCK_CHARGING, DOCK_DONE
} dock_state_t;

void task_dock(void *arg) {
    dock_state_t state = DOCK_IDLE;
    while (1) {
        switch (state) {
            case DOCK_IDLE:
                if (battery_pct() < 20) state = DOCK_SEARCH;
                break;

            case DOCK_SEARCH:
                drive_rotate_cw(40);
                if (ir_detected()) {
                    drive_stop();
                    state = DOCK_APPROACH;
                }
                break;

            case DOCK_APPROACH:
                drive_forward(50);
                if (hcsr04_read_cm(1) < 30) {  // back sensor
                    drive_stop();
                    state = DOCK_ALIGN;
                }
                break;

            case DOCK_ALIGN:
                if (ir_phase_offset() < 3.0f) state = DOCK_REVERSE;
                else drive_rotate_cw(20);
                break;

            case DOCK_REVERSE:
                motor_set(0, MOTOR_REV, 25);
                motor_set(1, MOTOR_REV, 25);
                if (dock_voltage() > 4.5f) {
                    drive_stop();
                    state = DOCK_CHARGING;
                }
                break;

            case DOCK_CHARGING:
                if (charge_current_ma() < 100) state = DOCK_DONE;
                break;

            case DOCK_DONE:
                state = DOCK_IDLE;
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Checklist Phase 9

- [ ] Dock IR beacon phát 38kHz modulated → receiver detect ở khoảng cách 1.5m
- [ ] Copper contact: đo điện trở tiếp xúc < 0.5Ω
- [ ] Manual approach → robot dock thành công 5/5 lần
- [ ] Auto approach từ 2m → dock thành công 4/5 lần (80% threshold)

---

## Phase 10 — Firmware architecture (FreeRTOS)

### Pattern: XiaoZhi FreeRTOS task organization

Mỗi chức năng = 1 task riêng, giao tiếp qua queue/event group.

### Task table

| Task | Priority | Stack | Period | Mục đích |
|------|----------|-------|--------|----------|
| `task_sensor_fusion` | 5 | 4096 | 50ms | Đọc IMU + ultrasonic |
| `task_dock` | 4 | 4096 | 100ms | State machine docking |
| `task_navigation` | 4 | 4096 | 50ms | Obstacle avoidance, motor cmd |
| `task_vision` | 3 | 8192 | event | Camera frame → MJPEG stream |
| `task_voice` | 3 | 8192 | event | I2S audio in/out + STT/TTS |
| `task_mqtt` | 2 | 4096 | 1000ms | Home Assistant publish |
| `task_oled` | 1 | 2048 | 200ms | Update OLED status |
| `task_sim800` | 2 | 4096 | event | Cellular SOS |

### Inter-task communication

```c
// Global queues
QueueHandle_t state_queue;       // robot_state_t
QueueHandle_t voice_cmd_queue;   // voice_cmd_t (move, stop, etc.)
QueueHandle_t mqtt_pub_queue;    // mqtt_msg_t
EventGroupHandle_t robot_events; // FALL, BATTERY_LOW, USER_PRESENT, etc.
```

### main.c — Entry point

```c
void app_main(void) {
    // Init drivers
    nvs_flash_init();
    wifi_init();
    i2c_master_init();
    motor_init();
    servo_init();
    audio_init();
    mpu6050_init();
    oled_init();
    sim800_power_on();

    // Init queues
    state_queue     = xQueueCreate(10, sizeof(robot_state_t));
    voice_cmd_queue = xQueueCreate(5, sizeof(voice_cmd_t));
    mqtt_pub_queue  = xQueueCreate(20, sizeof(mqtt_msg_t));
    robot_events    = xEventGroupCreate();

    // Spawn tasks
    xTaskCreate(task_sensor_fusion, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(task_navigation,    "nav",    4096, NULL, 4, NULL);
    xTaskCreate(task_dock,          "dock",   4096, NULL, 4, NULL);
    xTaskCreate(task_vision,        "vision", 8192, NULL, 3, NULL);
    xTaskCreate(task_voice,         "voice",  8192, NULL, 3, NULL);
    xTaskCreate(task_mqtt,          "mqtt",   4096, NULL, 2, NULL);
    xTaskCreate(task_sim800,        "sim",    4096, NULL, 2, NULL);
    xTaskCreate(task_oled,          "oled",   2048, NULL, 1, NULL);

    ESP_LOGI("MAIN", "All tasks launched.");
}
```

### Cấu trúc thư mục firmware đề xuất

```
firmware/
├── platformio.ini           # ESP-IDF framework
├── main/
│   ├── main.c               # app_main()
│   └── CMakeLists.txt
├── drivers/
│   ├── motor_l298n.c/.h
│   ├── servo_pwm.c/.h
│   ├── audio_i2s.c/.h
│   ├── mpu6050.c/.h
│   ├── hcsr04.c/.h
│   ├── oled_ssd1306.c/.h
│   ├── sim800_uart.c/.h
│   └── ir_dock.c/.h
├── tasks/
│   ├── task_sensor_fusion.c
│   ├── task_navigation.c
│   ├── task_dock.c
│   ├── task_vision.c
│   ├── task_voice.c
│   ├── task_mqtt.c
│   ├── task_oled.c
│   └── task_sim800.c
└── utils/
    ├── wifi_manager.c
    ├── mqtt_client.c
    ├── config_storage.c    # NVS load/save
    └── ota_update.c
```

---

## Phase 11 — Test & calibration

### Test 1: Smoke test (1 giờ)

- Robot đứng yên, cắm USB, log không có panic/abort
- WiFi + MQTT connect OK
- OLED hiện battery %, IP, state
- Tất cả task run (xem `pcTaskList()` output)

### Test 2: Drive train (30 phút)

- Forward 2m → đo lệch hướng
- Quay 360° tại chỗ → đo time + góc lệch
- Test obstacle: chặn trước → robot dừng < 200ms

### Test 3: Voice + camera (1 giờ)

- Stream camera 30 phút liên tục → không drop frame
- Wake word + LLM response trong < 3s end-to-end
- Audio playback rõ ràng, không méo

### Test 4: Docking (lặp 20 lần)

- Robot pin 80% → ép gọi dock command
- Đo % success, thời gian dock trung bình
- **Target**: 80% success, < 60s

### Test 5: Fall detection (10 lần)

- Drop từ 30cm → trigger event
- False positive khi đi xóc nhẹ: 0/10

### Test 6: SOS emergency (3 lần)

- Bấm nút SOS → SMS đến điện thoại trong < 15s
- Gọi điện đổ chuông trong < 20s

---

## Phase 12 — Deploy & Home Assistant

### MQTT topics

```yaml
elderly_robot/state           # JSON robot state
elderly_robot/cmd             # Receive commands
elderly_robot/event/fall      # Fall detected
elderly_robot/event/battery   # Battery low
elderly_robot/camera/stream   # MJPEG URL
elderly_robot/voice/in        # Audio command
elderly_robot/voice/out       # TTS response
```

### Home Assistant config

```yaml
# configuration.yaml
mqtt:
  sensor:
    - name: "Elderly Robot Battery"
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.battery }}"
      unit_of_measurement: "%"

  binary_sensor:
    - name: "Elderly Fall Detected"
      state_topic: "elderly_robot/event/fall"
      device_class: safety

automation:
  - alias: "Notify on Fall"
    trigger:
      platform: state
      entity_id: binary_sensor.elderly_fall_detected
      to: "on"
    action:
      - service: notify.family_telegram
        data:
          message: "⚠️ PHÁT HIỆN NGÃ! Camera: {{ states('camera.elderly_robot') }}"
      - service: tts.cloud_say
        data:
          entity_id: media_player.elderly_robot
          message: "Bạn có ổn không? Tôi đang gọi người nhà."
```

### Vận hành hằng ngày

| Thời điểm | Hành động tự động |
|-----------|------------------|
| 06:00 | Robot rời dock, đi chào buổi sáng |
| 08:00–11:00 | Patrol theo lịch (kiểm tra phòng) |
| 12:00 | Reminder uống thuốc (TTS) |
| 14:00–16:00 | Idle ở dock, sạc |
| 19:00 | Patrol tối |
| 22:00 | Trở về dock, sạc đêm |
| Bất kỳ | Fall event → SOS + notify + voice |

---

## Phụ lục A — Checklist trước khi đặt PCB

- [ ] ERC schematic = 0 violation
- [ ] DRC PCB = 0 violation (sau khi route)
- [ ] Tất cả footprint match component vật lý (đo bằng caliper)
- [ ] Gerber export đủ 6 layer: F.Cu, B.Cu, F.Mask, B.Mask, F.Silk, B.Silk + drill
- [ ] BOM export với LCSC part number cho JLCPCB assembly
- [ ] CPL (pick&place) export
- [ ] Đặt 5 board prototype (dự phòng 4 để hỏng)
- [ ] Kèm stencil để hand-paste solder paste nếu có 0402/0603

---

## Phụ lục B — Bảng tra cứu LCSC/MPN

(Chi tiết xem `hardware/kicad/elderly-companion-robot/*-bom.csv` sau khi export)

| Function | LCSC | MPN | Note |
|----------|------|-----|------|
| Buck 5V | C46378 | LM2596S-5.0 | TO-263-5 |
| LDO 3.3V | C51118 | AP2112K-3.3TRG1 | SOT-25 |
| H-bridge | C7395 | L298N | TODO board hoặc IC |
| Camera | (Shopee XiaoZhi) | ESP32-S3-CAM N16R8 | OV3660 DVP |
| MEMS Mic | (Shopee) | INMP441 | I2S |
| Audio amp | C910544 | MAX98357A | I2S 3W |
| IMU | C24112 | MPU-6050 | I2C 0x68 |
| OLED | (Shopee) | SSD1306 128×64 | I2C 0x3C |
| GSM | (Shopee) | SIM800L | UART 9600 |

---

## Phụ lục C — Troubleshooting

| Triệu chứng | Nguyên nhân khả dĩ | Cách xử lý |
|-------------|---------------------|------------|
| ESP32 reboot loop | GPIO12 không pull-down | Hàn R 10K xuống GND |
| Camera no init | PSRAM not detected | Enable PSRAM in menuconfig |
| Servo buzz | PWM freq sai | Set LEDC = 50Hz đúng |
| Motor không quay | L298N enable pin float | Check ENA/ENB PWM duty > 20% |
| Mic noise hỏng | I2S clock float / mic L/R pin sai | Pull L/R về GND, check clock |
| SIM800L không reg | RF tín hiệu yếu | Đổi anten, ra ngoài chỗ thoáng |
| Dock không tiếp xúc | Copper oxide | Lau bằng alcohol, dùng đồng đỏ thiếc |
| Fall false positive | DLPF mở quá cao | Set MPU CONFIG = 0x03 (44Hz) |
| WiFi disconnect | Buck ripple cao | Tăng cap output 22µF → 47µF |
| MQTT mất gói | QoS = 0 | Set QoS = 1 cho event quan trọng |

---

## Kết

Cuốn HDSD này là **bản vẽ tích hợp**, không phải tutorial từ A-Z. Mỗi pattern khi đem áp dụng vào dự án này đều cần **điều chỉnh ít nhiều** so với bản gốc — đặc biệt là pinout (đã quy chuẩn theo `pin-mapping.md`) và power tree (đã thêm rail SIM800L riêng).

**Thứ tự build đề xuất**: Phase 1 → 2 → 3 (test riêng MCU) → 5 → 6 → 4 → 7 → 8 → 9 → 10 → 11 → 12.

**Đừng skip phase nào**. Test pass Phase N rồi mới qua Phase N+1. Đó là cách duy nhất để khi cuối cùng cắm tất cả lại, hệ thống không sập vì 1 bug chôn sâu ở phase đầu.

— *Ryan, 2026-05-18*
