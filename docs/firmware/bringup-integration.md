# Phase 10 — Integration Bring-up

> Quy trình test khi tất cả 9 task FreeRTOS chạy đồng thời, behavior FSM điều phối hành vi. Đây là **first all-up smoke test** sau khi từng phase đã pass riêng.
>
> **Tiền đề**: Phase 3 → 9 đều pass riêng lẻ. Robot lắp ráp hoàn chỉnh, dock station built, SIM card cắm sẵn.

---

## Phase 10.A — Compile-time integration

### A.1 Rebuild với tất cả task

```bash
cd firmware/
pio run -e esp32-s3-cam -t fullclean   # buộc rebuild để verify dependency graph
pio run -e esp32-s3-cam
```

Mong đợi:
- Build success, không warning critical
- Binary size: < 1.5MB (đủ fit trong app slot 3MB)
- RAM usage: < 25% internal (do PSRAM hold camera + audio buffers)

### A.2 Verify task list

Flash + monitor. Sau boot, log nên có **tất cả** init lines:
```
I (xxx) main: PSRAM initialized: 8 MB
I (xxx) smoke: smoke test start
I (xxx) servo: init OK ...
I (xxx) ptz: task started ...
I (xxx) i2c_bus: init OK ...
I (xxx) mpu6050: init OK ...
I (xxx) hcsr04: init OK ...
I (xxx) sensors: task started ...
I (xxx) motor: init OK ...
I (xxx) nav: task started ...
I (xxx) audio: init OK ...
I (xxx) task_audio: task started ...
I (xxx) sim800: uart installed ...
I (xxx) sos: powering on SIM800L ...
I (xxx) battery: init OK ...
I (xxx) ir_dock: init OK ...
I (xxx) dock: IDLE -> IDLE
I (xxx) ssd1306: init OK ...
I (xxx) oled: task started ...
I (xxx) behavior: IDLE -> IDLE
I (xxx) mqtt: no broker_uri — skipping  (nếu chưa config)
I (xxx) smoke: HTTP server up ...
I (xxx) smoke: smoke test ready
```

Nếu task nào im lặng → check `firmware/main/CMakeLists.txt` REQUIRES + task spawn order trong smoke_test.c.

---

## Phase 10.B — OLED visual check (1 phút)

OLED phải hiện ngay sau boot:
```
Elderly Bot     5s
wifi:192.168.1.123
batt:11.85V  68%
state:IDLE
dock:IDLE
sim:wake → rdy
F 45  B 200 L 80 R 120
tilt:1.3
```

- [ ] Title + uptime tăng
- [ ] WiFi IP hợp lệ (không "-")
- [ ] Battery voltage gần khớp với DMM (sai số < 0.2V)
- [ ] State = IDLE (behavior chưa được command gì)
- [ ] SIM800 chuyển từ "wake" → "rdy" sau 10-30s

Nếu OLED không sáng:
- I2C bus busy? — verify MPU6050 vẫn đọc OK
- I2C địa chỉ sai? — `i2c_bus_probe(0x3C)` trong log
- Power 3.3V có dư? — OLED kéo 20mA peak

---

## Phase 10.C — Concurrency stress (15 phút)

Mục đích: verify các task không bị starve khi chạy đồng thời.

### C.1 Simultaneous loads

Trong 5 phút liên tục:
- Camera stream mở trên browser (Phase 3)
- PTZ pan-tilt quay mỗi 3s (Phase 4)
- Drive joystick kéo random (Phase 5)
- Audio loopback 3s mỗi 10s (Phase 7)

Bash script chạy song song trên máy host:
```bash
#!/bin/bash
while true; do curl -s "http://<ip>/ptz/pan?angle=$((RANDOM%160+10))"; sleep 3; done &
while true; do curl -s "http://<ip>/drive/velocity?linear=$((RANDOM%40-20))&angular=$((RANDOM%60-30))"; sleep 1; done &
while true; do curl -s "http://<ip>/audio/loopback?ms=3000"; sleep 10; done &
```

Quan sát:
- [ ] Camera stream không drop frame nghiêm trọng (FPS > 10)
- [ ] PTZ servo smooth, không jerk
- [ ] Robot drive responsive (latency < 300ms)
- [ ] OLED tiếp tục refresh không freeze
- [ ] Audio loopback complete được mỗi 10s
- [ ] CPU không sticky 100% (log không panic, không TWDT)

### C.2 Memory leak check

Lấy `/status` lúc bắt đầu và sau 5 phút stress:
```bash
curl -s http://<ip>/status | jq '.free_heap, .min_free_heap'
```

- [ ] `free_heap` cuối ~ giống `free_heap` đầu (± 5KB ok)
- [ ] `min_free_heap` không giảm dần → không leak

---

