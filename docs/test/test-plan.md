# Formal Test Plan — Elderly Companion Robot

> Bộ test chính thức để chấp nhận robot trước khi bàn giao production. Pass tất cả 6 test category + 24h soak là tiêu chuẩn ship.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 11 (HDSD)
> **Đối tượng**: Robot hoàn chỉnh sau Phase 10 (tất cả subsystem hợp nhất).

---

## 0. Pre-test setup

### 0.1 Environment

- [ ] Phòng test 3×3m thoáng, ánh sáng đều, không gương lớn
- [ ] Dock station lắp tường, cao 80mm, có sóng AC 220V
- [ ] WiFi 2.4GHz signal > -65 dBm tại khu vực test
- [ ] SIM card 2G còn tiền ≥ 50k, PIN disabled, signal RSSI ≥ 15
- [ ] Pin 18650 đầy 100% (12.6V đo bằng DMM)

### 0.2 Test team

| Vai trò | Người |
|---------|-------|
| Tester chính (chạy test) | ___________ |
| Người nhận SMS/call SOS | ___________ |
| Quan sát (note kết quả) | ___________ |

### 0.3 Equipment checklist

- [ ] Laptop có browser (mở web UI)
- [ ] Smartphone (xem stream, nhận SOS, dùng joystick mobile)
- [ ] DMM (đo voltage)
- [ ] Oscilloscope (optional, debug ripple/PWM)
- [ ] Thước dây 4m
- [ ] Đồng hồ bấm giây
- [ ] Vật chắn cứng (hộp giấy 30×30cm)
- [ ] Đệm xốp lớn (cho drop test fall detection)
- [ ] Biểu mẫu `test-results.md` để ghi kết quả

---

## Test 1 — Smoke test (1 giờ)

**Mục tiêu**: System cơ bản healthy, tất cả task chạy, không panic.

### 1.1 Boot sequence

- [ ] Cắm pin → robot boot (nghe motor click, OLED sáng)
- [ ] Serial log: tất cả 9 task init success (không có ERROR)
- [ ] `/status` JSON: `phase=10`, `free_heap > 150000`, `psram_size_mb=8`
- [ ] OLED hiển thị 8 dòng status đầy đủ

### 1.2 Self-test endpoint

```bash
curl http://<ip>/diag/selftest | jq
```

Tất cả subsystem phải `"pass": true`:
- [ ] wifi
- [ ] i2c.mpu6050
- [ ] i2c.ssd1306
- [ ] battery
- [ ] ptz
- [ ] motor (smoke pulse — robot trên giá)
- [ ] audio
- [ ] sim800
- [ ] ir_dock

### 1.3 1-giờ idle

- [ ] Robot bật, không command gì
- [ ] OLED refresh đều đặn, không freeze
- [ ] Sau 1 giờ:
  - [ ] Không reboot
  - [ ] `min_free_heap > 100000` (không leak)
  - [ ] WiFi RSSI vẫn ổn định
  - [ ] Battery voltage tự giảm ~0.1V/giờ (idle bình thường)
  - [ ] Tất cả task vẫn responsive (bấm vài button trong UI)

### Pass criteria
- [ ] All boot init OK
- [ ] Self-test 9/9 pass
- [ ] 1h idle stable

---

## Test 2 — Drive train (30 phút)

**Mục tiêu**: Robot di chuyển chính xác, an toàn.

### 2.1 Straight line accuracy

Robot xuống sàn, khu vực trống 3m.

- [ ] `linear=50&angular=0`, run 2 giây → đi thẳng ~50cm
- [ ] Đo lệch trục: < 10cm tại 1m (calibrated trim)
- [ ] Đo lệch trục: < 20cm tại 2m

Nếu fail → re-trim qua `/drive/calibrate`.

### 2.2 Rotation accuracy

- [ ] `linear=0&angular=60`, run 6s → quay ~360°
- [ ] Đo góc lệch sau 1 vòng: < 30° (không phải 360 chính xác do open-loop, OK)

### 2.3 Obstacle response time

