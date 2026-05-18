# Drive Train Bring-up & Calibration

> Quy trình test 2 motor BO + L298N sau khi Phase 4 (PTZ) đã pass.
>
> **Tiền đề**: Robot đã có chassis lắp ráp, motor BO + bánh xe đã gắn, PCB power + MCU working.
>
> **Dụng cụ**: Bench supply có current limit, đồng hồ vạn năng, thước dây 2m, đồng hồ bấm giây, oscilloscope (optional cho PWM verify).

---

## QUAN TRỌNG — An toàn

🛑 **Đặt robot trên giá đỡ** (kê 2 hộp ở chassis sao cho bánh xe **không chạm sàn**) trong tất cả test 5.A → 5.D. Chỉ cho robot xuống sàn ở phase 5.E sau khi command đã verify đúng.

🛑 **Có nút reset MCU trong tầm tay** — để cắt nguồn khẩn cấp nếu motor không dừng.

🛑 **Bench supply current limit 1A** ban đầu — phòng short bất ngờ.

---

## Phase 5.A — Build verify (5 phút)

### A.1 Rebuild với motor driver mới

```bash
cd firmware/
pio run -e esp32-s3-cam -t upload
pio device monitor
```

Mong đợi log mới:
```
I (xxx) motor: init OK — ENA=6 ENB=11, 20kHz/10-bit, cap=70%, dz=20%
I (xxx) nav: loaded trim: left=0% right=0%
I (xxx) nav: task started — watchdog=500ms tick=20ms
```

Nếu không thấy → CMakeLists require thiếu hoặc include path sai.

### A.2 Mở Web UI

URL: `http://<ip>/`

Mong đợi (mới so với Phase 4):
- Joystick 2D bên trái panel
- Nút "Emergency Stop" màu đỏ
- Status panel thêm dòng `drive  lin=0 ang=0 idle`

---

## Phase 5.B — Direction verification (10 phút)

**Robot vẫn trên giá đỡ, bánh treo không chạm sàn.**

### B.1 Test motor LEFT — forward

Bench supply 12V, current limit 1A. Cấp 12V vào VBAT.

Trong web UI: kéo joystick **lên + trái** một chút (linear=20, angular=0) — actually pure forward:
```bash
curl http://<ip>/drive/velocity?linear=30&angular=0
```

- [ ] Bánh trái quay theo hướng "forward" (đầu robot tiến)
- [ ] Bánh phải quay theo hướng "forward"
- [ ] Đèn LED trên L298N module (báo IN1, IN3) sáng

**Nếu bánh quay NGƯỢC chiều mong muốn**:
- Đảo dây +/- của motor đó tại terminal block L298N
- Hoặc đổi trong code: swap MOTOR_FWD ↔ MOTOR_REV trong `motor_l298n.c`

### B.2 Test reverse

```bash
curl http://<ip>/drive/velocity?linear=-30&angular=0
```

- [ ] Cả 2 bánh quay NGƯỢC chiều của test B.1
- [ ] LED IN2, IN4 sáng

### B.3 Test rotate CW

```bash
curl http://<ip>/drive/velocity?linear=0&angular=30
```

Joystick kéo sang phải. Quy ước: angular dương = CW (robot quay phải khi nhìn từ trên xuống).

- [ ] Bánh trái quay forward
- [ ] Bánh phải quay reverse

### B.4 Test rotate CCW

```bash
curl http://<ip>/drive/velocity?linear=0&angular=-30
```

- [ ] Bánh trái quay reverse
- [ ] Bánh phải quay forward

### B.5 Watchdog timeout

Gửi forward 50:
```bash
curl http://<ip>/drive/velocity?linear=50&angular=0
```

Đợi 1 giây, không gửi lệnh tiếp.

- [ ] Sau ~500ms, 2 motor **tự dừng** (brake)
- [ ] Log serial: `W (xxx) nav: watchdog expired — brake`

**Nếu motor không dừng** → bug task_navigation hoặc clock sai. Nhấn nút reset ngay.

---

## Phase 5.C — PWM measurement (optional, 5 phút)

Cần oscilloscope. Probe vào pin ENA test point.

- [ ] Set `linear=50` → đo PWM duty: ~50% (dead zone 20% + 50% × 50% = 45%)
- [ ] Tần số: 20 kHz ± 1 kHz
- [ ] Tín hiệu sạch, không ringing > 1V

Probe IN1:
- [ ] Khi forward: IN1 = HIGH (3.3V)
- [ ] Khi reverse: IN1 = LOW (0V)
- [ ] Khi brake: IN1 = HIGH (cả IN2 cũng HIGH)

---

## Phase 5.D — Dead zone tuning

Default dead zone 20%. Test xem motor cụ thể của bạn:

### D.1 Tìm minimum speed

Giảm dần `linear` từ 100 xuống cho đến khi motor dừng quay:

```bash
for i in 30 20 15 10 5 1; do
  echo "Testing linear=$i"
  curl "http://<ip>/drive/velocity?linear=$i&angular=0"
  sleep 2
done
```

- [ ] Ghi giá trị min mà 2 bánh vẫn quay đều
- [ ] Nếu min > 5% → dead zone OK
- [ ] Nếu min = 30% (1 bánh đứng yên dưới 30%) → tăng `DEAD_ZONE_PCT` lên 30, rebuild

Edit `motor_l298n.c`:
```c
#define DEAD_ZONE_PCT   30   // ← tăng theo motor thực
```

