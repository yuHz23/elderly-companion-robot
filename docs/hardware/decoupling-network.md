# Decoupling Network — Elderly Companion Robot

> Bảng tra cứu cap decoupling cho mọi IC trên board. Mỗi IC có yêu cầu khác nhau dựa trên switching frequency, dòng peak, nhạy cảm noise.
>
> **Nguyên tắc cốt lõi**: mỗi chân VCC của mỗi IC phải có **ít nhất 1 cap 100nF X7R 0603** đặt **< 3mm** từ chân chip, hàn trên cùng một mặt PCB với chip.
>
> **Version**: 1.0 — 2026-05-18

---

## 1. Tại sao cần decoupling cap?

Mỗi IC khi switch transistor bên trong sẽ kéo xung dòng vài ns. Trace từ IC về buck/LDO có inductance ~1nH/mm — quá xa → xung dòng tạo voltage spike trên rail.

Decoupling cap = bể chứa năng lượng cục bộ. Khi IC cần dòng nhanh, lấy từ cap (gần) thay vì rail (xa).

**Hai vai trò khác nhau**:
- **Bulk cap** (10-470µF): cung cấp năng lượng cho switching tải lớn, low frequency
- **Bypass cap** (100nF ceramic): cung cấp dòng HF (MHz range), không cho noise quay về rail

---

## 2. Bảng decoupling per-IC

### 2.1 ESP32-S3-CAM module

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| Sát chân 5V_IN | 470µF | Aluminium low-ESR | Bulk cho WiFi TX burst |
| Sát chân 5V_IN | 100nF | X7R 0603 | HF bypass |
| Sát chân 3V3 (internal) | (module đã có on-board) | — | Đa số module có cap on-board, không cần thêm |

ESP32-S3 trong burst WiFi kéo ~500mA trong vài µs. Không có bulk cap → áp sụt → reboot. **Cap 470µF không thể thiếu**.

