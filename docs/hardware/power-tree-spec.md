# Power Tree Specification — Elderly Companion Robot

> Authoritative power architecture cho robot. Tài liệu này định nghĩa kiến trúc nguồn 4-rail, lựa chọn linh kiện, công thức tính external components, và budget current — mọi quyết định KiCad schematic phải khớp với spec này.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 2 (HDSD)
> **Source of truth**: `power_budget.py` (chạy lại khi thay đổi load)

---

## 1. Kiến trúc tổng thể

```
                    +-------------------+
                    | 3S 18650 Battery  |
                    | 11.1V nom (9-12.6V)|
                    | 3000mAh × 3 cells |
                    +---------+---------+
                              │
                ┌─────────────┼─────────────┐
                │             │             │
                ▼             ▼             ▼
          [Fuse 5A]     [BUCK #1]      [BUCK #2]
                │       LM2596S-ADJ    LM2596S-ADJ
                │        →5.0V/3A       →4.0V/2A
                │             │             │
            ┌───┴───┐         ▼             ▼
            ▼       ▼      ┌──────┐    ┌────────┐
         L298N   (raw 12V  │  5V  │    │  4V    │
         motor   for fans)│ rail  │    │ rail   │
                          └──┬───┘    └────┬───┘
                             │             │
                  ┌──────────┼────────┐    │
                  │          │        │    │
                  ▼          ▼        ▼    ▼
              [LDO]      ESP32-S3   Servo SIM800L
              AP2112K    Camera   HC-SR04 (only)
              →3.3V/600mA L298N   MAX98357A
                  │       logic
                  ▼
              ┌───────┐
              │ 3V3   │
              │ rail  │
              └───┬───┘
                  │
       ┌──────────┼────────────┐
       ▼          ▼            ▼
    MPU6050   SSD1306      INMP441
              OLED          mic

                         ┌─────────────┐
                         │ Charge in   │
                         │ from dock   │
                         │ 12.6V × 1A  │
                         └──────┬──────┘
                                ▼
                         [3S BMS + balance]
                                │
                                ▼
                         (sạc lại battery)
```

### 4 rail riêng biệt

| Rail | Nguồn | Max current | Mục đích |
|------|-------|-------------|----------|
| **12V** | Battery direct | 8A (giới hạn fuse) | Motor power (L298N VS) |
| **5V** | LM2596S-ADJ #1 | 3A | Camera, servo, sensor, audio amp |
| **3V3** | AP2112K-3.3 từ 5V | 600mA | Logic 3.3V (IMU, OLED, mic, pull-ups) |
| **4V** | LM2596S-ADJ #2 | 2A | SIM800L only (cần dedicated) |

### Tại sao 4 rail riêng?

**5V và 4V tách hẳn nhau từ 12V** (không cascade) vì:
- SIM800L burst 2A trong 577µs sẽ kéo sụt áp rail 5V → ESP32 reset
- Tách buck riêng đảm bảo glitch không lan
- Cost: thêm 1 IC LM2596S (~10k VND) nhưng độ tin cậy cao hơn nhiều

**3V3 cascade từ 5V** (không từ 12V) vì:
- LDO 12V→3.3V drop 8.7V × 100mA = 870mW heat → quá nóng
- Cascade từ 5V drop chỉ 1.7V × 100mA = 170mW → mát
- Trade-off: phải đảm bảo rail 5V luôn còn > 3.5V để LDO hoạt động (dropout 0.25V)

---

## 2. Current budget — đã tính bằng `power_budget.py`

### 2.1 Summary per rail

| Rail | Reg max | I_typ | I_peak | Headroom (typ) | Headroom (peak) |
|------|---------|-------|--------|----------------|------------------|
| 12V | 8000 mA | 800 mA | 4000 mA | +7200 mA OK | +4000 mA OK |
| 5V  | 3000 mA | 847 mA | 2522 mA | +2153 mA OK | +478 mA OK |
| 3V3 | 600 mA  | 87 mA  | 150 mA  | +513 mA OK   | +450 mA OK |
| 4V  | 2000 mA | 350 mA | 2000 mA | +1650 mA OK  | +0 mA **WARN** |

### 2.2 Phân tích headroom

