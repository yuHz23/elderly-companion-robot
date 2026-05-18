# PCB Layout — Power Section

> Quy chuẩn layout PCB cho phần nguồn. Tuân theo những nguyên tắc này khi route trong KiCad PCB editor.
>
> **Áp dụng**: stack-up 2 lớp (FR-4 1.6mm, 1oz copper) — phù hợp với JLCPCB economy.
>
> **Version**: 1.0 — 2026-05-18

---

## 1. Stack-up

```
┌──────────────────────────────────┐
│ Top silk                          │
├──────────────────────────────────┤
│ Top copper 1oz (35µm)             │ ← signal + power, components
├──────────────────────────────────┤
│ FR-4 dielectric 1.6mm             │
├──────────────────────────────────┤
│ Bottom copper 1oz (35µm)          │ ← GND pour + minimum signal
├──────────────────────────────────┤
│ Bottom silk                       │
└──────────────────────────────────┘
```

**Top layer**: components + signal routing + power rails.
**Bottom layer**: **GND POUR liên tục** + một vài signal traces nếu không thể route ở top.

---

## 2. Ground strategy

### 2.1 Single ground plane (recommended)

Toàn bộ lớp BOTTOM phủ GND pour. Đây là **AC ground reference** cho mọi tín hiệu.

**Lý do**:
- Loop area cho return current = nhỏ nhất (return current chảy ngay dưới signal trace)
- EMI giảm đáng kể
- Decoupling cap hiệu quả hơn

### 2.2 Khi nào cần split ground?

**Không split** với robot này. Lý do:
- Mixed signal nhưng dòng/điện áp không khác biệt nhiều
- Split ground tạo discontinuity → return current loop lớn → EMI tệ hơn
- Chỉ split khi có analog 0.1mV-level (ADC chính xác) hoặc RF — không phải case của ta

### 2.3 Tách "noisy" vs "quiet" qua component placement

Thay vì split GND, **đặt component cách xa nhau**:

| Noisy (gây nhiễu) | Quiet (nhạy nhiễu) | Min distance |
|-------------------|--------------------|--------------:|
| Buck LM2596S | MPU6050 IMU | 30mm |
| Buck LM2596S | INMP441 mic | 40mm |
| L298N + motor | MPU6050 IMU | 50mm |
| MAX98357A class-D | INMP441 mic | 30mm |
| Antenna SIM800L | Tất cả | (đặt antenna ngoài board) |
| Buck switch node (SW pin) | Anything sensitive | 10mm |

---

## 3. Trace width calculation

### 3.1 Công thức IPC-2152 simplified

Trace **internal** (lớp inner — không áp dụng cho ta 2 layer):
- 35µm copper, 20°C rise: width (mm) ≈ Current(A) × 0.4

Trace **external** (top/bottom):
- 35µm copper, 20°C rise: width (mm) ≈ Current(A) × 0.25

### 3.2 Bảng tra cứu

| Current (A) | Min width (mm, external) | Recommended |
|-------------|--------------------------|-------------|
| 0.1 (signal) | 0.15 | 0.25 |
| 0.5 (logic supply) | 0.20 | 0.4 |
| 1.0 (small load) | 0.25 | 0.5 |
| 2.0 (servo/audio) | 0.40 | 0.8 |
| 3.0 (Buck output, 5V rail) | 0.60 | 1.2 |
| 5.0 (Battery main) | 1.00 | 2.0 |
| 8.0 (Fuse path) | 1.60 | 3.0 |

### 3.3 Áp dụng cho từng rail

| Rail | Current max | Trace width | Note |
|------|-------------|-------------|------|
| 12V main (battery → bucks + L298N) | 5A | **2.0mm** | Hoặc copper pour |
| 5V output buck #1 | 3A | **1.2mm** | |
| 4V output buck #2 | 2A | **0.8mm** | |
| 3V3 output LDO | 0.6A | **0.5mm** | |
| Signal (I2C, I2S, GPIO) | <0.1A | **0.2mm** | |

