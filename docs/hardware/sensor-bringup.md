# Sensor Suite Bring-up

> Quy trình test IMU + 4× ultrasonic sau khi Phase 5 (drive train) đã pass.
>
> **Tiền đề**: Drive train working, robot có thể di chuyển bằng joystick UI.
>
> **Dụng cụ**: Thước dây 4m, vật chắn (hộp giấy ~30×30cm), bàn phẳng để IMU calibrate, đồng hồ bấm giây.

---

## Phase 6.A — Build & I2C scan (5 phút)

### A.1 Rebuild

```bash
cd firmware/
pio run -e esp32-s3-cam -t upload
pio device monitor
```

Log mong đợi (mới so với Phase 5):
```
I (xxx) i2c_bus: init OK — SDA=21 SCL=22 @ 400 kHz
I (xxx) mpu6050: init OK (WHO_AM_I=0x68)
I (xxx) mpu6050: offsets loaded: a=(0,0,0) g=(0,0,0)
I (xxx) hcsr04: init OK (4 sensors)
I (xxx) sensors: task started @ 20Hz
```

### A.2 Nếu I2C fail

```
E (xxx) mpu6050: device not found at 0x68 — check wiring/pull-ups
```

Debug:
- [ ] Đo SDA và SCL → phải = +3.3V khi idle (pull-up effective)
- [ ] Probe scope: I2C transaction phải có START/ACK/STOP đúng
- [ ] Verify AD0 pin của MPU6050 = GND (0x68); = VCC sẽ thành 0x69 → driver không tìm thấy

---

## Phase 6.B — IMU baseline test (10 phút)

### B.1 Mở Web UI

URL `http://<ip>/` — phải thấy panel "Sensors" có 4 distance + IMU values.

Mong đợi khi robot đứng yên trên bàn phẳng:
- Pitch ≈ 0 ± 5°
- Roll ≈ 0 ± 5°
- Tilt ≈ 0 ± 5°
- accel ≈ 1.00g (chỉ trọng lực)

Nếu pitch/roll lệch > 10° khi đặt phẳng → cần calibrate.

### B.2 IMU calibrate

1. Đặt robot lên **bàn phẳng đo bằng level** (không nghiêng)
2. Đảm bảo robot **không rung** (không gần motor đang chạy, không gần loa)
3. Bấm nút "Calibrate IMU" trong web UI
4. Đợi ~1 giây
5. Log:
   ```
   I (xxx) mpu6050: calibrate done: a=(ax,ay,az) g=(gx,gy,gz) saved=1
   ```
6. Pitch/Roll/Tilt sau calibrate phải < 1°

Offset lưu NVS, persist qua reboot.

### B.3 IMU tilt test

- [ ] Nghiêng robot 30° sang trái → roll ≈ -30°, tilt ≈ 30°
- [ ] Nghiêng robot 45° forward → pitch ≈ +45°, tilt ≈ 45°
- [ ] Trả về phẳng → pitch/roll/tilt ≈ 0°

Pitch/Roll dấu có thể khác (depend on board orientation) — không sao, chỉ cần consistent.

---

## Phase 6.C — Fall detection test (15 phút)

### C.1 Drop test

⚠️ **An toàn**: chuẩn bị tấm xốp hoặc đệm ở dưới — robot sẽ rơi.

- [ ] Cầm robot lên cao 30cm khỏi mặt bàn
- [ ] Thả tay → robot rơi xuống xốp
- [ ] Trong < 1 giây sau khi rơi, log phải có:
  ```
  W (xxx) sensors: spike 3.45g — fall candidate
  E (xxx) sensors: FALL DETECTED — tilt 78.5°
  ```
- [ ] Web UI hiển thị "⚠ FALL"
- [ ] Cooldown 30s — không trigger lại trong cùng phase

### C.2 Stand up test

- [ ] Sau khi fall detected, dựng robot lên đứng phẳng
- [ ] Sau 10s upright liên tục:
  ```
  I (xxx) sensors: fall cleared — robot upright
  ```
- [ ] Web UI hết "⚠ FALL"

### C.3 False positive guard

Test cases SHOULD NOT trigger fall:

- [ ] Lắc nhẹ robot (< 2g) → không trigger
- [ ] Đẩy robot đi nhanh (motor max speed) → không trigger
- [ ] Robot bị đứng trên mặt nghiêng 15° → không trigger (tilt < 60°)
- [ ] Đập tay xuống bàn cạnh robot → có thể có spike nhưng tilt vẫn nhỏ → không trigger

Nếu false positive xảy ra trong 5/10 lần test → tăng threshold `FALL_SPIKE_G` từ 2.5f lên 3.0f.

---

## Phase 6.D — Ultrasonic accuracy test (15 phút)

### D.1 Distance measurement

Robot đặt trên bàn, mặt FRONT hướng về tường. Đo bằng thước dây thực tế, so với `dist.front` trong UI.

| Distance thực | Mong đợi UI | Tolerance |
|---------------|-------------|-----------|
| 5 cm | 5 ± 1 cm | ±20% |
| 20 cm | 20 ± 2 cm | ±10% |
| 50 cm | 50 ± 3 cm | ±6% |
| 100 cm | 100 ± 5 cm | ±5% |
| 200 cm | 200 ± 10 cm | ±5% |
| 400 cm | 400 ± 20 cm | ±5% |
| > 400 cm | "---" (OOR) | — |
| < 2 cm | "---" hoặc 2cm | sensor blind zone |

