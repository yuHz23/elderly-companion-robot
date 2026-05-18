# Dock + Charging Bring-up

> Quy trình test auto-dock sau khi Phase 8 (SIM800L) đã pass.
>
> **Tiền đề**: Robot drive train working, sensor suite working, battery 18650 cắm đủ.
>
> **Dụng cụ**: Đồng hồ vạn năng, đồng hồ bấm giây, miếng đệm xốp, dock station đã xây xong (xem `dock-spec.md` mục 2).

---

## Phase 9.A — Build dock station (60-90 phút)

### A.1 Mua linh kiện theo BOM (mục 8 dock-spec)

Tổng ~165k VND. Riêng IR module 38kHz: dùng module pre-modulated (Shopee tag "IR transmitter 38khz") cho dễ.

### A.2 Lắp dock plate

1. Đặt acrylic 5mm 150×120mm
2. Khoan 4 lỗ M3 ở góc → vít vào tường (cao 80mm so với sàn)
3. Cắt 2 miếng đồng 60×30mm, mạ thiếc bề mặt
4. Khoan 2 lỗ M3 mỗi miếng, bắt vào dock plate:
   - Miếng `+5V` (top): y = 50mm từ đáy
   - Miếng `GND` (bottom): y = 15mm từ đáy
   - **Khoảng cách tâm-tâm**: **35mm** — đo chính xác bằng caliper
5. Hàn dây silicone 18AWG vào đồng (đỏ + đen) — luồn ra sau dock plate
6. Gắn IR beacon module ở giữa, y = 75mm, hướng ra phía trước
7. LED indicator xanh + R 1k ở góc trên, báo "POWER ON"

### A.3 Wire điện

```
AC-DC adapter 12.6V/3A
        │
       [Fuse PTC 2A]
        │
        ▼
       [Diode SS54] (anode → adapter, cathode → output)
        │
        ├──► Copper plate +
        ├──► IR beacon module VCC
        └──► LED indicator (qua R 1k)

   GND ─►── Copper plate -
        ─►── IR beacon module GND
        ─►── LED indicator cathode
```

### A.4 Verify dock standalone

Trước khi đưa robot lại gần:

- [ ] Cắm AC adapter → LED indicator xanh sáng
- [ ] Đo điện áp giữa 2 copper plate: **12.0 - 12.6V**
- [ ] Đo điện áp module IR (cấp ngoài 5V nếu module riêng): IR LED sáng (đèn IR mắt không thấy, dùng camera điện thoại để soi → thấy đốm tím)
- [ ] Đèn IR phải **flash 38kHz** — không phải on liên tục — verify bằng cách điện thoại quay slow-motion

---

## Phase 9.B — Robot-side bring-up

### B.1 Lắp spring contact + IR receiver

- [ ] Spring contact pogo gắn mặt trước robot, khoảng cách Y **35mm** (match dock)
- [ ] Hàn dây spring (+) → diode SS54 → BMS input (+)
- [ ] Hàn dây spring (-) → BMS input (-)
- [ ] TSOP38238 receiver gắn mặt trước robot, OUT → GPIO5

### B.2 Rebuild firmware

```bash
cd firmware/
pio run -e esp32-s3-cam -t upload
pio device monitor
```

Log mong đợi mới:
```
I (xxx) battery: init OK — ADC1_CH0 (GPIO1) @ 12dB atten
I (xxx) ir_dock: init OK — TSOP38238 OUT on GPIO5
I (xxx) dock: IDLE -> IDLE
```

### B.3 Verify web UI

URL `http://<ip>/` — panel "Dock & charging" hiển thị:
```
state    IDLE
battery  11.50V  62%
ir beam  0/100
```

Battery voltage phải gần đúng so với DMM đo (sai số < 0.2V).

---

## Phase 9.C — IR beacon detection (5 phút)

⚠️ **Robot rút khỏi dock**.

### C.1 Out-of-range

