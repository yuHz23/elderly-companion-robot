# Dock Station + Charging Specification

> Tổ hợp **dock station tại tường** + **robot-side dock interface** + **state machine docking**. Khi pin yếu, robot tự tìm dock và sạc lại.
>
> **Pattern**: Watney chassis + Caretaker spring contact + osrf/autodock state machine.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 9 (HDSD)

---

## 1. Topology

### 1.1 Hệ tổng quan

```
        ┌──── WALL DOCK STATION ────┐
        │                            │
        │   AC-DC adapter           │
        │   220V → 12.6V / 3A       │
        │              │             │
        │   ┌──────────┘             │
        │   │    Fuse + diode        │
        │   │                        │
        │   ▼                        │
        │ COPPER PLATE +   │         │
        │   ░░░░░░         │ 60mm    │
        │                  │         │
        │ COPPER PLATE -   │ 30mm    │
        │   ░░░░░░         │         │
        │                  ▼         │
        │       ◯  IR LED 940nm     │
        │          + 38kHz mod       │
        └────────────────────────────┘
                  ▲ ▲ ▲
                  │ │ │ (IR + RF coupling on touch)
                  │ │ │
        ┌────────────────────────────┐
        │  ROBOT (front side)        │
        │                            │
        │  SPRING CONTACT +  ◐       │
        │  SPRING CONTACT -  ◐       │  ← match plate spacing 30mm
        │                            │
        │     IR receiver TSOP38238  │
        │     ◉                      │
        │                            │
        │  → diode SS54 (block back) │
        │  → 3S BMS charger          │
        │  → battery 11.1V (3×18650) │
        └────────────────────────────┘
```

### 1.2 Robot tiếp cận

Robot **front-docks** (đầu đụng dock):
- Camera/PTZ trên đầu → khi dock, camera nhìn vào tường — chấp nhận trade-off
- Đơn giản hóa: không cần 180° turn để back-dock
- IR receiver + copper contact đều ở **mặt trước**

---

## 2. Dock station (xây ngoài robot)

### 2.1 Vị trí lắp

- Cố định tường, cao **~80mm** so với sàn → ngang tầm copper contact trên robot
- Khu vực front 50cm thoáng (cho phép robot tiếp cận)
- Nguồn điện 220V gần đó (cách 1-2m)
- Tránh ánh nắng mặt trời chiếu trực tiếp (nhiễu IR)

### 2.2 Cấu trúc cơ khí

```
DOCK PLATE (gỗ/acrylic 5mm hoặc in 3D PLA):
- Kích thước: 150 × 120 × 40mm (W × H × D)
- Bắt vít M3 vào tường

Mặt trước (hướng robot):
┌─────────────────────────────────┐
│                                  │   ← top of dock
│     COPPER PLATE +5V (60×30mm)   │
│     ░░░░░░░░░░░░░░░░░░░░░░░░    │   y = 50mm from base
│                                  │
│     COPPER PLATE GND  (60×30mm)  │
│     ░░░░░░░░░░░░░░░░░░░░░░░░    │   y = 15mm from base
│                                  │
│              ◯  IR LED          │   y = 75mm
│                                  │
└─────────────────────────────────┘
    └ AC adapter cable ↗ (back)
```

### 2.3 Copper plate

- Vật liệu: **đồng đỏ 0.5mm dày**, cắt theo 60×30mm
- Mạ thiếc bề mặt → chống oxide
- Khoan 2 lỗ M3 ở góc, bắt vít vào dock plate
- Hàn dây 18AWG silicone sau khi cố định

**Khoảng cách 2 plate (Y axis)**: **35mm tâm-tâm** (đảm bảo robot spring contact match)

**Y tolerance**: ± 5mm. Spring contact 3mm stroke đảm bảo tiếp xúc tốt trong window này.

### 2.4 IR beacon

#### LED 940nm

- IR LED 5mm 940nm, ~100mA forward current
- R series 220Ω giới hạn dòng từ 5V → I_LED ≈ (5 - 1.2) / 220 = 17mA (peak 50% duty 38kHz → average 8mA)
- Beam angle 30° — chiếu ra phía trước

#### 38kHz modulation

TSOP38238 receiver chỉ detect signal modulated 38kHz. **Phải có** modulator, không thể dùng IR LED thường.

3 options:

**Option A**: 555 timer tạo 38kHz (đơn giản nhất)
```
555 IC + R1 + R2 + C: f = 1.44 / ((R1 + 2×R2) × C)
Set f = 38kHz: R1 = 1kΩ, R2 = 8.2kΩ, C = 2.2nF
```

