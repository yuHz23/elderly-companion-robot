# Power Tree Bring-up Procedure

> Quy trình test PCB nguồn lần đầu sau khi assemble. Tuân thủ thứ tự — **không skip bước** vì 1 lỗi nguồn có thể chết MCU $20.
>
> **Áp dụng**: sau khi PCB hàn xong, chưa cắm MCU/SIM800L/sensor.
>
> **Dụng cụ cần**:
> - Bench DC power supply có current limit
> - Đồng hồ vạn năng DMM
> - Oscilloscope (đo ripple, optional nhưng nên có)
> - Bench DC load hoặc resistor power (cho load test)
> - Probe kẹp cá sấu

---

## Pre-flight checklist (TRƯỚC KHI CẮM ĐIỆN)

### Visual inspection (10 phút)

- [ ] Quan sát kỹ tất cả mối hàn — không có short giữa 2 chân kề nhau
- [ ] Tất cả IC đặt đúng chiều (pin 1 dot khớp với silk)
- [ ] Polarized cap (tantalum, electrolytic) đúng chiều (vạch trắng = âm)
- [ ] Diode đúng chiều (vạch trắng/cathode khớp với silk)
- [ ] Không có "tin ball" (giọt thiếc lăn lóc trên board)
- [ ] Không có flux rosin dính nối 2 trace

### Continuity test (15 phút)

DMM mode continuity (beep):

- [ ] Test short giữa **VIN battery** và **GND** → KHÔNG kêu beep (không short)
- [ ] Test short giữa **5V rail** và **GND** → KHÔNG kêu beep
- [ ] Test short giữa **3V3 rail** và **GND** → KHÔNG kêu beep
- [ ] Test short giữa **4V SIM rail** và **GND** → KHÔNG kêu beep
- [ ] Test short giữa **5V** và **12V** → KHÔNG kêu beep
- [ ] Test short giữa **3V3** và **5V** → KHÔNG kêu beep
- [ ] Test connectivity của GND giữa các IC → tất cả phải kêu beep

**Nếu phát hiện short nào**: NGỪNG, tìm lý do trước khi cấp nguồn.

---

## Phase A: Power-up 12V rail (không gắn buck)

### A.1 Setup bench supply

- Set bench supply: **12.0V**, current limit **0.5A** (thấp để bảo vệ ban đầu)
- KHÔNG cắm pin 18650 — dùng bench supply để dễ ngắt
- Kết nối: bench (+) → VBAT, bench (-) → GND

### A.2 Cấp điện

- [ ] Bật bench → đo dòng tiêu thụ: phải < 50mA (chỉ vài cap idle)
- [ ] Nếu dòng > 500mA → có vấn đề, current limit kích → tắt ngay
- [ ] Đo VBAT bằng DMM: phải = 12.0V ± 0.1V

**Pass criteria**:
- Bench draw < 50mA
- VBAT = 12.0V
- Không IC nào nóng (sờ tay được)

### A.3 Đo các điểm test (chưa cắm buck/LDO)

- [ ] VBAT test point: 12.0V
- [ ] Tất cả node khác: 0V (chưa có buck output)

---

## Phase B: Bring up Buck #1 (5V rail)

### B.1 Tháo Buck #2 và LDO (tạm thời)

Mục đích: bring-up từng cái một, không cho nhau ảnh hưởng.

- [ ] Tháo LM2596S #2 khỏi socket (hoặc lift pin VIN nếu hàn SMD — dùng nhíp ép nhẹ)
- [ ] Tháo AP2112K LDO khỏi mạch

Nếu không socket (SMD) → bỏ qua tháo, nhưng phải đảm bảo Buck #2 và LDO không có cap input lớn → no startup spike.

### B.2 Cấp nguồn → đo 5V

- [ ] Bench 12V on, dòng limit 1A
- [ ] Đo **5V rail**: phải = 5.00V ± 0.15V
- [ ] Nếu lệch > 0.5V → kiểm tra feedback resistor R1, R2
- [ ] Nếu Vout = Vin (12V): buck **không switch** → kiểm tra inductor L1, diode D1 (có thể đặt ngược)
- [ ] Nếu Vout = 0V: feedback ngược, hoặc IC chết

### B.3 Ripple test với oscilloscope (nếu có)