- **5V rail**: 478mA peak headroom — đủ an toàn. Hai servo stall đồng thời + camera + audio loud cùng lúc là kịch bản worst case.
- **3V3 rail**: 450mA headroom — quá thoải mái. Có thể giảm xuống AP7361 (300mA) nếu cần tiết kiệm cost, nhưng AP2112K rẻ và sẵn có.
- **4V rail**: **+0mA peak** — sát limit. OK vì SIM800L burst chỉ 577µs trong 4.6ms (12% duty). LM2596S 2A có thermal margin dư.
- **12V rail**: motor dual stall 4A < 8A fuse — đủ headroom 2× cho an toàn.

### 2.3 Battery runtime estimate

- Total power typical load: **16.3 W**
- Battery current: 1.47 A @ 11.1V
- Runtime đến 80% DoD: **~1.6 giờ** (98 phút) liên tục
- Patrol/idle mix: 3-5 giờ thực tế (vì idle ở dock đa số thời gian)

---

## 3. BUCK #1 — LM2596S-ADJ → 5.0V / 3A

### 3.1 Vout calculation

Internal Vfb = 1.23V. Feedback divider:

```
Vout = Vfb × (1 + R2/R1)
5.00 = 1.23 × (1 + R2/R1)
R2/R1 = 3.07
```

**Chọn**: R1 = 1.0kΩ (1%), R2 = 3.09kΩ (1% E96) → Vout = 5.03V ✓

Hoặc dùng E24: R1 = 1.0kΩ, R2 = 3.0kΩ → Vout = 4.92V (acceptable).

### 3.2 External components

| Component | Value | Spec | LCSC | Note |
|-----------|-------|------|------|------|
| U1 | LM2596S-ADJ | 3A buck, TO-263-5 | C46376 | Hoặc package SOP-8 |
| L1 | 33µH | I_sat ≥ 4A, DCR < 50mΩ | C18198 | Shielded SMD |
| D1 | SS34 | Schottky 3A 40V | C9082 | Catch diode |
| Cin1 | 100µF | 25V electrolytic, low ESR | C16133 | Input bulk |
| Cin2 | 1µF | 50V X7R | C28233 | Input HF bypass |
| Cout1 | 220µF | 16V electrolytic, low ESR | C2680 | Output bulk |
| Cout2 | 10µF | 25V X7R | C19702 | Output mid-freq |
| Cout3 | 100nF | 50V X7R | C14663 | Output HF |
| R1 | 1.0kΩ | 1% 0603 | C25804 | Feedback bottom |
| R2 | 3.09kΩ | 1% 0603 | C26015 | Feedback top |

### 3.3 PCB notes

- Đặt **Cin** sát chân VIN của LM2596S (< 5mm trace)
- Đặt **L1 → D1** loop ngắn nhất có thể (high di/dt loop)
- Diode anode về cùng GND với Cin/Cout (single point ground)
- Copper pour rộng cho dải VOUT (carry 3A) — xem `pcb-layout-power.md`

---

## 4. BUCK #2 — LM2596S-ADJ → 4.0V / 2A (SIM800L)

### 4.1 Vout calculation

```
Vout = 1.23 × (1 + R2/R1)
4.00 = 1.23 × (1 + R2/R1)
R2/R1 = 2.25
```

**Chọn**: R1 = 1.0kΩ, R2 = 2.21kΩ (E96 1%) → Vout = 3.95V (trong dải SIM800L 3.4-4.4V) ✓

Hoặc E24: R1 = 1.0kΩ, R2 = 2.2kΩ → Vout = 3.94V (acceptable).

### 4.2 External components

Giống Buck #1 nhưng:
- **Cout1 = 470µF low-ESR** (không 220µF) — burst 2A của SIM800L cần bulk cap lớn để giữ áp trong 577µs
- **Thêm Cout4 = 100µF tantalum** sát chân VCC của SIM800L
- L1 vẫn 33µH (max 2A, OK)
- D1 vẫn SS34

### 4.3 SIM800L-specific note

SIM800L kéo 2A trong burst TX rất hẹp. Cap output phải đủ lớn:

```
ΔV = I × Δt / C
ΔV < 0.2V (cho phép sụt từ 4V xuống 3.8V vẫn trong spec)
I = 2A
Δt = 577µs

C > I × Δt / ΔV = 2 × 577e-6 / 0.2 = 5770µF ?!
```

Quá lớn. Nhưng thực tế buck phản ứng nhanh (LM2596 switching 150kHz), nên ΔV không tăng tuyến tính. Thực tế **470µF + 100µF tantalum** sát chip là đủ.

**Best practice**: dùng module SIM800L có sẵn bulk cap 470µF on-board (đa số module Shopee có).

---