### 2.2 LM2596S-ADJ buck (cho cả Buck #1 và #2)

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VIN — GND | 100µF | Aluminium 25V low-ESR | Input bulk |
| VIN — GND | 1µF | X7R 0603 50V | Input HF |
| VOUT — GND | 220µF (#1) / 470µF (#2) | Aluminium 16V low-ESR | Output bulk |
| VOUT — GND | 10µF | X7R 0603 25V | Mid-freq |
| VOUT — GND | 100nF | X7R 0603 50V | HF bypass |

**Loop quan trọng**: Cin → SW → L1 → D1 → Cout phải kín, ngắn, không xuyên via.

### 2.3 AP2112K LDO

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VIN | 1µF | X7R 0603 10V | Datasheet require min 1µF |
| VOUT | 1µF | X7R 0603 10V | Datasheet require min 1µF |
| VOUT | 100nF | X7R 0603 10V | HF bypass |

LDO không có inductor → không cần bulk cap lớn. Chỉ cần 1µF mỗi side.

### 2.4 SIM800L module

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VCC — GND | 100µF | Tantalum 6.3V | Burst 2A TX, ESR thấp critical |
| VCC — GND | 100nF | X7R 0603 | HF |
| Module pre-installed | 470µF | (đa số module có sẵn) | Bulk |

SIM800L module Shopee phổ biến đã có cap bulk on-board. Vẫn nên thêm 100µF tantalum sát chip để dự phòng.

### 2.5 L298N H-bridge driver

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VS (motor power, 12V) | 100µF | Aluminium 25V | Motor inrush |
| VS — GND | 100nF | X7R 0603 50V | HF noise từ motor brush |
| VSS (logic, 5V) | 10µF | X7R 0603 10V | Logic stability |
| VSS — GND | 100nF | X7R 0603 10V | HF |
| Mỗi motor output | snubber RC | 100Ω + 100nF | Flyback protection |

Motor brush tạo nhiễu HF mạnh. Snubber RC trên mỗi output (4 chỗ) **bắt buộc** nếu robot dùng motor DC chổi than.

### 2.6 MPU6050 IMU

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VDD (3.3V) | 100nF | X7R 0603 | Bypass standard |
| VLOGIC | 100nF | X7R 0603 | (nếu chip không tie VLOGIC=VDD) |
| REGOUT (internal LDO output) | 100nF | X7R 0603 | Datasheet require |

MPU6050 nhạy với noise — đặc biệt ảnh hưởng accuracy của accelerometer. **Đặt xa motor**.

### 2.7 SSD1306 OLED

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VDD | 100nF | X7R 0603 | Logic |
| VCC (panel, 7-12V internal boost) | 1µF | X7R 0603 25V | Panel power |

Module Shopee đã có sẵn boost converter on-board. Không cần lo VCC panel.

### 2.8 INMP441 mic

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VDD (3.3V) | 100nF | X7R 0603 | Sát chân, **critical**: noise vào VDD trực tiếp vào audio |
| VDD | 10µF | X7R 0603 | Bulk |

MEMS mic cực nhạy với power supply noise. **Trace VDD riêng từ LDO output, không share với MPU6050/OLED**.

### 2.9 MAX98357A I2S amp

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VDD (5V) | 100µF | Aluminium | Bulk cho class-D output |
| VDD | 100nF | X7R 0603 | HF |
| VDD | 10µF | X7R 0603 | Mid-freq |

Class-D switching tạo nhiễu — đặt amp xa MEMS mic ít nhất 30mm.

### 2.10 HC-SR04 ultrasonic

| Position | Value | Type | Mục đích |
|----------|-------|------|----------|
| VCC (5V) | 100nF | X7R 0603 | Bypass — module đã có on-board nhưng thêm nữa ổn |
| Echo pin | (voltage divider 1KΩ+2KΩ) | — | Cấp xuống 3.3V cho ESP32 |

---

## 3. Quy chuẩn placement trên PCB

### 3.1 Khoảng cách

| Cap type | Max distance to pin |
|----------|---------------------|
| 100nF HF bypass | **< 3mm** từ chân VCC |
| 1µF mid bypass | < 10mm |
| 10-100µF bulk | < 20mm |
| 470µF main bulk | < 50mm (lớn nhưng được vì frequency thấp) |

### 3.2 Via strategy

- Cap 100nF: 2 via lên VCC plane + 2 via xuống GND plane = 4 via tổng. Đường via phải đối xứng.
- Cap bulk: 4+ via mỗi chân để giảm via inductance.
- **Không bao giờ** dùng thermal relief cho via decoupling — full copper connection.

### 3.3 Sai lầm hay gặp

- ❌ Cap 100nF cách chân 10mm → vô tác dụng ở HF
- ❌ Đặt cap xa rồi nối bằng trace dài → trace inductance phá HF performance
- ❌ Cap cùng giá trị xếp song song nhiều = không cải thiện ESR (vẫn 1 cap effective)
- ✓ Cap nhiều giá trị khác nhau (100nF + 10µF + 100µF) song song → cover dải tần rộng

---

## 4. Bulk cap calculation cho high-current pulse loads

Công thức:

```
ΔV = (I_peak × Δt) / C
```

Với:
- ΔV: sụt áp cho phép (V)
- I_peak: dòng burst peak (A)
- Δt: thời gian burst (s)
- C: cap cần (F)

### Ví dụ tính cho SIM800L

- I_peak = 2A
- Δt = 577µs (1 burst TX)
- ΔV cho phép = 0.2V (4V → 3.8V vẫn trong spec)

```
C > (2 × 577e-6) / 0.2 = 5770µF
```

**5770µF là quá lớn**. Vì sao thực tế chỉ cần 470µF + 100µF?

Vì:
1. Buck LM2596S switching 150kHz → mỗi 6.7µs có pulse refill. Trong 577µs có ~86 lần refill.
2. Loop control buck phản ứng trong vài chu kỳ → ΔV không tăng tuyến tính
3. SIM800L module thường có cap on-board

Thực tế: **470µF + 100µF tantalum + buck phản ứng nhanh** → đủ.

Nếu thấy reboot khi SIM800L TX → tăng cap output buck #2 lên 1000µF.

---

## 5. Checklist khi review schematic

Trước khi gửi PCB đặt:

- [ ] Mỗi IC có ≥ 1 cap 100nF sát chân VCC trên schematic
- [ ] Buck inputs: 100µF + 1µF mỗi cái
- [ ] Buck outputs: 220-470µF + 10µF + 100nF mỗi cái
- [ ] LDO in/out: 1µF mỗi side + 100nF
- [ ] ESP32-S3-CAM 5V: 470µF + 100nF
- [ ] SIM800L 4V: 100µF tantalum + 100nF (extra cho on-board)
- [ ] Motor outputs: snubber RC mỗi pin (4 chỗ)
- [ ] Tất cả cap có chỉ rõ voltage rating ≥ 2× rail voltage
- [ ] Tất cả cap ceramic là X7R hoặc X5R (không Y5V/Z5U)
