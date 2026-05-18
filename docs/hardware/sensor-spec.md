# Sensor Suite Specification

> IMU MPU6050 + 4× HC-SR04 ultrasonic. Cốt lõi cho fall detection và obstacle avoidance.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 6 (HDSD)

---

## 1. Sensor list

| Sensor | Vị trí | Interface | Address | Mục đích |
|--------|--------|-----------|---------|----------|
| MPU6050 GY-521 | Trên main PCB | I2C | 0x68 | Fall detection + heading |
| HC-SR04 #1 | Front mặt robot | TRIG/ECHO 35/36 | — | Obstacle phía trước |
| HC-SR04 #2 | Back mặt robot | TRIG/ECHO 37/38 | — | Obstacle phía sau |
| HC-SR04 #3 | Left mặt robot | TRIG/ECHO 39/40 | — | Obstacle bên trái |
| HC-SR04 #4 | Right mặt robot | TRIG/ECHO 42/43 | — | Obstacle bên phải |
| SSD1306 OLED (Phase 10 dùng) | Layer 1 top | I2C | 0x3C | Status display |

---

## 2. I2C bus design

### 2.1 Wiring

```
ESP32-S3 GPIO21 (SDA) ──┬── R 4.7kΩ ── +3V3
                        │
                        ├── MPU6050 SDA
                        └── SSD1306 SDA (Phase 10)

ESP32-S3 GPIO22 (SCL) ──┬── R 4.7kΩ ── +3V3
                        │
                        ├── MPU6050 SCL
                        └── SSD1306 SCL (Phase 10)
```

### 2.2 Pull-up calculation

Fast Mode I2C 400kHz, 2 device, ~200pF bus capacitance:

```
R_max = 1000ns / (0.847 × Cb) = 1000 / (0.847 × 200pF) = 5.9kΩ
R_min = (Vcc - Vol) / IOL = (3.3 - 0.4) / 3mA = 0.97kΩ

Choose: R = 4.7kΩ (E24)
```

### 2.3 Bus speed

ESP32-S3 i2c master driver supports up to 1MHz. Chọn **400kHz** Fast Mode — chuẩn, mọi sensor 3-5V đều support.

### 2.4 Why GPIO21 + GPIO22?

ESP-IDF default `I2C_NUM_0`. Khác pin không sao, nhưng:
- GPIO21/22 không phải strapping pin
- Không xung đột với DVP camera, USB, JTAG
- Available trên cả ESP32-S3 generic và ESP32-S3-CAM variants

---

## 3. MPU6050 configuration

### 3.1 Registers cần config

| Register | Address | Value | Mục đích |
|----------|---------|-------|----------|
| PWR_MGMT_1 | 0x6B | 0x00 | Wake from sleep, internal 8MHz osc |
| CONFIG | 0x1A | 0x03 | DLPF 44Hz (cắt rung động motor) |
| GYRO_CONFIG | 0x1B | 0x10 | Full scale ±1000 deg/s (LSB 32.8) |
| ACCEL_CONFIG | 0x1C | 0x10 | Full scale ±8g (LSB 4096) |
| SMPLRT_DIV | 0x19 | 0x09 | Sample rate 100Hz (1000/(1+9)) |

### 3.2 Read sequence

Đọc 14 byte liên tiếp từ register 0x3B (ACCEL_XOUT_H):
- 0x3B-0x40: accel X, Y, Z (6 byte, big-endian int16)
- 0x41-0x42: temp (skip)
- 0x43-0x48: gyro X, Y, Z (6 byte)

### 3.3 LSB → physical units

```c
accel_g = raw_accel / 4096.0f;       // ±8g range
gyro_dps = raw_gyro / 32.8f;          // ±1000 dps range
```

### 3.4 Calibration offsets

Sai số factory ~ 50mg accel, 5dps gyro. Hiệu chuẩn bằng cách:
1. Đặt MPU6050 đứng yên, mặt phẳng, **az hướng lên**
2. Đọc 100 mẫu, trung bình → bias
3. Bias bias_z phải gần +1g → offset_z = (avg - 4096) (raw)
4. Lưu vào NVS

```c
// Apply offsets
ax = raw_ax - offset_ax;
ay = raw_ay - offset_ay;
az = raw_az - offset_az - 4096;  // ground offset = 1g
```

---

## 4. Fall detection algorithm