- AC coupling, 20MHz bandwidth limit, time 5µs/div
- [ ] Đo ripple 5V: phải < 50mV peak-to-peak
- [ ] Quan sát có spike > 200mV không? Nếu có → kém decoupling, thêm cap

### B.4 Load test

- Resistor 10Ω 10W (= 0.5A load) → cắm giữa 5V và GND
- [ ] Dòng vào battery 12V: phải ≈ 250mA (0.5 × 5 / 12 / 0.85 = 245mA do efficiency 85%)
- [ ] 5V vẫn = 5.00V ± 0.15V
- [ ] LM2596S nóng nhẹ (< 50°C) sau 1 phút

Resistor 2.2Ω 50W → 2.3A load:
- [ ] Dòng vào battery: ≈ 1.1A
- [ ] 5V vẫn ổn định (đo bằng DMM, không sụt)
- [ ] LM2596S < 70°C
- [ ] Đo ripple: vẫn < 50mV pp

**Pass criteria Phase B**:
- 5V = 5.00V ± 0.15V no load và load
- Ripple < 50mV pp
- IC không quá nóng

---

## Phase C: Bring up LDO 3.3V

### C.1 Gắn lại AP2112K

- [ ] Hàn/cắm lại LDO

### C.2 Cấp nguồn → đo 3.3V

- [ ] Bench 12V, Buck #1 đã working (5V OK)
- [ ] Đo **3.3V rail**: phải = 3.30V ± 0.10V
- [ ] Đo dòng vào battery: tăng thêm ~ 10mA (LDO quiescent + chính nó)

### C.3 Load test

- Resistor 33Ω 1W (= 100mA load on 3.3V) → giữa 3.3V và GND
- [ ] 3.3V vẫn = 3.30V ± 0.05V
- [ ] LDO ấm nhẹ (drop 1.7V × 0.1A = 170mW)

Resistor 6.8Ω 5W (= 500mA load):
- [ ] 3.3V vẫn ổn định
- [ ] LDO nóng (~60°C — drop 1.7V × 0.5A = 850mW, không có heatsink)
- [ ] **Không vận hành lâu** > 1 phút ở 500mA — sát limit nhiệt

**Pass criteria Phase C**:
- 3.3V = 3.30V ± 0.10V
- Stable under 100mA load (normal use)
- LDO không thermal shutdown trong 5 phút ở 200mA

---

## Phase D: Bring up Buck #2 (4V rail SIM800L)

### D.1 Gắn lại LM2596S #2

- [ ] Hàn/cắm lại
- [ ] Verify R1, R2 feedback (1.0kΩ + 2.2kΩ)

### D.2 Cấp nguồn → đo 4V

- [ ] Bench 12V, Buck #1 + LDO working
- [ ] Đo **4V SIM rail**: phải = 3.95-4.05V (range 3.85-4.15V acceptable)
- [ ] Đo dòng tổng: ~+ 5mA cho Buck #2 idle

### D.3 Load test mô phỏng SIM800L burst

Khó test burst 2A/577µs với resistor đơn giản. Thay thế:

- Resistor 4Ω 10W (= 1A load liên tục)
- [ ] 4V vẫn ổn định = 4.00V ± 0.10V
- [ ] LM2596S #2 < 60°C
- [ ] Ripple < 100mV pp (chấp nhận được do tải lớn)

**Test burst** (cao cấp): dùng MOSFET driver xung 2A on/off 1ms:
- [ ] Áp 4V sụt < 0.3V trong burst
- [ ] Sau burst phục hồi trong < 50µs

Skip burst test nếu không có dụng cụ → trust bulk cap 470µF + tantalum 100µF.

**Pass criteria Phase D**:
- 4V = 4.00V ± 0.15V
- 1A continuous OK
- Ripple < 100mV

---

## Phase E: Tổng hợp — power tree hoàn chỉnh

### E.1 Tất cả rail cùng lúc

- [ ] Bench 12V cấp vào VBAT
- [ ] Đo lần lượt:
  - VBAT: 12.0V
  - 5V: 5.00V
  - 3V3: 3.30V
  - 4V_SIM: 4.00V
- [ ] Tổng dòng vào: ~80-150mA (idle, không có MCU)

### E.2 Power sequencing

LM2596S không có power-good signal nên power-up timing không deterministic. Đo bằng oscilloscope (nếu có):