**Tốt nhất**: dùng **copper pour** cho 12V và 5V (rộng hơn trace), đảm bảo current spread.

---

## 4. Buck converter layout — Critical loop

### 4.1 Hot loop (input loop)

Loop chứa các xung dòng nhanh nhất (di/dt cao):
```
Cin → VIN pin → SW pin → L1 → Cout → GND → Cin (qua plane)
```

Phải:
- **Ngắn nhất có thể**
- Không xuyên via (via inductance phá performance)
- Trace rộng (giảm trace inductance)

### 4.2 Layout example (LM2596S TO-263)

```
        ┌─────────────────────────┐
        │  Cin           Cout     │ ← Cap cách chân < 3mm
        │  100µF         220µF    │
        │ │ │ │         │ │ │ │   │
        ├─┴─┴─┴─────────┴─┴─┴─┴───┤
        │ ┌───────────────────┐   │
        │ │      LM2596S      │   │
        │ │ 1 2 3 4 5         │   │
        │ └─┬─┬─┬─┬─┬─────────┘   │
        │   │ │ │ │ │             │
        │   │ │ │ │ └─ON/OFF      │
        │   │ │ │ └─── FB ───── R-divider
        │   │ │ └───── GND        │
        │   │ └─────── SW ─── L1 ─┐
        │   └───────── VIN        │
        │                         │
        │              ┌──────┐   │
        │      L1───►  │ ind  │───┤
        │              └──────┘   │
        │              ↓ Vout     │
        │              ▼          │
        │         D1 cathode──────┤
        │         D1 anode → GND  │
        └─────────────────────────┘
        ▼
       BOTTOM: full GND copper pour
       (cap return path)
```

### 4.3 Sai lầm cần tránh

- ❌ Cin đặt xa VIN, kết nối qua trace dài 10mm → loop area lớn
- ❌ D1 anode về GND qua via, không phải mass ground sát Cin → loop "rách"
- ❌ Trace SW (switching node) đi qua khu vực sensitive (mic, IMU)
- ✓ Cin và Cout về cùng 1 điểm GND (single-point ground tại LM2596S)
- ✓ D1 anode về GND ở **đúng** vị trí gần Cin's GND pin

---

## 5. SW node — "noise emitter"

Chân SW của buck chuyển đổi 0V ↔ 12V ở 150kHz → tạo dV/dt rất lớn (~24 V/µs). Đây là nguồn nhiễu radiated chính.

### Quy tắc

- Trace SW: **ngắn nhất** có thể, **không đi qua dưới** sensitive IC
- Không phủ copper SW vào polygon lớn (sẽ là anten phát)
- Đặt L1 sát chân SW (< 5mm)

---

## 6. Decoupling cap layout

### 6.1 Quy tắc cơ bản

Cap 100nF luôn:
1. Cùng layer với IC (top thường)
2. < 3mm từ chân VCC
3. 2 via (mỗi pad) xuống GND plane

```
       IC pin VCC
            ▼
        ┌───────┐
        │  IC   │
        │       │
        └──┬────┘
           │ < 3mm
        ┌──┴──┐
        │ 100nF│
        └──┬──┘
           │
        ┌──┴──┐
       (GND via to bottom GND plane)
```

### 6.2 Bulk cap (470µF SIM800L, ESP32)

Bulk cap to → 6-8 via mỗi pad (chia dòng):
- Reducing via inductance
- Spreading current evenly

---

## 7. Via stitching

Tạo "wall" via nối GND top và bottom → giảm EMI, tăng integrity.

### 7.1 Quy tắc

- Via GND mỗi 5mm dọc theo edge board
- Via GND quanh switching IC mỗi 3mm
- Via GND dưới antenna SIM800L: pattern dense (mỗi 2mm)
- Tổng via GND: nhiều > tốt (không đếm, cứ rải)

### 7.2 Pattern khuyến nghị

```
. = via GND
─ = signal/power trace

. . . . . . . . . . . . . . . . .
.                               .
.   ┌────┐                      .
.   │ IC │ . . . . . . .        .
.   └────┘                      .
.                               .
.            ┌────┐             .
.            │BUCK│ . . . . .   .
.            └────┘             .
.                               .
. . . . . . . . . . . . . . . . .
```