## Phase 10.D — Behavior FSM transitions (15 phút)

### D.1 PATROL mode

- [ ] Bấm "Patrol" trong web UI panel "Behavior"
- [ ] OLED hiện `state:PATROL`
- [ ] Robot bắt đầu di chuyển random forward + rotate
- [ ] Khi gặp tường → obstacle gate brake, robot rotate đi hướng khác
- [ ] Camera stream tiếp tục
- [ ] Để chạy 5 phút → robot không va đập nặng nề

### D.2 Battery low → auto-dock

Manual force battery low để test (không cần đợi pin yếu thật):

Cách 1: Edit `VBAT_LOW_V` trong battery.c thành 12.0V tạm thời, rebuild. Pin 11.5V sẽ trigger low.

Hoặc cách 2: trong PATROL, đợi pin tự xuống dưới 20%.

- [ ] Battery low → log:
  ```
  W (xxx) behavior: battery low mid-patrol — returning home
  ```
- [ ] Behavior state → RETURN_HOME
- [ ] task_dock activate SEARCH → APPROACH → CONTACT → CHARGING
- [ ] Behavior state → DOCKED
- [ ] OLED hiện state:DOCKED dock:CHARGING

### D.3 Fall preemption

⚠️ **Đặt đệm xốp** dưới robot.

- [ ] Robot đang PATROL
- [ ] Cầm robot lên + thả → fall detected
- [ ] Log:
  ```
  W (xxx) sensors: spike X.XXg — fall candidate
  E (xxx) sensors: FALL DETECTED ...
  W (xxx) sos: trigger source: FALL
  I (xxx) behavior: PATROL -> SOS_ACTIVE
  ```
- [ ] Motor brake ngay lập tức
- [ ] SIM800L gửi SMS + dial
- [ ] OLED hiện state:SOS_ACTIVE + *FALL*

### D.4 Recovery

- [ ] Dựng robot lên đứng phẳng
- [ ] Đợi 10s upright + 30s cooldown
- [ ] Log:
  ```
  I (xxx) sensors: fall cleared — robot upright
  I (xxx) behavior: fall cleared, resuming PATROL
  ```
- [ ] Behavior trở về PATROL

### D.5 Leave dock

- [ ] Behavior state = DOCKED
- [ ] Bấm "Leave dock" trong UI
- [ ] Robot drive forward 1.5s, tách dock
- [ ] Behavior state → IDLE

---

## Phase 10.E — MQTT (optional)

Nếu có broker Mosquitto chạy local hoặc HomeAssistant:

### E.1 Config broker

Set URI qua endpoint custom (chưa có Phase 10 — dùng nvs partition tool):

```bash
# Trên ESP32 console (BOOT button + reset to enter download), hoặc qua serial:
# Set NVS mqtt/broker_uri = "mqtt://192.168.1.100:1883"
```

Hoặc: thêm endpoint `/mqtt/config?uri=...` (TODO Phase 11). Phase 10 ship scaffold only.

### E.2 Verify publish

Subscribe topic trên broker:
```bash
mosquitto_sub -h 192.168.1.100 -t 'elderly_robot/#'
```

Mong đợi (Phase 12 mới publish active; Phase 10 chỉ có connect + subscribe):
- [ ] `elderly_robot/state` (JSON state, 1Hz) — Phase 12
- [ ] `elderly_robot/event/fall` "1" khi té — Phase 12

Phase 10: verify connect log thôi:
```
I (xxx) mqtt: connecting to mqtt://...
I (xxx) mqtt: connected to broker
```

---

## Phase 10.F — 1-hour soak test

Để robot bật 1 giờ với behavior IDLE.

- [ ] OLED không freeze
- [ ] Battery voltage tự giảm dần (~0.1V/giờ idle)
- [ ] WiFi không disconnect
- [ ] Free heap không giảm dần
- [ ] Tất cả task vẫn responsive (mỗi 10 phút bấm vài button trong UI)

Sau 1 giờ:
- [ ] Robot không reboot
- [ ] Log không có panic / TWDT / brownout

---

## Phase 10.G — Final checklist

Tất cả phải pass:

- [ ] 9 task spawned (log đầy đủ init)
- [ ] OLED hiển thị status đúng
- [ ] Concurrent stream + drive + PTZ + audio 5 phút không lag
- [ ] Memory không leak sau 5 phút stress
- [ ] PATROL ↔ IDLE manual transition
- [ ] Fall → SOS_ACTIVE preemption
- [ ] Recovery sau 30s cooldown
- [ ] Auto-dock khi battery low
- [ ] Leave dock manual
- [ ] 1-hour soak idle stable

→ **Tiếp theo**: Phase 11 — formal test cases, 24h soak, edge cases (WiFi flap, motor stall, dock fail mid-approach).