## 5. LDO — AP2112K-3.3 → 3.3V / 600mA

### 5.1 Topology

LDO fixed 3.3V output, cascade từ rail 5V. Dropout 0.25V (rất low) @ 600mA → 5V - 0.25V = 4.75V > 3.3V ✓

### 5.2 External components

| Component | Value | Spec | LCSC | Note |
|-----------|-------|------|------|------|
| U2 | AP2112K-3.3TRG1 | 600mA LDO, SOT-25 | C51118 | Fixed output |
| Cin | 1µF | 10V X7R | C15849 | Input |
| Cout | 1µF | 10V X7R | C15849 | Output (datasheet recommends ≥1µF) |
| Cbypass | 100nF | 10V X7R | C14663 | HF bypass, parallel với Cout |

### 5.3 PCB notes

- AP2112K SOT-25 không có thermal pad — dropout 0.25V × 600mA = 150mW max heat → không cần copper pour lớn
- Đặt Cin và Cout sát chân U2 (< 3mm)
- Trace 3V3 ra ngoài đảm bảo width ≥ 0.5mm (cho 600mA)

---

## 6. Charging — 3S Li-ion từ dock

### 6.1 Topology

```
Dock 12.6V/1A → Copper contact → Diode (chống cắm ngược) → 3S BMS → Battery pack
                                       (TVS protect)
```

### 6.2 BMS chọn

**3S Li-ion BMS + balance + protection module** (Shopee, ~30-50k VND):
- Charge voltage: 12.6V (3 × 4.2V)
- Charge current: 1A (matched với adapter)
- Discharge max: 8A (matched với fuse robot side)
- Balance: dùng resistor balance đảm bảo 3 cell đều áp ± 50mV
- Protection: over-charge, over-discharge, over-current, short

### 6.3 Charge dock circuit

| Component | Value | Note |
|-----------|-------|------|
| AC-DC adapter | 12.6V / 1A | Cắm tường |
| Diode dock-side | SS54 (5A Schottky) | Chống cắm ngược |
| TVS diode | SMBJ15CA bidirectional | Protect ESD khi robot dock vào |
| Copper plates | 50×30mm đồng đỏ, mạ thiếc | Tiếp xúc với spring contact robot |

### 6.4 Robot dock side

| Component | Value | Note |
|-----------|-------|------|
| Spring contact | nén 3mm, gold-plated | Chống oxide |
| Schottky diode | SS54 | Cùng SS54 hoặc tương đương — chống dòng ngược battery → dock khi robot không dock |
| Fuse | 5A polyfuse PTC | Tự reset, bảo vệ short |
| Charge indicator LED | 2x (đỏ = sạc, xanh = đầy) | Driver từ BMS |

---

## 7. Fuse & protection

| Vị trí | Loại | Rating | Mục đích |
|--------|------|--------|----------|
| Battery → 12V rail | Polyfuse PTC | 5A trip | Chống short toàn hệ |
| L298N VS | Polyfuse PTC | 3A trip | Chống motor short |
| Buck #1 VIN | Glass fuse 3A | 3A blow | Chống IC chết short ngắn ngày |
| Buck #2 VIN | Glass fuse 2A | 2A blow | Tương tự |
| Charge in | Diode SS54 + polyfuse 1.5A | — | Chống cắm ngược |

**TVS diode** ở các vị trí I/O ra ngoài board (SIM800L antenna, dock contact, USB).

---

## 8. Battery monitoring

Đo điện áp battery để robot biết khi nào cần dock.

### 8.1 Voltage divider

```
12V battery ──R1(100kΩ)──+──R2(33kΩ)──GND
                          │
                          └── ESP32-S3 ADC1_CH0 (GPIO1)
```

Vadc = Vbat × 33 / (100 + 33) = Vbat × 0.248

- Vbat = 12.6V (full) → Vadc = 3.12V (gần max ADC 3.3V)
- Vbat = 9.0V (cutoff) → Vadc = 2.23V

**Quan trọng**: dùng ADC1 (GPIO1-10), không dùng ADC2 (mâu thuẫn với WiFi).

### 8.2 Tính %battery

```c
// firmware/drivers/battery.c
float battery_voltage(void) {
    int raw = adc1_get_raw(ADC1_CHANNEL_0);
    float v_adc = (raw / 4095.0f) * 3.3f;
    return v_adc * (133.0f / 33.0f);   // Reverse divider
}

uint8_t battery_pct(void) {
    float v = battery_voltage();
    if (v >= 12.4f) return 100;
    if (v <= 9.0f)  return 0;
    // Linear approx (Li-ion thực ra cong, dùng table nếu cần chính xác)
    return (uint8_t)((v - 9.0f) / (12.4f - 9.0f) * 100);
}
```