Pattern: spike detection + orientation check. Tránh false positive khi robot va vào dốc hay xóc nhẹ.

### 4.1 State machine

```
[NORMAL]
   │ (|a| > 2.5g for any 50ms window)
   ▼
[SPIKE_DETECTED]
   │ wait 500ms then check
   ▼
[ORIENTATION_CHECK]
   │ tilt > 60° from upright?
   │
   ├── YES ──► [FALL_CONFIRMED] → raise event bit
   │
   └── NO ───► back to [NORMAL]
```

### 4.2 Tilt calculation

```
pitch = atan2(ax, sqrt(ay² + az²)) × 180 / π
roll  = atan2(ay, sqrt(ax² + az²)) × 180 / π
tilt_total = sqrt(pitch² + roll²)
```

Robot bình thường đặt đứng → tilt ~ 0°. Khi té ngang → tilt 80-90°.

### 4.3 Cooldown

Sau khi raise FALL event, **không trigger lại trong 30 giây** — tránh spam SOS khi robot vẫn đang nằm. Reset bằng cách robot dựng lại đứng (tilt < 30° liên tục 10s).

### 4.4 False positive guards

- Magnitude check: |a| > 2.5g (không phải 2g) để loại trừ rung động motor
- Window check: spike phải kéo dài > 50ms (50 sample @ 1kHz, hoặc 5 sample @ 100Hz)
- Orientation gate: chỉ confirm nếu tilt > 60°

---

## 5. HC-SR04 hardware

### 5.1 Voltage divider on Echo

Echo output 5V → ESP32-S3 input 3.3V max → cần chia áp:

```
        +5V Echo ──┐
                   │
                  R1 1kΩ
                   │
                   ├────── ESP32-S3 GPIO (3.33V max)
                   │
                  R2 2kΩ
                   │
                  GND
```

V_gpio = 5V × 2kΩ / (1kΩ + 2kΩ) = 3.33V ✓

**4 sensor × 2 resistor = 8 voltage dividers** trên PCB.

### 5.2 TRIG signal

ESP32-S3 GPIO output 3.3V → HC-SR04 TRIG. Min HIGH 2V → 3.3V đủ.
Trigger pulse: 10µs HIGH, sau đó LOW.

### 5.3 Echo measurement

Đợi Echo pin HIGH, đo thời gian Echo HIGH:
- Pulse width 150µs → distance 2.6cm
- Pulse width 25ms → distance 430cm
- > 38ms = out of range (sensor returns 0 = invalid)

```c
distance_cm = pulse_width_us / 58
```

Lý thuyết: âm thanh 340m/s, đi-về 2× → 1cm tương đương 58µs.

### 5.4 Cross-talk avoidance

Nếu 2 sensor cùng phát trigger → echo có thể lẫn nhau (sensor F nghe được echo từ trigger của L).

**Solution**: round-robin 4 sensor, mỗi lần đo 1 sensor, đợi 30ms trước khi đo sensor tiếp theo.
- Tổng cycle = 4 × 30ms = 120ms ≈ 8Hz update rate per sensor.
- 8Hz đủ cho navigation (robot tốc độ 0.3m/s → 1 cycle = 3.75cm di chuyển).

### 5.5 Range & accuracy

- Range: 2cm - 400cm
- Accuracy: ±3mm (typical)
- Beam width: ~30° cone
- Min range: < 2cm sensor không đọc được (echo về quá sớm)

---

## 6. Obstacle avoidance logic

Hook vào `task_navigation`:

```c
// In nav_task tick:
sensor_state_t state;
xQueuePeek(sensor_queue, &state, 0);

int linear = s_cmd.linear;
int angular = s_cmd.angular;

const int OBSTACLE_BRAKE_CM = 15;
const int OBSTACLE_SLOW_CM = 40;

// Brake forward if front obstacle close
if (linear > 0 && state.dist_front < OBSTACLE_BRAKE_CM) {
    linear = 0;
}
// Slow forward if obstacle medium distance
else if (linear > 0 && state.dist_front < OBSTACLE_SLOW_CM) {
    linear = linear * state.dist_front / OBSTACLE_SLOW_CM;
}

// Same for backing up
if (linear < 0 && state.dist_back < OBSTACLE_BRAKE_CM) linear = 0;
else if (linear < 0 && state.dist_back < OBSTACLE_SLOW_CM)
    linear = linear * state.dist_back / OBSTACLE_SLOW_CM;

// Angular OK regardless of obstacle (rotate in place to find clearance)
```