- Cấp 12V → đo 5V và 3.3V cùng lúc, time 10ms/div
- [ ] 5V đạt 4.5V trong < 5ms
- [ ] 3.3V đạt 3V trong < 10ms (sau 5V đã ổn)
- [ ] Không có rail nào overshoot > 110% target

### E.3 Battery monitor (ADC)

- [ ] Đo voltage divider output ở chân ADC GPIO1 của ESP32 footprint
- [ ] Tính: VBAT × 33 / 133 = 12.0 × 0.248 = 2.98V
- [ ] DMM đo phải = 2.98V ± 0.05V
- [ ] Sweep VBAT từ 9V → 12.6V → ADC tracking linearly

---

## Phase F: Pin 18650 và charge test

### F.1 Cắm pin lần đầu

Chỉ làm sau khi đã pass Phase A-E với bench supply.

- [ ] Cắm 3 cell 18650 (đã sạc đầy 12.6V) vào holder
- [ ] **CẨN THẬN**: đảm bảo BMS có chống cắm ngược; sai chiều = nổ pin
- [ ] Đo VBAT: phải = 12.6V (full)
- [ ] Robot rails tự bật, đo lại 5V/3.3V/4V → vẫn ổn

### F.2 Charge test

- [ ] Cắm dock 12.6V/1A vào copper contact
- [ ] BMS bắt đầu sạc — LED đỏ trên BMS
- [ ] Đo dòng sạc: ~ 0.8-1.0A
- [ ] Đo VBAT: tăng dần (sạc CC mode)
- [ ] Khi VBAT đạt 12.6V: chuyển CV mode, dòng giảm
- [ ] Khi dòng < 100mA: sạc xong, LED xanh

### F.3 Cycle test

- [ ] Sạc đầy (12.6V)
- [ ] Ngắt dock, robot chạy idle (chỉ rails on, no MCU)
- [ ] Sau 5h: VBAT vẫn > 11.0V → leakage current OK
- [ ] Cắm lại dock, sạc đầy

---

## Phase G: 1 giờ burn-in

Để mạch chạy 1 giờ liên tục dưới load điển hình:
- 5V load: 1A (resistor 5Ω 10W)
- 3.3V load: 100mA
- 4V load: 350mA (mô phỏng SIM800L average)

Trong giờ đó:
- [ ] Đo tất cả rail mỗi 15 phút — không drift > 50mV
- [ ] Sờ tay tất cả IC mỗi 15 phút — không quá nóng (< 60°C)
- [ ] Đo dòng tổng — không có rò rỉ (current không tăng dần)

**Sau 1 giờ pass → Power tree sẵn sàng cho Phase 3 (cắm MCU)**.

---

## Troubleshooting nhanh

| Triệu chứng | Nguyên nhân khả dĩ | Cách xử lý |
|-------------|---------------------|------------|
| Buck Vout = 0V | IC ngược, feedback short, D1 ngược | Kiểm tra orientation và polarity |
| Buck Vout = Vin (~12V) | Không switching → D1 hở/ngược hoặc L1 hở | Đo continuity D1, L1 |
| Vout dao động 0-V_target | Feedback noise → R2 hở hoặc cap output thiếu | Thêm 22µF song song với Cout |
| Vout đúng nhưng ripple > 100mV | ESR cap output cao | Thay cap thường bằng low-ESR aluminum |
| IC nóng > 80°C | Switching frequency sai, hoặc load > rating | Đo dòng output, kiểm tra L1 saturation |
| LDO drop > 0.5V | Vin quá thấp, hoặc dropout chip lớn | Verify Vin = 5V (không sụt khi load) |
| Battery sense ADC sai | Voltage divider tỷ lệ sai | Đo R1, R2; tính lại 33/(100+33) |
| Charge không sạc | Diode dock ngược, BMS đã trigger protect | Reset BMS bằng cách cắm-rút |

---

## Sign-off

Khi tất cả phase A-G pass:

- [ ] Ký tên + ngày: ____________________ ____________
- [ ] Lưu kết quả đo vào `docs/test-results/phase2-power-bringup.md`
- [ ] Chụp ảnh PCB powered → `docs/photos/phase2-power-on.jpg`
- [ ] Backup gerber + BOM dùng đặt PCB này (lưu Phase 2 release): `hardware/releases/v1.0-power-only/`

→ **Tiếp theo**: Phase 3 — cắm ESP32-S3-CAM module và bring-up MCU.
