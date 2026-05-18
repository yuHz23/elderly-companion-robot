# PTZ Bring-up & Calibration

> Quy trình test cụm pan-tilt sau khi Phase 3 (MCU + camera) đã pass.
>
> **Tiền đề**: smoke_test build OK, mở `http://<ip>/` thấy camera stream và slider Pan/Tilt/Speed.
>
> **Dụng cụ**: PC + browser, đồng hồ vạn năng (đo current), thước đo góc (hoặc app điện thoại), điện thoại để chụp servo position.

---

## Phase 4.A — Smoke verify build (5 phút)

### A.1 Rebuild & flash với servo driver mới

```bash
cd firmware/
pio run -e esp32-s3-cam -t upload
pio device monitor
```

Mong đợi log mới (so với Phase 3):
```
I (xxx) servo: loaded offsets pan=0 tilt=0
I (xxx) servo: init OK — pan GPIO44, tilt GPIO45, 50Hz, 14-bit
I (xxx) ptz: task started — pan=90 tilt=90 speed=50dps
```

Nếu không thấy → CMakeLists missing REQUIRES, hoặc include path sai.

### A.2 Mở Web UI

URL: `http://<ip>/`

Mong đợi:
- Camera stream như Phase 3
- 3 slider (Pan / Tilt / Speed) bên dưới
- 2 nút Center / Park
- Status panel hiển thị pan/tilt/target/speed/uptime/heap

---

## Phase 4.B — Bench test (chưa lắp camera)

**LƯU Ý**: Chưa gắn camera vào tilt servo. Servo chạy thử với load nhỏ trước để tránh hỏng nếu mech sai.

### B.1 Cấp nguồn từ bench supply

- 5V rail từ bench, current limit **1A**
- Cắm chỉ 1 servo (pan) vào connector PAN
- Cấp nguồn 12V cho buck → 5V → servo

### B.2 Test centering

- [ ] Power on → servo PAN nên di chuyển về vị trí 90° (centre)
- [ ] Đo dòng: ~80-100mA khi giữ vị trí
- [ ] Servo không có tiếng buzz lớn

### B.3 Slider test (UI)

Trong web UI:

- [ ] Kéo Pan slider 10° → servo quay về 10° (mất ~2s với speed=50dps)
- [ ] Kéo Pan slider 170° → servo quay sang 170°
- [ ] Đo dòng peak khi đang move: < 300mA
- [ ] Servo không bị bounce / overshoot khi đến target

### B.4 Gắn servo tilt và test

- Tắt nguồn, cắm tilt servo vào connector TILT
- Power on → cả 2 servo về centre

- [ ] Kéo Tilt slider 30° → 150° → kiểm tra phạm vi
- [ ] Đo dòng 2 servo cùng move: < 500mA

---

## Phase 4.C — Mechanical limits (10 phút)

Mỗi MG90S có dung sai. Cần tìm soft limit thực tế cho cụm đã lắp.

### C.1 Pan limit

- [ ] Pan slider kéo từ 90° giảm dần xuống 10°
- [ ] Tại điểm bắt đầu nghe tiếng "khục" (gear bottoming out) → ghi giá trị
- [ ] Soft limit thực = giá trị đó + 5° margin
- [ ] Lặp lại cho hướng tăng (90° → 170°)

Update `servo_pwm.h` nếu cần:
```c
#define SERVO_PAN_MIN_DEG    15   // ← chỉnh từ thực tế
#define SERVO_PAN_MAX_DEG   165
```

### C.2 Tilt limit

Quan trọng: kiểm tra **camera + dây cáp** không đụng chassis / pan servo khi tilt move.

- [ ] Tilt slider 90° → 30°: camera tilt xuống — nó có đụng chassis?
- [ ] Tilt slider 90° → 150°: camera tilt lên — có đụng cáp servo dưới?
- [ ] Điều chỉnh `SERVO_TILT_MIN_DEG` / `_MAX_DEG` cho an toàn

### C.3 Lưu ý cáp camera

ESP32-S3-CAM cáp ngắn 50mm. Khi pan quay 180° → cáp xoắn → kéo chân connector.

**Solution**:
- Đảm bảo cáp dư 2-3 cm khi PTZ ở centre
- Cáp cần "loop" tự do (không kẹp chặt)
- Soft limit pan 10°-170° (không full 0-180°) để giảm xoắn

---

## Phase 4.D — Calibration offset

Mỗi MG90S khác nhau ~3-5° trong sản xuất. Hiệu chuẩn để khi gọi "90°" thì camera nhìn thẳng phía trước thực sự.

### D.1 Calibrate Pan

1. Đặt robot trên bàn, có vật reference thẳng phía trước (ví dụ: thước, hoặc đường thẳng vẽ trên giấy)
2. Mở web UI, kéo Pan slider về 90°
3. Quan sát camera stream: vạch reference có nằm giữa khung hình?
4. Nếu lệch trái 5° (vạch nằm phía bên phải khung hình) → offset = -5
5. Nếu lệch phải 5° → offset = +5