### 6.1 Allow rotation

Khi obstacle phía trước, vẫn cho phép rotate để robot tự thoát. Đây là behavior key — không chặn cứng tất cả motion.

### 6.2 Side sensors

Left/Right không gate motion (robot có thể đi sát tường), nhưng publish event cho behavior layer dùng (Phase 10+).

---

## 7. Sensor fusion task

20Hz loop:

```c
void task_sensor_fusion(void *arg) {
    sensor_state_t state = {0};
    while (1) {
        // IMU (~1ms)
        mpu6050_read(&state.accel_x, &state.accel_y, &state.accel_z, ...);
        compute_orientation(&state);
        state.fall_detected = check_fall(&state);

        // Ultrasonic round-robin
        static int ult_idx = 0;
        switch (ult_idx) {
            case 0: state.dist_front = hcsr04_read(0); break;
            case 1: state.dist_back  = hcsr04_read(1); break;
            case 2: state.dist_left  = hcsr04_read(2); break;
            case 3: state.dist_right = hcsr04_read(3); break;
        }
        ult_idx = (ult_idx + 1) % 4;

        state.timestamp_us = esp_timer_get_time();
        xQueueOverwrite(sensor_queue, &state);   // single-slot, latest-wins

        if (state.fall_detected) {
            xEventGroupSetBits(robot_events, EVT_FALL_DETECTED);
        }

        vTaskDelay(pdMS_TO_TICKS(50));   // 20Hz
    }
}
```

### 7.1 Queue strategy: overwrite single-slot

Sensor consumer (navigation, OLED, MQTT) chỉ cần biết state **latest** — không cần lịch sử. `xQueueOverwrite` với queue depth 1 = atomic publish.

### 7.2 Event group bits

```c
#define EVT_FALL_DETECTED    BIT0
#define EVT_OBSTACLE_FRONT   BIT1
#define EVT_OBSTACLE_BACK    BIT2
#define EVT_OBSTACLE_SIDE    BIT3
#define EVT_BATTERY_LOW      BIT4
#define EVT_USER_PRESENT     BIT5    // (Phase 7+ vision)
```

Behavior layer (Phase 10+) subscribe bằng `xEventGroupWaitBits`.

---

## 8. Schematic verification

Kiểm tra `sensors.kicad_sch`:

- [ ] MPU6050 module 6-pin connector (VCC=3.3V, GND, SDA, SCL, XDA, XCL nhưng XDA/XCL không cần)
  - Hoặc IC LSM6DS3 nếu dùng custom (MPU6050 đã EOL nhưng Shopee vẫn đầy)
- [ ] R 4.7kΩ pull-up SDA → +3.3V
- [ ] R 4.7kΩ pull-up SCL → +3.3V
- [ ] AD0 pin của MPU6050 → GND (chọn address 0x68)
- [ ] 4× HC-SR04 connector 4-pin (VCC=5V, TRIG, ECHO, GND)
- [ ] 8× voltage divider (R1 1kΩ + R2 2kΩ) trên 4 đường Echo
- [ ] TVS diode optional trên đường Echo (chống ESD)
- [ ] 100nF decoupling trên VCC của mỗi HC-SR04 module
- [ ] Test point: SDA, SCL (debug I2C bằng logic analyzer)

---

## 9. BOM

| Item | Qty | Source | Cost ước |
|------|-----|--------|----------|
| MPU6050 GY-521 module | 1 | Đã có (Sesame inventory) | 0 |
| HC-SR04 ultrasonic | 4 | Shopee | 15k × 4 = 60k VND |
| Resistor 1kΩ 1% 0603 | 4 | Shopee | 100 × 4 |
| Resistor 2kΩ 1% 0603 | 4 | Shopee | 100 × 4 |
| Resistor 4.7kΩ 1% 0603 | 2 | Shopee | 100 × 2 |
| Cap 100nF X7R 0603 | 4 | (đã trong stock decoupling) | — |
| Connector header 4-pin 2.54mm | 4 | Shopee | 1k × 4 |
| **TOTAL** | | | **~70,000 VND** |

---

## 10. Next phase

Sau Phase 6 pass (sensor đo đúng, obstacle gate hoạt động, fall detect không false positive):
→ **Phase 7**: Audio I/O (INMP441 mic + MAX98357A amp + voice pipeline)