- Đặt robot **cách dock 2m**, mặt trước robot hướng đi nơi khác (lưng dock)
- [ ] `ir beam` panel: 0-10/100 (chỉ noise)

### C.2 Direct line-of-sight

- Quay robot mặt trước hướng dock từ 1m
- [ ] `ir beam` panel: **> 60/100** (lock threshold)
- Quay robot đi hướng khác → strength giảm về < 20

### C.3 Range test

- 1m: > 80
- 2m: > 60
- 3m: > 40
- 4m: > 20
- 5m: ~0 (out of range)

Nếu range yếu (chỉ 1m work):
- IR LED yếu, đổi LED khác
- Tăng dòng LED (giảm R 220 xuống 100Ω) — cẩn thận peak current
- Dùng 2 LED song song

---

## Phase 9.D — Manual docking dry-run (10 phút)

⚠️ **Robot trên giá đỡ, bánh treo**. Test direction trước.

### D.1 Simulate SEARCH

- [ ] Trigger `/dock/start`
- [ ] Robot bắt đầu rotate CCW
- [ ] Khi quay đến hướng dock → IR strength > 60 → log:
  ```
  I (xxx) dock: beacon locked (strength=78)
  I (xxx) dock: SEARCH -> APPROACH
  ```
- [ ] Robot bắt đầu rotate dừng, drive forward (nhưng treo nên bánh quay không di chuyển)

### D.2 Cancel

- [ ] Bấm "Cancel" trong UI
- [ ] Log:
  ```
  I (xxx) dock: APPROACH -> IDLE
  ```
- [ ] Robot brake

---

## Phase 9.E — Floor docking attempt 1 — manual final align

⚠️ **Robot xuống sàn**. Bắt đầu xa dock 1m, mặt trước hướng dock.

### E.1 First dock

1. [ ] Trigger `/dock/start`
2. [ ] Robot rotate, lock beacon, drive forward
3. [ ] Khi cách dock ~30cm: log `APPROACH -> CONTACT`, robot crawl chậm forward
4. [ ] Spring contact chạm copper plate:
   - Battery voltage panel: **+10V → +12.6V** (sạc input điện vào)
   - Log: `dock: CONTACT -> CHARGING`
5. [ ] Robot dừng motor (brake hết)
6. [ ] Panel: `battery [CHG]`, `state CHARGING`

### E.2 Nếu fail

| Triệu chứng | Fix |
|-------------|-----|
| Robot không tìm thấy dock (SEARCH timeout) | Đặt gần dock hơn lúc start; verify IR strength > 60 khi mặt vào dock |
| Lock beacon nhưng đi không tới (lost mid-way) | Beacon spread không đủ — thêm LED #2 |
| Tới dock nhưng không charge | Spring lệch khỏi plate; verify Y = 35mm |
| Charge voltage spike rồi tắt | Spring contact lỏng; bench-press spring xuống xem |
| Robot đụng dock đè bẹp | Tăng `APPROACH_NEAR_CM` hoặc giảm `CONTACT_FWD_SPEED` |

### E.3 Adjust Y alignment

Nếu spring contact không chạm plate:
- Đo lại vị trí spring trên robot
- Đo lại vị trí plate trên dock
- Sai số cumulative không > 5mm

Spring stroke 3mm chỉ tolerate ±3mm Y misalign. Plate misalign > 5mm → robot không dock được, **phải sửa cơ khí**.

---

## Phase 9.F — Repeat docking reliability (20 phút)

Lặp dock 10 lần từ các vị trí khác nhau (đều cách 0.8-1.2m, mặt trước hướng dock khoảng ±45°):

- [ ] 10/10 lần SEARCH → tìm thấy beacon
- [ ] 9/10 lần APPROACH thành công (1 lần thất bại do beam góc rộng — chấp nhận)
- [ ] 9/10 lần CONTACT → CHARGING (spring align OK)
- [ ] **Target: 80% success end-to-end** (8/10)