**Option B**: ESP8266 / Arduino Nano riêng tại dock chạy PWM 38kHz — quá overkill cho 1 LED
**Option C**: IR module pre-modulated bán sẵn Shopee — tiện nhất

Recommend **Option C** ("IR transmitter module 38kHz") ~10k VND.

### 2.5 Power supply tại dock

- AC-DC adapter **12.6V / 3A** (5×4 cells charging current ≤ 1.5A → 3A có headroom)
- Diode SS54 trên đường ra (chống dòng ngược)
- Fuse PTC 2A
- LED báo "POWER ON" 5mm xanh + R 1kΩ

### 2.6 Dock BOM

| Item | Qty | Source | Cost (VND) |
|------|-----|--------|------------|
| Acrylic 5mm 150×120mm laser-cut | 1 | Shop laser | 30,000 |
| Copper plate đồng đỏ 0.5mm 100×100mm | 1 | Shopee | 25,000 |
| IR LED 940nm 5mm | 1 | Shopee | 1,000 |
| Resistor 220Ω 1W | 1 | Shopee | 500 |
| 555 timer + R + C 38kHz module | 1 | Shopee "IR 38khz module" | 15,000 |
| AC-DC adapter 12.6V 3A | 1 | Shopee | 80,000 |
| Diode SS54 Schottky 5A | 1 | Shopee | 2,000 |
| Fuse PTC 2A | 1 | Shopee | 2,500 |
| LED xanh + R 1k indicator | 1 | Shopee | 1,000 |
| Dây silicone 18AWG đỏ + đen | 1m | Shopee | 5,000 |
| Vít M3 + nut bắt tường | x10 | Shopee | 5,000 |
| **DOCK TOTAL** | | | **~165,000 VND** |

---

## 3. Robot-side dock interface

### 3.1 Spring contact (front)

- Mua **spring-loaded contact pogo** kiểu test pin Shopee
- Stroke 3mm (đẩy ra khi không tiếp xúc, nén khi dock)
- Gold-plated tip → chống oxide
- 2 spring × 2 chỗ (đỏ + đen)
- Khoảng cách Y: **35mm tâm-tâm** match với dock plate

Hoặc DIY: gắn 2 mảnh đồng cong 30×10mm có spring lò xo nhỏ phía sau.

### 3.2 Wiring

```
Robot front:
   Spring + ────► diode SS54 (anode) ────►── + input của 3S BMS charger
                                              │
                                              ▼
                                          [3S BMS] ── + battery
   Spring - ────────────────────────────────── - input
                                              │
                                              ▼
                                              - battery
```

Diode SS54 quan trọng — chống current từ battery chảy ngược về dock khi robot không dock.

### 3.3 Dock voltage sense

Để robot biết "đã dock chưa", đo điện áp tại đầu spring contact:

```
Spring +12.6V ──┬── R1 100kΩ ──┬── ESP32-S3 GPIO2 (ADC1_CH1)
                │               │
                │              R2 33kΩ
                │               │
                └──────────────GND
```

V_adc = 12.6 × 33/(100+33) = 3.12V (khi docked, gần max ADC 3.3V)

Threshold "docked" = ADC > 2.5V (≈ 10V tại spring) → đã có tiếp xúc.

### 3.4 IR receiver

TSOP38238 receiver:

```
VCC ────── +3.3V (đảm bảo decouple 100nF)
GND ────── GND
OUT ────── GPIO5 (digital input, internal pull-up enabled)
```

Receiver output: **LOW** khi có 38kHz beacon, **HIGH** khi không có.

Lắp ở mặt trước robot, hướng song song trục camera (cùng hướng "phía trước").

---

## 4. State machine docking

```
                     [user request /dock/start]
                              │
                              ▼
                       ┌──────────┐
                       │   IDLE   │
                       └────┬─────┘
                            │
                            ▼
                       ┌──────────────┐
                       │   SEARCH     │  rotate CCW @ 30 dps
                       │ (rotate to   │  scan IR receiver
                       │  find beacon)│  timeout 30s
                       └────┬─────────┘
                            │ (IR detected)
                            ▼
                       ┌──────────────┐
                       │   APPROACH   │  drive forward @ 30 cm/s
                       │              │  obstacle gate enabled
                       └────┬─────────┘
                            │ (front ultrasonic < 30cm)
                            ▼
                       ┌──────────────┐
                       │   CONTACT    │  drive forward slow @ 10 cm/s
                       │              │  until dock_voltage > 10V
                       └────┬─────────┘
                            │ (charge voltage seen)
                            ▼
                       ┌──────────────┐
                       │   CHARGING   │  motor brake
                       │              │  monitor charge complete
                       └────┬─────────┘
                            │ (battery V > 12.4V + steady 5 min)
                            ▼
                       ┌──────────────┐
                       │  CHARGED     │  idle on dock
                       │  (docked)    │  await /dock/leave or low batt event
                       └──────────────┘

Any state → [CANCEL] → motor brake → IDLE
```

