# Drive Train Specification

> Hardware spec cho 2WD differential drive — H-bridge L298N + 2× BO motor + bánh xe 65mm. Pattern Watney chassis.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 5 (HDSD)

---

## 1. Topology — Differential drive 2WD

```
     ┌──── Caster wheel (front) ────┐
     │                              │
   ───                            ───
  │ L │ ←── motor L              │ R │ ←── motor R
   ─┬─                            ─┬─
    │                              │
    │      L298N H-bridge          │
    │   ┌──────────────────────┐  │
    └──►│OUT1     OUT3◄────────┘
        │OUT2     OUT4
        │                       │
        │  ENA  IN1  IN2 IN3  IN4  ENB
        └───┬────┬───┬───┬────┬───┬─┘
            │    │   │   │    │   │
         GPIO6   7   8   9   10  11

       VS ←── 12V battery (motor power)
       VSS ←── 5V (logic — L298N internal LDO)
```

**Differential drive math**: cùng tốc độ 2 bánh → đi thẳng; ngược chiều → quay tại chỗ; tốc độ khác nhau → đi cong.

---

## 2. Component selection — L298N vs alternatives

### 2.1 So sánh H-bridge options

| Driver | Current cont | Voltage drop | Cost | Pro | Con |
|--------|--------------|--------------|------|-----|-----|
| **L298N** | 2A/channel | 2.5V (BJT) | 25k VND module | Sẵn có, robust, đơn giản | Voltage drop lớn → motor yếu hơn 20% |
| TB6612FNG | 1.2A/channel | 0.5V (MOSFET) | 30k VND module | Hiệu suất tốt | Current giới hạn 1.2A |
| DRV8833 | 1.5A/channel | 0.7V (MOSFET) | 20k VND module | Compact, hiệu suất | Cần PCB tự thiết kế |

**Quyết định**: dùng **L298N module** (Shopee). Lý do:
- BO motor stall ~1.5A → cần ≥ 1.2A per channel, L298N dư
- Robot không cần leo dốc → 20% loss chấp nhận được
- Module Shopee có sẵn heatsink, terminal block — không cần tự thiết kế PCB cho phần này

### 2.2 Motor — BO motor

BO motor (Battery Operated motor) loại **6V DC có hộp số 1:48**:
- Tốc độ no-load: 200 RPM
- Stall current: 1.5A @ 6V
- Torque stall: 0.8 kg·cm
- Trục bánh 5.5mm + 2-flat (chuẩn bánh 65mm)

**Chọn BO motor có encoder** nếu muốn closed-loop control sau này (hall sensor 2 channel, không cần Phase 5 nhưng để dành option).

---

## 3. Pin mapping

| L298N pin | ESP32 GPIO | Function | LEDC ch / timer |
|-----------|------------|----------|-----------------|
| ENA | GPIO6 | PWM motor A (left) speed | **CHANNEL_4 / TIMER_2** |
| IN1 | GPIO7 | Motor A direction bit 0 | GPIO OUT |
| IN2 | GPIO8 | Motor A direction bit 1 | GPIO OUT |
| IN3 | GPIO9 | Motor B direction bit 0 | GPIO OUT |
| IN4 | GPIO10 | Motor B direction bit 1 | GPIO OUT |
| ENB | GPIO11 | PWM motor B (right) speed | **CHANNEL_5 / TIMER_2** |
| VS | 12V rail | Motor power | — |
| VSS | 5V rail | Logic supply | — |
| GND | GND | — | — |

### 3.1 ⚠️ Deviation từ HDSD

HDSD section "Phase 5" ghi LEDC channel 0/1 cho motor. **Sai** — channel 0 đã claim cho camera XCLK (Phase 3). LEDC resource map đã chốt:

| Timer | Channels | Use |
|-------|----------|-----|
| TIMER_0 | CH 0 | Camera XCLK (Phase 3) |
| TIMER_1 | CH 2-3 | Servo PTZ (Phase 4) |
| **TIMER_2** | **CH 4-5** | **Motor L298N (Phase 5)** ← deviation |

Mỗi timer cấp riêng tần số. Motor PWM dùng **20kHz** (cao hơn ngưỡng âm thanh nghe được — motor sẽ êm hơn so với 1kHz phổ biến).

### 3.2 PWM frequency selection