Nếu < 80%: tune tunables trong `task_dock.c`:
- `BEACON_LOCK_THRESHOLD` cao quá → giảm xuống 50
- `APPROACH_FWD_SPEED` chậm quá → tăng lên 40
- `CONTACT_FWD_SPEED` nhanh quá → giảm xuống 10

---

## Phase 9.G — Charging session (3 giờ)

Sau khi dock thành công lần đầu:

- [ ] Đặt robot dock + pin còn 60-70%
- [ ] State CHARGING, monitor mỗi 15 phút
- [ ] Battery voltage tăng dần: 11.5V → 11.8V → 12.0V → 12.4V → 12.6V
- [ ] Battery % tăng tương ứng

Sau ~3 giờ:
- [ ] Battery voltage steady 12.4-12.6V
- [ ] State → CHARGED (sau 5 phút steady)

### G.1 Heat check

- [ ] BMS module ấm nhẹ (< 45°C) sau 1 giờ
- [ ] AC adapter dock ấm nhẹ (< 50°C)
- [ ] Copper plate không nóng (không có short)

---

## Phase 9.H — Leave dock (2 phút)

- [ ] State = CHARGED
- [ ] Bấm "Leave dock" trong UI
- [ ] Log:
  ```
  I (xxx) dock: leave requested — driving forward
  I (xxx) dock: CHARGED -> IDLE
  ```
- [ ] Robot drive forward 1.5s (~ 30cm), tách khỏi dock
- [ ] State → IDLE
- [ ] Battery voltage tụt xuống ~12.4V (battery only, không có charger)

---

## Phase 9.I — Combined fall + dock test (15 phút)

Quy trình thực tế:
1. Robot đang patrol (drive random)
2. Battery hạ xuống < 20% (low)
3. *(Phase 12 sẽ tự trigger; Phase 9 manual)* — bấm `/dock/start`
4. Robot tự dock
5. Khi đang dock, gây fall (kéo robot ngã)
6. Verify SOS vẫn trigger được trong dock state

- [ ] Manual: cho robot 18% pin, bấm /dock/start
- [ ] Robot dock thành công
- [ ] Đang charging — gây "fall" nhỏ (chỉ test sự kiện, không thật sự ngã)
- [ ] SOS vẫn dispatch (SMS + dial) — không bị block bởi dock task

---

## Common pitfalls

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| State stuck SEARCH | IR strength noise dưới 60 | Tăng IR LED power, hoặc giảm threshold |
| State CHARGED không vào | VBAT chưa stable 5 phút | Đợi thêm, check BMS thực sự kết thúc CC |
| Robot leave dock đụng tường | LEAVE_DURATION_MS quá dài | Giảm xuống 1000ms |
| Pin xả khi robot trên dock idle | Diode SS54 dropout không đủ | Verify diode forward voltage 0.4V không leak |
| Multiple IR sources interference | TV remote, sunlight | Đặt dock xa TV + tránh nắng |
| Robot dock thành công nhưng restart vài lần | Spring contact intermittent | Mạ thiếc plate dày hơn |

---

## Phase 9 — Sign-off

- [ ] Dock station built, đo plate 12.6V, IR beam modulated 38kHz
- [ ] Robot phát hiện beacon strength > 60 ở 1m line-of-sight
- [ ] SEARCH timeout < 30s khi robot start đối diện dock
- [ ] CONTACT thành công 8/10 lần từ 1m
- [ ] CHARGING tăng voltage đều đặn → CHARGED sau ~3h
- [ ] LEAVE dock tách khỏi charger ngon
- [ ] SOS vẫn work khi đang dock

→ **Tiếp theo**: Phase 10 — Firmware integration. Hợp nhất tất cả task FreeRTOS, behavior state machine cấp cao (IDLE / PATROL / SOS / DOCKING), MQTT broker connection.