- [ ] Robot drive forward toward wall
- [ ] Đo thời gian từ wall < 15cm đến motor brake: **< 200ms**
- [ ] Không có overshoot va đụng tường

### 2.4 Watchdog brake

- [ ] Robot drive forward
- [ ] Tắt WiFi điện thoại (mất command)
- [ ] Đo thời gian đến motor brake: **< 600ms** (500ms watchdog + 1 tick)

### 2.5 Emergency stop response

- [ ] Robot drive full speed
- [ ] Bấm "Emergency Stop" trong UI
- [ ] Đo thời gian brake: **< 50ms**

### Pass criteria
- [ ] Straight 2m < 20cm lệch
- [ ] Obstacle brake < 200ms
- [ ] Watchdog brake < 600ms
- [ ] E-stop < 50ms

---

## Test 3 — Camera + audio (1 giờ)

### 3.1 Camera stream 30 phút

- [ ] Mở `http://<ip>/stream` trong browser
- [ ] Để chạy 30 phút liên tục
- [ ] Đo FPS (đếm frame trong 10s): **≥ 15 FPS**
- [ ] Không drop frame nghiêm trọng (visible glitch)
- [ ] Latency input→display: **< 500ms** (chuyển động tay trước cam, đối chiếu)

### 3.2 PTZ control

- [ ] Pan 10° → 170° trong < 5s
- [ ] Tilt 30° → 150° trong < 5s
- [ ] Stream sync với PTZ — không lag, không tearing

### 3.3 Audio loopback

- [ ] Record 3s → download WAV → play PC: nghe rõ giọng "test 1 2 3"
- [ ] Loopback 3s mic → speaker: hiểu được nội dung
- [ ] Tone test 440Hz và 1kHz: pitch chính xác bằng tai

### 3.4 Combined load

- [ ] Stream + PTZ random move + Audio loopback **đồng thời** trong 5 phút
- [ ] Tất cả vẫn responsive
- [ ] CPU ESP32 không reach watchdog timeout

### Pass criteria
- [ ] Camera FPS ≥ 15 trong 30 phút
- [ ] Audio loopback rõ ràng (nghe hiểu)
- [ ] Combined load không lag/freeze

---

## Test 4 — Docking reliability (20 lần lặp)

**Mục tiêu**: Auto-dock success rate ≥ 80%.

### 4.1 Standard approach

Mỗi attempt:
- Robot start cách dock **1m**, mặt trước hướng dock **±30°**
- Bấm "Auto-dock" hoặc `/dock/start`
- Đo thời gian SEARCH→CHARGING

### 4.2 Record results

| # | Start angle | Success? | Time (s) | Notes |
|---|-------------|----------|----------|-------|
| 1 | 0° | | | |
| 2 | +15° | | | |
| 3 | -15° | | | |
| ... | ... | ... | ... | ... |
| 20 | 0° | | | |

### 4.3 Acceptance

- [ ] Success rate ≥ 16/20 (80%)
- [ ] Trung bình thời gian dock < 60s
- [ ] Lần thất bại: log SEARCH timeout hoặc CONTACT timeout (không hỏng vật lý)

### 4.4 Edge case — dock fail mid-approach

- [ ] Trigger `/dock/start`
- [ ] Khi robot đang APPROACH, đặt vật chắn giữa robot và dock
- [ ] Robot phải: obstacle brake + log "beacon lost" + retry SEARCH
- [ ] Không đụng vật chắn

---

## Test 5 — Fall detection (10 lần)

**Mục tiêu**: True positive ≥ 9/10, false positive 0/10.

### 5.1 Drop test (true positive)

Đặt robot trên đệm xốp 30cm cao.

| # | Drop height | FALL trigger? | Time to SMS (s) | Notes |
|---|-------------|---------------|-----------------|-------|
| 1 | 30cm | | | |
| 2 | 30cm | | | |
| ... | | | | |
| 10 | 30cm | | | |

- [ ] **TP rate ≥ 9/10**
- [ ] Trigger time < 1s
- [ ] SMS đến phone < 30s