---

## 9. BOM tổng (Phase 2)

| Item | LCSC | Qty | Cost ước (VND) | Note |
|------|------|-----|----------------|------|
| LM2596S-ADJ TO-263 | C46376 | 2 | 8,000 × 2 | Buck #1 + #2 |
| AP2112K-3.3 SOT-25 | C51118 | 1 | 2,500 | LDO 3V3 |
| Inductor 33µH 4A | C18198 | 2 | 5,000 × 2 | Buck inductor |
| SS34 Schottky | C9082 | 2 | 1,000 × 2 | Catch diode |
| SS54 5A Schottky | C9080 | 2 | 1,500 × 2 | Dock + reverse protect |
| 470µF aluminium | C107198 | 1 | 3,000 | SIM800L bulk |
| 220µF aluminium | C2680 | 1 | 2,500 | Buck #1 out |
| 100µF aluminium | C16133 | 2 | 2,000 × 2 | Buck inputs |
| 100µF tantalum | C32479 | 1 | 5,000 | SIM800L sát chip |
| 10µF X7R 25V | C19702 | 4 | 500 × 4 | Mid bypass |
| 1µF X7R | C15849 | 4 | 200 × 4 | LDO + HF bypass |
| 100nF X7R | C14663 | 10 | 100 × 10 | Decoupling chung |
| Resistor 1% 0603 (feedback) | various | 4 | 100 × 4 | R1, R2 cho 2 buck |
| Polyfuse 5A | C70748 | 1 | 3,000 | Main fuse |
| Polyfuse 3A | C70747 | 1 | 2,500 | L298N protect |
| TVS SMBJ15CA | C153247 | 2 | 1,500 × 2 | Dock contact |
| 3S BMS 12.6V/8A | (Shopee) | 1 | 30,000 | Charge + balance |
| AC-DC adapter 12.6V/1A | (Shopee) | 1 | 50,000 | Dock power |
| Spring contact gold | (Shopee) | 2 | 5,000 × 2 | Robot side |
| Copper plate dock | DIY | 2 | 5,000 | Đồng đỏ 50×30 |
| | | **TOTAL** | **~150,000 VND** | |

---

## 10. Mapping vào KiCad schematic

Các sheet đã có trong `hardware/kicad/elderly-companion-robot/`:

- **Power chính**: tạo sheet mới `power.kicad_sch` (chưa tồn tại — cần thêm)
- Hoặc đưa power vào sheet `mcu_core.kicad_sch` (nếu giữ flat hierarchy)
- Net naming chuẩn:
  - `+12V` — battery direct
  - `+5V` — sau buck #1
  - `+3V3` — sau LDO
  - `+4V_SIM` — sau buck #2 (riêng SIM800L)
  - `GND` — ground duy nhất
  - `VBAT_SENSE` — về ADC1_CH0

**Lưu ý cho KiCad ERC**:
- Mỗi rail cần `PWR_FLAG` cấp 1 lần
- 4 rail = 4 power flag cần đặt
- Mỗi rail có ít nhất 1 power input pin (PWR_INPUT) — thường gắn ở connector battery

---

## 11. Acceptance criteria Phase 2

Trước khi chuyển sang Phase 3, Phase 2 phải pass:

- [ ] Tất cả 4 rail đo đúng điện áp ± 5% (no load):
  - 12V = 11.1 → 12.6V (battery dependent)
  - 5V = 4.85 - 5.15V
  - 3V3 = 3.20 - 3.40V
  - 4V_SIM = 3.85 - 4.15V
- [ ] Ripple oscilloscope AC coupling:
  - 5V: < 50mV pp
  - 3V3: < 20mV pp
  - 4V_SIM: < 100mV pp (acceptable vì cấp SIM800L)
- [ ] Load test 1 giờ: tất cả rail giữ ổn định khi load 50% rating
- [ ] Thermal: không có IC nào > 60°C sau 1 giờ liên tục
- [ ] Charge: cắm dock → BMS sạc, dòng ~1A, dừng ở 12.6V ± 0.05V
- [ ] Battery monitor: ADC đọc đúng ± 0.1V so với đo bằng đồng hồ

→ Chi tiết quy trình test: xem `power-bringup.md`