---

## 8. Thermal considerations

### 8.1 LM2596S TO-263 thermal pad

- LM2596S khi tải 3A: P_loss ≈ (1 - 0.85) × 5V × 3A = 2.25W
- TO-263 với thermal pad → cần copper pour bottom **≥ 25 × 25mm** mỗi IC
- 5-10 via thermal nối thermal pad lên GND pour

```
TOP:                       BOTTOM:
┌──────────────────┐       ┌──────────────────┐
│  LM2596S         │       │                   │
│   ▒▒▒▒▒          │       │   ░░░░░░░░░░     │
│   ▒thermal       │ vias  │   ░░ copper ░░    │
│   ▒pad ▒  ───────┼──────▶│   ░░ pour   ░░    │
│   ▒▒▒▒▒          │       │   ░░ 30×30mm░░    │
└──────────────────┘       └──────────────────┘
                          (gắn vào GND plane)
```

### 8.2 AP2112K SOT-25 không cần copper lớn

P_loss = (5 - 3.3) × 0.1 = 0.17W → 1cm² copper là đủ.

### 8.3 L298N

L298N TO-220 → cần **tản nhiệt nhôm gắn ngoài** nếu motor 2A continuous. Bolt-on heatsink, không dùng SMD package cho ứng dụng này.

---

## 9. Layer-by-layer routing strategy

### Top layer:
1. Components placement (theo logic block)
2. Power traces (12V, 5V, 4V, 3.3V) — copper pour ưu tiên
3. Signal traces ngắn (I2C, I2S)
4. Khu vực còn lại: GND pour (connect via to bottom)

### Bottom layer:
1. **GND POUR** chiếm 70-80% diện tích
2. Signal traces chỉ khi không thể route ở top (giao nhau)
3. Mỗi signal cross qua bottom phải có via GND đối xứng < 5mm để return path liền mạch

### Order khi route:
1. Place tất cả component trước (theo schematic block)
2. Route power **trước** signal (power = priority cao)
3. Route critical signal (clock, reset, USB)
4. Route I2C/I2S
5. Route GPIO general purpose
6. Fill copper pour
7. Add via stitching
8. DRC check

---

## 10. Kiểm tra cuối — Checklist trước Gerber export

- [ ] DRC pass 0 errors
- [ ] Tất cả cap decoupling đặt < 3mm từ chân chip
- [ ] Hot loop buck (Cin → SW → L1 → Cout → GND) không xuyên via
- [ ] Trace 12V main ≥ 2mm width (hoặc copper pour)
- [ ] Trace 5V buck output ≥ 1.2mm width
- [ ] SW node không đi qua khu vực sensitive
- [ ] MPU6050 cách buck/motor ≥ 30mm
- [ ] INMP441 cách MAX98357A ≥ 30mm
- [ ] Thermal pad LM2596S có copper bottom ≥ 25×25mm + 5+ via
- [ ] Via stitching GND quanh switching IC
- [ ] Power flag đặt cho mỗi rail (4 rails)
- [ ] Test point cho mỗi rail (cho phép đo bằng oscilloscope)

---

## 11. Test point — luôn cần

Đặt test point ngoài cho:

- VBAT (12V battery)
- VBUCK1_OUT (5V)
- VBUCK2_OUT (4V SIM)
- VLDO (3.3V)
- GND (2 cái, đối xứng)
- VBAT_SENSE (ADC)

Test point dạng pad 1.5mm hole 0.8mm — đủ kẹp probe oscilloscope.

Vị trí test point đặt **gần edge board** → dễ probe khi power-on.

---

## 12. Documentation trong KiCad

Thêm `text-on-silk` để hiện:
- Rail name (5V, 3.3V, 4V_SIM, 12V) trên silk gần connector
- Direction arrow cho input/output
- Version number của PCB
- "FUSE 5A" gần fuse position

Silk giúp debug sau khi assembly — không phải mỹ thuật.