---

## Phase 5.E — Floor test (15 phút)

**Robot xuống sàn**. Khu vực 2×2m thoáng, không vật cản.

### E.1 Straight line 1m

```bash
# Người đứng cuối khoảng cách 1m
curl http://<ip>/drive/velocity?linear=50&angular=0 && sleep 2 && curl http://<ip>/drive/stop
```

- [ ] Robot đi thẳng 1m trong ~2s
- [ ] Đo độ lệch khỏi đường thẳng tại 1m:
  - < 5cm = OK (no trim cần)
  - 5-15cm = lệch nhẹ, cần trim
  - > 15cm = motor quá mismatched, kiểm tra bánh xe có vênh không

### E.2 Calibrate trim (nếu cần)

Robot lệch sang **trái** 10cm tại 1m → motor trái nhanh hơn → giảm motor trái:

```bash
curl "http://<ip>/drive/calibrate?left_trim=-5&right_trim=0"
```

Test lại E.1. Lặp cho đến khi lệch < 5cm.

**Trim lưu NVS** — persist qua reboot.

### E.3 Rotate 360°

```bash
# Đo thời gian quay 360°
curl http://<ip>/drive/velocity?linear=0&angular=60 && sleep 4 && curl http://<ip>/drive/stop
```

Đo góc thực tế quay:
- [ ] Nếu quá < 360° → tăng thời gian sleep
- [ ] Nếu robot không quay tại chỗ mà di chuyển → 2 motor ko balance, trim lại

### E.4 Joystick UI test

- [ ] Mở web UI từ điện thoại (cùng WiFi)
- [ ] Kéo joystick forward → robot tiến
- [ ] Kéo joystick CCW → robot quay trái
- [ ] Thả tay → robot dừng (drop trong < 200ms)
- [ ] Latency: kéo joystick → robot phản ứng < 200ms

---

## Phase 5.F — Safety stress test (10 phút)

### F.1 WiFi disconnect during drive

- [ ] Kéo joystick forward → robot đi
- [ ] Tắt WiFi của điện thoại
- [ ] Trong < 600ms (watchdog 500ms + 1 tick) robot phải brake

### F.2 Browser tab close

- [ ] Kéo joystick → drive
- [ ] Đóng tab trình duyệt đột ngột
- [ ] Robot brake trong < 600ms

### F.3 Emergency stop button

- [ ] Đang drive nhanh (linear=80)
- [ ] Bấm nút "Emergency Stop"
- [ ] Robot brake **ngay lập tức** (< 50ms)
- [ ] Joystick tự reset về centre

### F.4 Stall test (cẩn thận)

- [ ] Đặt vật chặn trước robot
- [ ] Drive forward 50% — robot va vào vật
- [ ] Đo dòng peak 12V: < 4A (2 motor stall mỗi 1.5A + L298N quiescent)
- [ ] Sau 5s stall, fuse vẫn chưa trip
- [ ] Tắt command — motor dừng, không bị hỏng

---

## Phase 5.G — Long-run mileage test

### G.1 30 phút tự động đi vòng

Script chạy random navigation:

```bash
#!/bin/bash
# Random drive 30 minutes — keep robot in 1m² area
for i in {1..900}; do
  lin=$((RANDOM % 100 - 50))   # -50..50
  ang=$((RANDOM % 60 - 30))    # -30..30
  curl -s "http://<ip>/drive/velocity?linear=$lin&angular=$ang" > /dev/null
  sleep 2
done
curl http://<ip>/drive/stop
```

- [ ] 30 phút sau:
  - L298N body ấm nhưng < 50°C (sờ tay được)
  - Bánh xe không lỏng / không drift offset
  - Battery 12V vẫn > 11V (chưa cạn)
  - Không có log lỗi serial

---

## Common issues

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| Cả 2 motor không quay | L298N VSS không có 5V | Đo VSS pin = 5V; kiểm tra dây |
| 1 motor không quay | ENA hoặc ENB pin không nhận PWM | Đo bằng scope; verify GPIO config |
| Motor quay nhưng yếu | L298N VS không có 12V | Đo VS pin = 12V; kiểm tra fuse |
| 1 bánh nhanh hơn nhiều | Motor mismatched | Trim ±10% có thể không đủ → đổi motor hoặc swap |
| Motor giật, không smooth | PWM 1kHz nhiều noise audible | Verify code dùng 20kHz |
| Motor kêu rít | PWM < 18kHz | Tăng `MOTOR_FREQ_HZ` lên 20000 |
| Robot không dừng | task_navigation crash | Reset, kiểm tra serial log có panic |
| Stall trip fuse PTC | Fuse rating thấp | Đổi fuse 5A thay vì 3A |
| ESP32 reboot khi drive | 5V rail sụt do back-EMF | Thêm cap 1000µF trên VS L298N |

---

## Phase 5 — Sign-off

Pass tất cả:

- [ ] Both motors run forward & reverse correctly
- [ ] Rotate CW & CCW work
- [ ] Watchdog brakes after 500ms no command
- [ ] Floor straight line < 5cm drift at 1m (after trim)
- [ ] Joystick UI smooth, latency < 200ms
- [ ] Emergency stop instant
- [ ] WiFi drop → auto-brake
- [ ] 30-min run no overheat, no drift

→ **Tiếp theo**: Phase 6 — Sensor suite (IMU MPU6050 + 4× HC-SR04 ultrasonic) để add obstacle avoidance vào nav.