| Freq | Pro | Con |
|------|-----|-----|
| 1 kHz | Simple, mặc định nhiều thư viện | Motor kêu rít (audible ~1kHz) |
| 5 kHz | Bớt rít | Vẫn nghe được |
| **20 kHz** | **Trên ngưỡng nghe (silent)** | Heat L298N tăng nhẹ |
| 50+ kHz | Êm hoàn hảo | L298N không phản ứng tốt — switching loss lớn |

Chọn **20 kHz** — đặc biệt quan trọng vì robot care-elderly không nên ồn.

---

## 4. Direction truth table (L298N)

| IN1 | IN2 | Motor A | IN3 | IN4 | Motor B |
|-----|-----|---------|-----|-----|---------|
| 0 | 0 | Coast (free spin) | 0 | 0 | Coast |
| 0 | 1 | Reverse | 0 | 1 | Reverse |
| 1 | 0 | Forward | 1 | 0 | Forward |
| 1 | 1 | Brake (short) | 1 | 1 | Brake |

- **Forward**: motor quay theo hướng "tiến" (định nghĩa khi lắp ráp, có thể đảo dây nếu sai)
- **Brake**: dừng nhanh bằng cách short motor → energy dump qua diode bảo vệ
- **Coast**: thả lỏng, motor dừng từ từ do ma sát

Robot dùng **Brake mode** để dừng nhanh (an toàn người già). Coast chỉ dùng khi power-off.

---

## 5. Differential drive math

Input từ user: `(linear, angular)` mỗi cái -100 đến +100.

```
left_motor  = linear + angular
right_motor = linear - angular

if (left  > 100) left  = 100
if (left  < -100) left  = -100
if (right > 100) right = 100
if (right < -100) right = -100
```

**Ví dụ**:
- (linear=100, angular=0) → cả 2 bánh +100 → đi thẳng full speed
- (linear=0, angular=100) → L=+100, R=-100 → quay tại chỗ CW full speed
- (linear=50, angular=50) → L=100, R=0 → cong sang phải
- (linear=-50, angular=0) → L=R=-50 → lùi nửa tốc độ

### 5.1 Dead zone compensation

BO motor không quay được < 20% PWM (ma sát + voltage drop L298N). Map linear:

```
real_pwm = (input_pwm == 0) ? 0 : DEAD_ZONE + (abs(input_pwm) * (100 - DEAD_ZONE) / 100)
```

Với DEAD_ZONE = 20%, input 1-100% → output 20-100%.

### 5.2 Trim balance

2 motor BO không giống hệt nhau (sai số ±5% RPM). Khi đi thẳng full speed, robot có thể lệch trái/phải 10°/m. Trim:

```
left_final  = left  * (1 + left_trim_pct / 100)
right_final = right * (1 + right_trim_pct / 100)
```

Trim ±10% là đủ, lưu vào NVS.

---

## 6. Safety — Watchdog timeout

**Vấn đề**: client gửi `/drive/forward?speed=80` rồi mất kết nối WiFi → robot vẫn chạy tiếp đến đụng tường.

**Solution**: task_navigation tick 50Hz. Mỗi khi nhận command, lưu timestamp. Nếu **không có command mới trong 500ms** → **brake all motors**.

```c
typedef struct {
    int8_t linear;          // -100..+100
    int8_t angular;         // -100..+100
    uint32_t timestamp_ms;  // ESP timer tick
} drive_cmd_t;

static drive_cmd_t s_cmd;

void task_nav(void *arg) {
    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - s_cmd.timestamp_ms > 500) {
            motor_stop_all();   // brake
        } else {
            apply_diff_drive(s_cmd.linear, s_cmd.angular);
        }
        vTaskDelay(pdMS_TO_TICKS(20));   // 50Hz
    }
}
```

Client phải gửi command **liên tục** (mỗi 100-200ms) khi muốn move. Phù hợp UX joystick.

### 6.1 Continuous commands

Client UI (web joystick) gửi:
- Khi user kéo joystick → gửi `/drive/velocity?linear=X&angular=Y` mỗi 100ms
- Khi user thả → dừng gửi → watchdog kick in sau 500ms → robot brake

---

## 7. Snubber RC trên motor

Motor DC chổi than sinh **back-EMF spike** khi tắt → có thể chết H-bridge hoặc gây EMI.

L298N có internal flyback diode (8 con, 1 cho mỗi half-bridge) nhưng vẫn nên thêm:

**Snubber RC trên mỗi motor output** (4 chỗ):
- R = 100Ω 0.5W
- C = 100nF X7R 50V
- Mắc từ OUT1↔OUT2 và OUT3↔OUT4

```
   OUT1 ──┬── motor ──┬── OUT2
          │           │
        100Ω        100Ω
          │           │
        100nF       100nF
          │           │
         GND         GND
```

Snubber:
- Hấp thụ spike
- Giảm EMI radiated từ brush sparking
- Bảo vệ MCU khỏi nhiễu

---

## 8. Power & current

### 8.1 Current per motor

| Mode | Current per motor |
|------|-------------------|
| Idle (PWM=0, brake) | ~50 mA (chỉ leak) |
| Cruise (50% PWM, no load) | 200-300 mA |
| Stall (kéo vật chắn) | 1.5 A |
| Brake hard | ~100 mA (energy dump qua diode) |

### 8.2 Battery draw

2 motor cruise: 2 × 0.3A × 6V = 3.6W → từ 12V battery: 0.3A
2 motor stall: 2 × 1.5A × 6V = 18W → từ 12V battery: 1.5A (peak ngắn)

Đã verify trong `power_budget.py`: 12V rail 8A fuse, peak 4A motor = đủ headroom 2×.

### 8.3 Motor voltage

L298N output ~12V minus 2.5V BJT drop = **9.5V cho motor BO** (motor spec 6V).

**Cảnh báo**: 9.5V trên motor BO 6V có thể giảm tuổi thọ. Options:
- **Limit PWM to 60-70% max** trong firmware (giả lập 6V) → đơn giản nhất
- Dùng buck riêng 6V cho motor → phức tạp + thêm BOM
- Dùng motor 12V (như motor JGA25) → đắt hơn

Phase 5 chọn option 1: **limit PWM max = 70%** trong driver.

```c
#define PWM_MAX_PCT  70   // L298N drop 2.5V → 9.5V; limit 70% ≈ 6.65V cho motor
```

---

## 9. Encoder (optional, không Phase 5)

BO motor "có encoder" có 2 chân hall sensor cho mỗi motor. Đếm xung → tính khoảng cách + closed-loop control.

**Pin**:
- Left encoder A: GPIO1 (ADC1_CH0 — nhưng cũng dùng được digital input pulse counter)
- Left encoder B: ?
- Right encoder A: ?
- Right encoder B: ?

Để dành cho Phase 11+ (nếu cần precision đi). Phase 5 dùng **open-loop** (set PWM, không feedback).

---

## 10. Schematic verification

Kiểm tra `motor_driver.kicad_sch`:

- [ ] L298N module connector 11-pin có:
  - ENA → GPIO6, IN1 → GPIO7, IN2 → GPIO8
  - IN3 → GPIO9, IN4 → GPIO10, ENB → GPIO11
  - VS → +12V (trực tiếp battery, sau fuse 3A)
  - VSS → +5V rail
  - GND → GND
- [ ] Snubber RC trên 4 output (OUT1-OUT2, OUT3-OUT4) — optional but recommended
- [ ] Diode 1N5822 (3A Schottky) flyback song song mỗi motor — backup nếu L298N internal diode fail
- [ ] Fuse PTC 3A trên đường +12V vào L298N
- [ ] Test point: ENA, ENB (đo PWM bằng oscilloscope khi debug)
- [ ] 100µF + 100nF decoupling trên VS pin

---

## 11. BOM

| Item | Qty | Source | Cost ước |
|------|-----|--------|----------|
| L298N module (có heatsink + terminal block) | 1 | Shopee | 25,000 VND |
| BO motor 6V có encoder + bracket + bánh xe 65mm | 2 set | Shopee combo | 50,000 × 2 |
| Snubber resistor 100Ω 0.5W | 4 | Shopee | 500 × 4 |
| Snubber cap 100nF X7R 50V | 4 | Shopee | 200 × 4 |
| Flyback diode 1N5822 (optional) | 2 | Shopee | 1,000 × 2 |
| Fuse PTC 3A | 1 | Shopee | 2,500 |
| Cáp nối motor đến L298N | 4× 20cm | Shopee | 5,000 |
| **TOTAL** | | | **~135,000 VND** |

---

## 12. Next phase

Sau Phase 5 pass (robot di chuyển tiến/lùi/quay theo UI):
→ **Phase 6**: Sensor suite (MPU6050 IMU + 4× HC-SR04 ultrasonic) → enable obstacle avoidance trong navigation task.