### 4.1 Timeout & guardrails

| Trạng thái | Timeout | Action khi timeout |
|------------|---------|---------------------|
| SEARCH | 30s | → FAULT (no beacon found) |
| APPROACH | 60s | → SEARCH (lost beacon mid-way) |
| CONTACT | 30s | → FAULT (couldn't make contact) |
| CHARGING | 4 giờ | OK (slow charge fine) |

### 4.2 Low battery auto-dock

Khi `battery_pct < 20%` + state == IDLE → tự động trigger /dock/start. Phase 12 sẽ enable hook này; Phase 9 chỉ manual trigger.

---

## 5. Pin mapping (Phase 9 additions)

| GPIO | Function | Direction | Note |
|------|----------|-----------|------|
| GPIO5 | IR receiver OUT | Input + pull-up | TSOP38238 — pin-mapping.md đã ghi |
| GPIO2 | DOCK_SENSE ADC | Input | ADC1_CH1, voltage divider 100k+33k |

**Lưu ý**: GPIO4 (IR_TX trong pin-mapping.md) **không dùng Phase 9** — phase này chỉ receive. Có thể dùng tương lai để robot phát tín hiệu confirmation hoặc indicator.

---

## 6. Schematic verification

Kiểm tra trong `camera_dock.kicad_sch`:

- [ ] TSOP38238 receiver:
  - VCC → +3.3V với 100nF decoupling
  - OUT → GPIO5
  - Internal pull-up enabled in firmware (không cần external)
- [ ] Spring contact connector 2-pin (đầu vào sạc)
- [ ] Diode SS54 trên đường spring(+) → 3S BMS input
- [ ] Voltage divider 100kΩ + 33kΩ → GPIO2 ADC
- [ ] Optional: TVS diode SMBJ15CA trên đường dock input (chống ESD)
- [ ] Test point: IR_RX, DOCK_SENSE, dock plate input

---

## 7. Charging timing

### 7.1 Charge profile

3S Li-ion BMS dùng **CC-CV** standard:
- **CC mode**: dòng 1A đến khi đạt 12.6V (~80% capacity)
- **CV mode**: giữ 12.6V, dòng giảm dần đến 100mA
- Total charge time từ 9.0V → 12.6V: ~3 giờ

### 7.2 "Charged" detection

Firmware không có dây cho phép đọc charge current trực tiếp. Detect "charged" qua heuristic:

```
charged = (VBAT > 12.4V) AND (VBAT_delta_over_5min < 0.05V)
```

Tức là: pin gần full + áp ổn định 5 phút (CC kết thúc, vào CV).

### 7.3 Safety

- Nếu VBAT > 12.7V trong 1 giây → tắt motor + alert (over-voltage, BMS có thể fail)
- Nếu charge current = 0 trong 5 phút nhưng VBAT < 12.4V → lost contact, retry CONTACT state

---

## 8. Common pitfalls

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| IR receiver luôn LOW | Sunlight noise | Đặt dock tránh nắng trực tiếp |
| IR receiver luôn HIGH | Beacon không modulated 38kHz | Verify 555 timer hoặc dùng IR module pre-mod |
| Robot tìm dock thấy "ảo" | Reflection từ tường gương | Đặt khu vực approach không có gương |
| Copper contact không truyền điện | Oxide trên đồng | Lau bằng cồn trước test; mạ thiếc nếu lâu dài |
| Robot đụng dock không charge | Spring stroke không đủ | Đảm bảo Y alignment ± 5mm |
| Charge voltage spike khi just dock | Bulk cap không có | Thêm 470µF sau diode SS54 robot side |
| BMS không nhận sạc | Diode SS54 đặt ngược | Verify cathode ở phía BMS |
| Robot lose beacon mid-approach | IR beam narrow | Dùng 2 IR LED 940nm song song spread 60° |

---

## 9. Next phase

Sau Phase 9 pass (robot tự dock + charge từ 80% pin yếu trong < 2 phút):
→ **Phase 10** — Firmware integration (tất cả task chạy đồng thời, behavior layer state machine).