### 5.2 False positive test

Phải KHÔNG trigger:

| # | Stimulus | Trigger? (phải N) |
|---|----------|--------------------|
| 1 | Đẩy robot mạnh forward | |
| 2 | Đập tay xuống bàn cạnh robot | |
| 3 | Quay robot 360° nhanh | |
| 4 | Robot đi qua sàn xóc nhẹ | |
| 5 | Cầm robot lên bằng tay (giữ thẳng) | |
| 6 | Robot lắc bên hông 15° | |
| 7 | Va chạm robot khác (nhẹ) | |
| 8 | Robot đụng tường ở tốc độ cruise | |
| 9 | Robot reverse đụng vật mềm | |
| 10 | Vận hành PATROL bình thường 5 phút | |

- [ ] **FP rate 0/10**

---

## Test 6 — SOS emergency (3 lần)

**Mục tiêu**: SOS dispatch reliable.

### 6.1 Manual trigger

⚠️ **Báo trước người nhận** rằng đây là test.

| # | Trigger source | SMS p1? | SMS p2? | Call p1? | Total time (s) |
|---|----------------|---------|---------|----------|----------------|
| 1 | Manual /sos/trigger | | | | |
| 2 | Drop test (fall) | | | | |
| 3 | Manual button UI | | | | |

- [ ] SMS đến p1 < 30s từ trigger
- [ ] SMS đến p2 < 60s
- [ ] Voice call ring trong < 30s

### 6.2 SIM800L recovery

- [ ] Tắt SIM800L (rút antenna 30s rồi cắm lại)
- [ ] Trigger SOS
- [ ] Module tự re-power-on + register network
- [ ] SOS vẫn dispatch (có thể chậm 30-60s lần đầu sau recovery)

---

## Test 7 — Edge cases (15 phút)

### 7.1 WiFi flap

- [ ] Robot đang PATROL
- [ ] Tắt router 30s
- [ ] Robot: motor brake trong < 600ms (no command → watchdog)
- [ ] OLED hiển thị `wifi: -`
- [ ] Bật lại router → robot reconnect tự động trong < 30s
- [ ] Web UI resume

### 7.2 Brownout test

- [ ] Pin yếu (~10V)
- [ ] Robot drive full speed
- [ ] Brownout detector kick in trước khi crash chip?
  - Log "brownout" + auto reboot → OK
  - Random crash + panic → fail, cần điều chỉnh CONFIG_ESP_BROWNOUT_DET_LVL

### 7.3 Motor stall

- [ ] Robot drive forward
- [ ] Tay nắm bánh xe lại trong 3s
- [ ] L298N không thermal shutdown
- [ ] Robot tự brake (current limit hoặc obstacle gate kick in)
- [ ] Sau khi nhả tay, robot vẫn responsive

### 7.4 Multiple simultaneous events

- [ ] Robot đang RETURN_HOME (auto-dock active)
- [ ] Gây fall event giữa chừng
- [ ] SOS_ACTIVE preempt → motor brake
- [ ] SMS gửi success
- [ ] Sau cooldown, robot resume RETURN_HOME

### 7.5 Battery critical

Manual force `VBAT_LOW_V` lên 12V tạm thời để test:
- [ ] Robot từ PATROL → RETURN_HOME tự động
- [ ] OLED hiển thị `LOW` flag

---

## Test 8 — 24h soak

Xem `24h-soak-plan.md` cho chi tiết.

---

## Sign-off

**Tester**: ____________________  **Date**: _______________

| Test | Pass / Fail | Notes |
|------|-------------|-------|
| 1. Smoke test | | |
| 2. Drive train | | |
| 3. Camera + audio | | |
| 4. Docking 20× | | (16/20 minimum) |
| 5. Fall detect 10× | | (9 TP, 0 FP) |
| 6. SOS 3× | | |
| 7. Edge cases | | |
| 8. 24h soak | | |

**Verdict**: ☐ SHIP   ☐ NEEDS REWORK

Issues tracking: lưu vào `test-results.md`.
