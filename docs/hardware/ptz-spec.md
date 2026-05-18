# PTZ Pan-Tilt Specification

> Hardware spec cho cụm camera Pan-Tilt — 2× servo MG90S điều khiển bằng PWM 50Hz từ ESP32-S3 LEDC peripheral.
>
> **Pattern source**: [Rudra](https://github.com/aceta-minophen/Rudra) 2-servo PTZ mount, port driver sang ESP-IDF LEDC API.
>
> **Components owned**: 8× MG90S từ Sesame Robot — dùng 2 cho PTZ, còn 6 dự phòng.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 4 (HDSD)

---

## 1. Servo selection — MG90S vs SG90

| Spec | SG90 (nhựa) | MG90S (kim loại) |
|------|-------------|---------------------|
| Trọng lượng | 9g | 13.4g |
| Torque @ 4.8V | 1.8 kg·cm | 1.8 kg·cm |
| Torque @ 6.0V | 2.2 kg·cm | 2.2 kg·cm |
| Dead band | 7µs | 5µs |
| Gear material | Nhựa POM | Đồng |
| Tuổi thọ stall | Thấp (gãy bánh răng) | Cao |
| Giá | 25-30k VND | 40-50k VND |
| Có sẵn | — | **Có 8 con từ Sesame** |

**Quyết định**: Dùng **MG90S**. Kim loại bền hơn cho ứng dụng care-elderly chạy hàng tháng. Có sẵn nên không tốn.

### Pinout MG90S (cùng SG90)

| Wire color | Function | ESP32-S3 connection |
|------------|----------|---------------------|
| Orange/Yellow | PWM signal | GPIO44 (pan) / GPIO45 (tilt) |
| Red | +5V power | 5V rail |
| Brown/Black | GND | GND chung |

### PWM signal spec

| Tham số | Value |
|---------|-------|
| Frequency | 50Hz (period 20ms) |
| Pulse width 0° | 1.0ms |
| Pulse width 90° | 1.5ms |
| Pulse width 180° | 2.0ms |
| Signal level | 3.3V (logic OK với MG90S 5V) |

**Lưu ý**: Một số MG90S spec PWM rộng hơn (0.5ms - 2.5ms cho 180°). Test thực tế khi calibrate.

---

## 2. Pin mapping & power

Theo `pin-mapping.md`:

| Servo | PWM GPIO | LEDC channel | LEDC timer | Power |
|-------|----------|--------------|------------|-------|
| Pan | GPIO44 | CHANNEL_2 | TIMER_1 | +5V chung |
| Tilt | GPIO45 | CHANNEL_3 | TIMER_1 | +5V chung |

**LEDC resource map** (không xung đột):
- TIMER_0 / CHANNEL_0 → Camera XCLK (Phase 3)
- TIMER_1 / CHANNEL_2-3 → Servo PTZ (Phase 4)
- TIMER_2 / CHANNEL_4-5 → Motor PWM L298N (Phase 5)

### Power budget (đã verified `power_budget.py`)

| Operating mode | Pan + Tilt total |
|----------------|-------------------|
| Idle (holding position) | 160 mA (5V) |
| Moving smooth | 300-400 mA |
| Stall (mechanical block) | **1.2 A peak** |

5V rail budget: 3A → headroom đủ. **Nhưng**: khi cả 2 servo stall đồng thời + camera + audio loud → tổng peak 2.5A, sát limit. Firmware **không cho phép** servo move khi audio playback level cao (mutex).

---

## 3. Mechanical mount

### 3.1 PTZ mount 3D-print

**File STL**: `mechanical/3d-models/ptz-mount-pan.stl`, `ptz-mount-tilt.stl`

**Material**: PLA hoặc PETG 0.2mm layer, 30% infill — đủ cứng cho khối lượng camera ESP32-S3-CAM ~15g.

**Cấu trúc**:
```
        ┌────────────┐
        │  Camera    │ ← ESP32-S3-CAM module
        │  ESP32-S3  │   gắn trên horn tilt
        └─────┬──────┘
              │
        ┌─────┴──────┐
        │ Tilt servo │ ← MG90S #2 (tilt)
        │   MG90S    │   thân lắp vào L-bracket
        └─────┬──────┘   horn quay 30° → 150° (giới hạn)
              │
        ┌─────┴──────┐
        │  L-bracket │ ← Plastic mount nối tilt servo
        │            │   với horn của pan servo
        └─────┬──────┘
              │
        ┌─────┴──────┐
        │ Pan servo  │ ← MG90S #1 (pan)
        │   MG90S    │   thân xuyên qua lỗ chassis 22×12mm
        └─────┬──────┘   horn quay 0° → 180°
              │
        ═════════════ Chassis layer 1
```

### 3.2 Mechanical limits

| Axis | Range hardware | Soft limit firmware | Lý do soft limit |
|------|----------------|---------------------|------------------|
| Pan | 0° - 180° | 10° - 170° | Tránh cáp camera xoắn |
| Tilt | 0° - 180° | 30° - 150° | Tránh camera đụng chassis (dưới) hoặc nhìn trần (trên) |

**Đo lại sau khi lắp**: cho phép servo quay từ từ tới giới hạn → đánh dấu vị trí góc thực bị block → set soft limit thấp hơn 5°.

### 3.3 Anti-jitter

MG90S dao động nhỏ (~0.5°) khi giữ vị trí — bình thường. Nếu rung lớn (> 2°):
- Cấp 5V không đủ áp (sụt khi load) → tăng cap output buck #1
- Mass camera quá nặng so với torque MG90S → giảm tốc move
- PWM signal nhiễu → thêm filter LP 1kHz (R 1kΩ + C 100nF) gần servo

---

## 4. LEDC PWM math

### 4.1 Resolution selection

ESP-IDF LEDC supports 1-20 bit. Chọn **14-bit** (16384 ticks):
- 20ms period / 16384 ticks = 1.22µs per tick
- Resolution angle: 1.22µs × 180° / 1000µs = 0.22° per tick
- Đủ mượt cho mắt người (< 1° không thấy step)

### 4.2 Duty calculation

```c
// LEDC tick @ 50Hz, 14-bit = 16384 ticks / 20ms = 819.2 ticks/ms
#define LEDC_TICKS_PER_MS  819
#define PULSE_MIN_US   1000   // 1.0ms = 0°
#define PULSE_MAX_US   2000   // 2.0ms = 180°

uint32_t angle_to_duty(uint8_t angle) {
    // 0° → 1.0ms → 819 ticks
    // 180° → 2.0ms → 1638 ticks
    uint32_t pulse_us = PULSE_MIN_US + ((uint32_t)angle * (PULSE_MAX_US - PULSE_MIN_US)) / 180;
    return (pulse_us * LEDC_TICKS_PER_MS) / 1000;
}
```

Verify:
- angle=0 → pulse=1000µs → duty = 1000 × 819 / 1000 = 819 ✓
- angle=90 → pulse=1500µs → duty = 1228 ✓
- angle=180 → pulse=2000µs → duty = 1638 ✓

### 4.3 Calibration offset

Mỗi MG90S khác nhau ~5° do dung sai sản xuất. Lưu offset vào NVS:

```c
// Offset persisted in NVS for each servo
int8_t pan_offset_deg;    // typically -5 to +5
int8_t tilt_offset_deg;

// Applied during write:
servo_set_raw(SERVO_PAN, target + pan_offset_deg);
```

User calibrate qua web UI: di chuyển servo đến đúng 90° thực tế, ghi offset.

---

## 5. Smooth motion strategy

### 5.1 Tại sao cần smooth?

Set duty trực tiếp = servo move với tốc độ MAX → giật bánh răng + dòng peak cao + camera blur.

### 5.2 Linear interpolation

Tại 50Hz update rate:
- 90° change = 1.8s default
- Tốc độ default: 50°/giây
- Speed adjustable: 10°/s (slow) → 200°/s (fast)

```c
// PTZ task @ 50Hz
void task_ptz(void *arg) {
    while (1) {
        for (int i = 0; i < 2; i++) {  // pan, tilt
            int diff = ptz_state[i].target - ptz_state[i].current;
            int step = ptz_state[i].speed_deg_per_sec / 50;  // 50Hz tick
            if (abs(diff) <= step) {
                ptz_state[i].current = ptz_state[i].target;
            } else {
                ptz_state[i].current += (diff > 0) ? step : -step;
            }
            servo_set_raw(i, ptz_state[i].current);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

### 5.3 Easing (optional)

Linear motion vẫn cảm giác robot. Easing function (ease-in-out cubic) cho cảm giác tự nhiên hơn — implement sau Phase 4 cơ bản pass.

---

## 6. HTTP control API

Endpoints (mở rộng smoke_test web server):

| Endpoint | Method | Body/Query | Response |
|----------|--------|------------|----------|
| `/ptz/pan?angle=N` | GET | N: 0-180 | `{"ok":true,"target":N}` |
| `/ptz/tilt?angle=N` | GET | N: 0-180 | `{"ok":true,"target":N}` |
| `/ptz/center` | GET | — | Move cả 2 về 90 |
| `/ptz/park` | GET | — | Pan=90, Tilt=80 (camera nhìn xuống nhẹ, parked) |
| `/ptz/state` | GET | — | `{"pan":N,"tilt":N,"pan_target":N,"tilt_target":N}` |
| `/ptz/calibrate?pan_offset=X&tilt_offset=Y` | POST | X, Y: -10 to +10 | Lưu offset vào NVS |
| `/ptz/speed?dps=N` | GET | N: 10-200 | Set tốc độ degrees per second |

### 6.1 Bảo vệ

- Validate input: 0 ≤ angle ≤ 180, clamp về soft limit
- Rate limit: max 5 request/giây để không spam queue
- Atomic update: mutex bảo vệ ptz_state

---

## 7. BOM

| Item | Qty | Source | Cost |
|------|-----|--------|------|
| MG90S servo | 2 | Sesame Robot inventory | 0 (đã có) |
| PTZ mount 3D-print PLA | 1 set | In tại chỗ hoặc gửi gia công | ~50k VND |
| Cáp jumper 3-pin 30cm | 2 | Shopee | 5k × 2 |
| L-bracket plastic kèm SG90 servo set | 1 | Shopee (~30k cho cả combo) | — |
| Vít M2×6 + đai ốc | 8 | Shopee | <5k |
| **TOTAL** | | | **~70k VND** |

---

## 8. Schematic verification

Kiểm tra `mcu_core.kicad_sch` hoặc sheet PTZ:

- [ ] Connector 3-pin J_PAN (Signal, +5V, GND) — male header 2.54mm
- [ ] Connector 3-pin J_TILT
- [ ] Signal pin nối GPIO44 (pan), GPIO45 (tilt) — không thêm pull-up/pull-down (servo input có internal Schmitt)
- [ ] +5V trên connector PAN+TILT lấy từ rail 5V chung
- [ ] Optional: R 1kΩ + C 100nF LP filter trên đường PWM signal (giảm jitter)
- [ ] Optional: D1 1N4148 từ +5V xuống GND song song với servo VCC (catch back-EMF khi servo brake)

---

## 9. Limitations & future upgrade

### 9.1 MG90S hạn chế

- Không có position feedback (open-loop control)
- Position drift ~1% sau nhiều lần move
- Không thể detect stall

### 9.2 Upgrade path

Nếu cần precision cao hơn cho mục đích nâng cao (auto-track face):
- Đổi MG90S → MG996R (torque cao hơn, vẫn open-loop)
- Hoặc dùng **smart servo Dynamixel XL-320** (~500k/con) — có position feedback I2C
- Hoặc thêm encoder externe trên trục pan/tilt — phức tạp

Phase 4 cơ bản dùng MG90S là đủ.