### D.2 4-direction check

Đặt vật chắn lần lượt ở 4 hướng F/B/L/R, mỗi hướng cách robot 30cm:

- [ ] `dist.front` ≈ 30cm, các hướng còn lại > 100cm (không có vật)
- [ ] `dist.back` ≈ 30cm khi đặt vật phía sau
- [ ] `dist.left` ≈ 30cm khi đặt vật bên trái
- [ ] `dist.right` ≈ 30cm khi đặt vật bên phải

Nếu sensor đo sai hướng (ví dụ vật ở trước nhưng `dist.back` báo gần) → swap wiring TRIG/ECHO trong code (pin-mapping đã ghi 35/36, 37/38, 39/40, 42/43).

### D.3 Cross-talk verification

- [ ] Đặt 2 vật ở front (30cm) và back (60cm)
- [ ] Cả 2 reading đều đúng (không lẫn sang nhau)
- [ ] Lý do: driver round-robin từng sensor, không fire đồng thời

---

## Phase 6.E — Obstacle avoidance integration (15 phút)

⚠️ **Robot xuống sàn**. Nhưng đảm bảo khu vực 2×2m, vật chắn không vỡ.

### E.1 Front brake test

- [ ] Đặt vật chắn ở phía front, cách robot 40cm
- [ ] Kéo joystick forward (linear=80)
- [ ] Robot chạy lên gần vật:
  - Tại 40cm: linear bắt đầu giảm (slow zone)
  - Tại 15cm: linear = 0 (brake zone)
- [ ] Robot dừng cách vật 15cm, KHÔNG va vào

### E.2 Back brake test

- [ ] Đặt vật chắn phía sau robot 30cm
- [ ] Kéo joystick reverse (linear=-80)
- [ ] Robot dừng cách vật 15cm

### E.3 Rotation still works

- [ ] Đặt vật trước robot, robot dừng tiến
- [ ] Kéo joystick angular (rotate)
- [ ] Robot QUAY tại chỗ — không bị block bởi obstacle gate

Đây là behavior quan trọng: robot có thể tự thoát khỏi obstacle bằng cách quay đi.

### E.4 No false brake

- [ ] Robot ở khu vực trống (> 1m mọi hướng)
- [ ] Drive full speed 5 giây
- [ ] Không có brake nhầm
- [ ] dist.* tất cả > OBSTACLE_SLOW_CM (40cm)

---

## Phase 6.F — Combined stress test (20 phút)

Random navigation với obstacle:

```bash
#!/bin/bash
# Random drive in maze-like area for 15 minutes
for i in {1..450}; do
  lin=$((RANDOM % 80 - 30))
  ang=$((RANDOM % 80 - 40))
  curl -s "http://<ip>/drive/velocity?linear=$lin&angular=$ang" > /dev/null
  sleep 2
done
```

- [ ] Robot không va đụng vật chắn quá vài lần
- [ ] Khi va đụng nhẹ (do quán tính), obstacle gate kick in và brake
- [ ] Sensor data update đều (age < 100ms trong web UI)
- [ ] Không có log error/panic

### F.1 Power monitoring

Trong 15 phút stress:
- [ ] Battery 12V vẫn > 11V
- [ ] 5V rail vẫn ổn định (đo test point)
- [ ] MPU6050 không bị I2C timeout (log không có warn về I2C)

---

## Common issues

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| MPU6050 not found | Wiring SDA/SCL, AD0 sai, pull-up thiếu | Đo điện áp pin, verify R 4.7k pull-up |
| WHO_AM_I bất thường | Chip clone (MPU-9250 style) | Driver vẫn work với 0x70/0x71 |
| Accel/gyro noisy | DLPF chưa enable | Verify CONFIG register = 0x03 |
| Ultrasonic luôn 0xFFFF | Echo voltage > 3.3V (chưa có divider) | Hàn R 1k+R 2k divider trên Echo |
| 1 sensor OOR mãi | TRIG/ECHO chân hỏng | Swap với sensor khác để test |
| Distance giật mạnh ±20cm | Bề mặt phản xạ kém (vải, xốp) | Hạn chế gần xốp/vải; sensor giới hạn cứng |
| Robot brake liên tục no obstacle | Sensor noise, dist <40cm spurious | Add median filter 3 sample trong driver |
| Fall trigger khi rung mạnh | Threshold 2.5g quá nhạy | Tăng FALL_SPIKE_G lên 3.0 |
| Fall không trigger khi rơi | Threshold quá cao | Giảm xuống 2.2g |

---

## Phase 6 — Sign-off

- [ ] MPU6050 init OK, WHO_AM_I đúng
- [ ] IMU calibrate, pitch/roll < 1° khi phẳng
- [ ] Fall detection true positive (drop 30cm) trong < 1s
- [ ] Fall detection 0 false positive trong 10 lần lắc/đẩy
- [ ] 4× ultrasonic đo chính xác ± 5% trong 10cm-200cm
- [ ] Obstacle gate dừng robot trước vật ≤ 15cm
- [ ] Rotation vẫn work khi obstacle phía trước
- [ ] 15-min random nav không va đụng nghiêm trọng

→ **Tiếp theo**: Phase 7 — Audio I/O (INMP441 mic + MAX98357A amp + voice pipeline).