Apply via curl hoặc browser address bar:
```
http://<ip>/ptz/calibrate?pan_offset=-3&tilt_offset=2
```

Offset lưu vào NVS, áp dụng vĩnh viễn (mất khi `nvs_flash_erase`).

### D.2 Verify

Reboot board → đọc log:
```
I (xxx) servo: loaded offsets pan=-3 tilt=2
```

Set lại pan=90 → camera nhìn thẳng vạch reference.

---

## Phase 4.E — Smooth motion test

### E.1 Speed range

- [ ] Speed=10dps: pan 0→180 mất ~18s. Quan sát: chuyển động chậm và mượt, không giật.
- [ ] Speed=50dps (default): mất ~3.6s. Cảm giác tự nhiên.
- [ ] Speed=200dps: mất ~0.9s. Quan sát: có thể nghe servo buzz lớn → tin hiệu PWM thay đổi quá nhanh.

Đa số use case dùng 30-80 dps.

### E.2 Concurrent move

- [ ] Vào console: gửi 2 request liên tiếp nhanh
  ```bash
  curl http://<ip>/ptz/pan?angle=30 && curl http://<ip>/ptz/tilt?angle=120
  ```
- [ ] 2 servo move song song (không tuần tự)
- [ ] Không có jitter, không "khựng" giữa chừng

### E.3 Long-run smooth

- [ ] Tự động đảo Pan 30° ↔ 170° mỗi 5s, 30 lần:
  ```bash
  for i in {1..30}; do
    curl http://<ip>/ptz/pan?angle=30; sleep 5
    curl http://<ip>/ptz/pan?angle=170; sleep 5
  done
  ```
- [ ] Servo vẫn move chính xác sau 30 cycle (không drift)
- [ ] Servo không quá nóng (sờ tay được, < 50°C)

---

## Phase 4.F — Heat & current stress

### F.1 Stall current

**Cẩn thận**: stall MG90S kéo ~1.2A peak — chỉ test ngắn, không kéo dài > 3s.

- [ ] Đặt vật chắn để servo không quay được tới target (ví dụ: tay nắm pan)
- [ ] Gửi request `pan?angle=170` khi đang ở 90°
- [ ] Đo dòng: peak ~1.0-1.2A (servo cố vượt qua chắn)
- [ ] Nhả tay sau 2s

Servo sau đó quay tới 170° bình thường. Nếu bánh răng kêu lạ → có thể đã bị nứt → thay servo.

### F.2 Soak test

- [ ] Để robot tự đảo PTZ 30 phút (Pan random 30-170°, Tilt random 50-130°, 3s interval)
- [ ] Cuối 30 phút:
  - Cả 2 servo vẫn move chính xác
  - Body servo ấm (< 50°C) — không quá nóng
  - 5V rail vẫn ổn định (đo tại test point)
  - Không có log lỗi trong serial monitor

---

## Phase 4.G — Common issues

| Triệu chứng | Nguyên nhân | Cách fix |
|-------------|-------------|---------|
| Servo không quay, dòng = 0mA | Tín hiệu PWM không tới chân, hoặc cáp lỏng | Đo PWM bằng oscilloscope tại GPIO44/45 — phải có sóng vuông 50Hz |
| Servo quay quá ngắn (chỉ 30-40°) | PWM range hẹp (1.0-2.0ms cho MG90S full range) | Mở rộng trong `servo_pwm.c`: `PULSE_MIN_US 600, PULSE_MAX_US 2400` |
| Servo quay quá xa, đụng cản | Mở rộng PWM nhưng quên cập nhật soft limit | Set lại `SERVO_*_MIN/MAX_DEG` |
| Buzz lớn khi đứng yên | PWM signal không ổn định | Thêm LP filter R 1k + C 100nF trên đường PWM |
| Servo rung giữ vị trí | 5V rail sụt khi load thay đổi | Tăng cap output buck #1 lên 470µF |
| 2 servo move không đồng bộ | Speed quá cao + xử lý sai trong task | Verify `step_axis` áp dụng cùng `step` cho cả 2 axis |
| Servo "chết" sau stress test | Bánh răng nhựa nứt (SG90 only) | Thay sang MG90S kim loại |
| ESP32 reboot khi move 2 servo | Sụt áp 5V do peak current 1.2A | Thêm cap bulk gần ESP32-S3-CAM (470µF) |

---

## Phase 4 — Sign-off

Pass tất cả là OK:

- [ ] Build + flash + web UI hiển thị slider PTZ
- [ ] Pan move 10° → 170° trong soft limit
- [ ] Tilt move 30° → 150° không đụng chassis/cáp
- [ ] Center / Park button hoạt động
- [ ] Speed slider thay đổi tốc độ
- [ ] Calibration offset lưu NVS, restore sau reboot
- [ ] Soak test 30 phút không drift / overheat / reboot
- [ ] Camera stream vẫn ổn định trong khi PTZ move

→ **Tiếp theo**: Phase 5 — Drive train L298N (2 motor DC + bánh xe).
